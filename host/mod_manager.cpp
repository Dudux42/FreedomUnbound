#include "mod_manager.hpp"
#include "mod_overlay.hpp"

#include "gpu/texture_pack_import.hpp"
#include "install/user_data.hpp"
#include "profile.hpp"
#include "settings/settings.hpp"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <system_error>

namespace portablekit::fu::mods {
namespace {
namespace fs = std::filesystem;

// Types this build can use, and the ones it knows but refuses for now.
const std::set<std::string> kSupportedTypes = {"texture", "text", "overlay", "camera", "qol"};
const std::set<std::string> kKnownTypes = {"texture", "text", "camera", "qol", "overlay", "audio"};
// Hooks a mod may name in "requires" that this build has.
const std::set<std::string> kBuildHooks = {"texture_pack", "text_hook", "font"};
bool native_type(const std::string &type) { return type == "overlay" || type == "camera" || type == "qol"; }

std::string utf8(const fs::path &p) { return install::path_to_utf8(p); }

std::string lower(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// "textures" (older packs) and "texture" are the same type.
std::string normal_type(std::string type) {
    type = lower(type);
    if (type == "textures") type = "texture";
    return type;
}

// A folder-safe id: letters, digits, '-', '_' and '.'.
std::string safe_id(const std::string &text) {
    std::string out;
    for (const char c : text) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || c == '-' || c == '_' || c == '.') out += static_cast<char>(std::tolower(u));
        else if (c == ' ' && !out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && (out.back() == '-' || out.back() == '.')) out.pop_back();
    while (!out.empty() && out.front() == '.') out.erase(out.begin());
    return out.substr(0, 64);
}

// Where a key's value starts in a flat JSON object; npos when it isn't there.
std::size_t json_value(const std::string &json, const char *key) {
    const std::string needle = std::string("\"") + key + "\"";
    std::size_t at = json.find(needle);
    if (at == std::string::npos) return at;
    at = json.find(':', at + needle.size());
    if (at == std::string::npos) return at;
    return json.find_first_not_of(" \t\r\n", at + 1);
}

// A JSON string starting at json[at] == '"'; sets end past the closing quote.
std::string json_string_at(const std::string &json, std::size_t at, std::size_t &end) {
    std::string out;
    for (std::size_t i = at + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '"') { end = i + 1; return out; }
        if (c == '\\' && i + 1 < json.size()) {
            c = json[++i];
            if (c == 'n') out += '\n';
            else if (c == 't') out += ' ';
            else if (c == 'u') { i += 4; out += '?'; }
            else out += c;
        } else {
            out += c;
        }
    }
    end = json.size();
    return {};
}

// The string values of a flat JSON object; enough for mod.json.
std::string json_string(const std::string &json, const char *key) {
    const std::size_t at = json_value(json, key);
    if (at == std::string::npos || json[at] != '"') return {};
    std::size_t end = 0;
    return json_string_at(json, at, end);
}

bool json_has(const std::string &json, const char *key) { return json_value(json, key) != std::string::npos; }

int json_int(const std::string &json, const char *key, int fallback) {
    std::size_t at = json_value(json, key);
    if (at == std::string::npos) return fallback;
    if (json[at] == '"') ++at;  // "1" is taken as 1
    const char *start = json.c_str() + at;
    char *stop = nullptr;
    const long value = std::strtol(start, &stop, 10);
    return stop == start ? fallback : static_cast<int>(value);
}

bool json_bool(const std::string &json, const char *key) {
    const std::size_t at = json_value(json, key);
    return at != std::string::npos && json.compare(at, 4, "true") == 0;
}

// A list of strings; a single string is taken as a list of one.
std::vector<std::string> json_strings(const std::string &json, const char *key) {
    std::vector<std::string> out;
    std::size_t at = json_value(json, key);
    if (at == std::string::npos) return out;
    std::size_t end = 0;
    if (json[at] == '"') {
        if (std::string one = json_string_at(json, at, end); !one.empty()) out.push_back(lower(one));
        return out;
    }
    if (json[at] != '[') return out;
    for (std::size_t i = at + 1; i < json.size() && json[i] != ']';) {
        if (json[i] == '"') {
            if (std::string one = json_string_at(json, i, end); !one.empty()) out.push_back(lower(one));
            i = end;
        } else {
            ++i;
        }
    }
    return out;
}

void parse_manifest(const std::string &json, ModInfo &info) {
    info.has_manifest = true;
    info.id = json_string(json, "id");
    info.name = json_string(json, "name");
    info.version = json_string(json, "version");
    info.author = json_string(json, "author");
    info.description = json_string(json, "description");
    info.type = normal_type(json_string(json, "type"));
    info.game_id = json_string(json, "game_id");
    info.language = json_string(json, "language");
    info.required_hooks = json_strings(json, "requires");
    info.conflicts = json_strings(json, "conflicts");
    info.has_api_version = json_has(json, "api_version");
    info.api_version = json_int(json, "api_version", kApiVersion);
    info.affects_save = json_bool(json, "affects_save");
    info.load_order = json_int(json, "load_order", 0);
}

// Why this build can't use a mod with this manifest; empty when it can.
// Older texture packs are let off the newer required fields.
std::string manifest_problem(const ModInfo &info) {
    if (info.type.empty()) return {};
    if (kKnownTypes.count(info.type) == 0)
        return "\"" + info.type + "\" isn't a mod type the launcher knows.";
    if (kSupportedTypes.count(info.type) == 0)
        return type_label(info.type) + " mods aren't supported in this build.";
    if (native_type(info.type) && !mod_overlay::supports(info.id))
        return "This mod needs a native provider that isn't included in this build: " + info.id + ".";
    if (info.api_version > kApiVersion)
        return "This mod needs a newer launcher (mod API " + std::to_string(info.api_version) + "; this build has " +
               std::to_string(kApiVersion) + ").";
    for (const std::string &hook : info.required_hooks)
        if (kBuildHooks.count(hook) == 0 &&
            !(native_type(info.type) && mod_overlay::has_hook(info.id, hook)))
            return "This mod needs \"" + hook + "\", which this build doesn't have.";
    if (info.type != "texture") {
        std::string missing;
        const auto need = [&](const std::string &value, const char *field) {
            if (value.empty()) missing += missing.empty() ? field : std::string(", ") + field;
        };
        need(info.has_manifest ? info.id : std::string(), "id");
        need(info.name, "name");
        need(info.version, "version");
        need(info.author, "author");
        if (!info.has_api_version) missing += missing.empty() ? "api_version" : ", api_version";
        if (!missing.empty()) return "Its mod.json is missing " + missing + ".";
    }
    if (info.type == "text" && info.language.empty())
        return "This text mod's mod.json doesn't say its language (for example \"language\": \"pt-BR\").";
    if (!info.game_id.empty() && lower(info.game_id) != lower(portablekit::game().disc_id))
        return "This mod is made for " + info.game_id + ", not " + portablekit::game().disc_id + ".";
    return {};
}

struct Zip {
    mz_zip_archive archive{};
    std::FILE *file{};
    bool open{};
    explicit Zip(const fs::path &path) {
#if defined(_WIN32)
        file = _wfopen(path.c_str(), L"rb");
#else
        file = std::fopen(path.c_str(), "rb");
#endif
        if (file == nullptr) return;
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        open = !ec && mz_zip_reader_init_cfile(&archive, file, size, 0);
    }
    ~Zip() {
        if (open) mz_zip_reader_end(&archive);
        if (file != nullptr) std::fclose(file);
    }
    Zip(const Zip &) = delete;
    Zip &operator=(const Zip &) = delete;
};

// An entry name that stays inside the folder it is unpacked into.
bool safe_entry(const std::string &name) {
    if (name.empty() || name.front() == '/' || name.front() == '\\' || name.find(':') != std::string::npos) return false;
    std::size_t start = 0;
    while (start <= name.size()) {
        const std::size_t end = name.find_first_of("/\\", start);
        const std::string part = name.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

// The folder inside the zip that is the mod's top: where mod.json is, or
// the one folder everything is in.
std::string zip_root(Zip &zip, std::string &manifest) {
    const mz_uint count = mz_zip_reader_get_num_files(&zip.archive);
    std::string best;
    bool found = false;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip.archive, i, &st)) continue;
        const std::string name = st.m_filename;
        const std::string base = name.substr(name.find_last_of('/') + 1);
        if (lower(base) != "mod.json") continue;
        const std::string dir = name.substr(0, name.size() - base.size());
        if (!found || dir.size() < best.size()) {
            best = dir;
            found = true;
            std::size_t size = 0;
            if (void *data = mz_zip_reader_extract_to_heap(&zip.archive, i, &size, 0)) {
                manifest.assign(static_cast<const char *>(data), size);
                mz_free(data);
            }
        }
    }
    return best;
}

fs::path enabled_file() { return mods_root() / "enabled.txt"; }

// The texture pack inside an unpacked mod: the importer's usual layouts, or
// else the shallowest folder holding a textures.ini (e.g. textures/).
gpu::TexturePackCheck find_pack(const fs::path &folder) {
    gpu::TexturePackCheck check = gpu::check_texture_pack(folder, portablekit::game().disc_id);
    if (check.found()) return check;
    std::error_code ec;
    fs::path best;
    int best_depth = 1 << 30;
    for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
        if (it.depth() > 4) { it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(ec) || lower(utf8(it->path().filename())) != "textures.ini") continue;
        if (it.depth() < best_depth) { best_depth = it.depth(); best = it->path().parent_path(); }
    }
    return best.empty() ? check : gpu::check_texture_pack(best, portablekit::game().disc_id);
}

// Why an unpacked text mod can't be used; empty when it can.
std::string text_problem(const fs::path &folder) {
    // A text\ folder with at least one file (host.txt alone is enough). A
    // font\ folder on its own isn't a text mod.
    std::error_code ec;
    bool has_file = false;
    for (fs::directory_iterator it(folder / "text", ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec)) { has_file = true; break; }
    if (has_file) return {};
    if (fs::is_directory(folder / "font", ec))
        return "This text mod only has a font\\ folder. Text mods need a text\\ folder with at least one file "
               "(a text\\host.txt on its own is enough).";
    return "This text mod has no files in a text\\ folder.";
}

// enabled.txt, in load order. Ids listed twice keep their first place.
std::vector<std::string> read_enabled() {
    std::vector<std::string> ids;
    std::ifstream in(enabled_file());
    for (std::string line; std::getline(in, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty() && std::find(ids.begin(), ids.end(), line) == ids.end()) ids.push_back(line);
    }
    return ids;
}

bool write_enabled(const std::vector<std::string> &ids) {
    std::error_code ec;
    fs::create_directories(mods_root(), ec);
    std::ofstream out(enabled_file(), std::ios::trunc);
    for (const std::string &id : ids) out << id << "\n";
    return static_cast<bool>(out);
}

void erase_id(std::vector<std::string> &ids, const std::string &id) {
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
}

ModInfo read_info(const fs::path &folder) {
    ModInfo info;
    std::ifstream in(folder / "mod.json", std::ios::binary);
    if (in) {
        const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        parse_manifest(json, info);
    }
    const std::string folder_name = utf8(folder.filename());
    if (info.id.empty()) info.id = folder_name;
    if (info.name.empty()) info.name = folder_name;
    std::error_code ec;
    // Older packs: no type, or the marker the launcher used to write.
    if (info.type.empty() && fs::exists(folder / ".type-textures", ec)) info.type = "texture";
    return info;
}

int type_rank(const std::string &type) {
    static const char *const order[] = {"texture", "text", "camera", "qol", "overlay", "audio"};
    for (int i = 0; i < 6; ++i)
        if (type == order[i]) return i;
    return 6;
}

bool same_language(const std::string &a, const std::string &b) { return lower(a) == lower(b); }
} // namespace

fs::path mods_root() { return install::user_data_directory() / "mods"; }

std::string type_label(const std::string &type) {
    if (type == "texture") return "Texture";
    if (type == "text") return "Text";
    if (type == "camera") return "Camera";
    if (type == "qol") return "Quality-of-life";
    if (type == "overlay") return "Overlay";
    if (type == "audio") return "Audio";
    return type.empty() ? "Unknown" : type;
}

ModPreview inspect(const fs::path &dropped) {
    ModPreview p;
    p.zip = dropped;
    const std::string file_name = utf8(dropped.filename());
    std::error_code ec;
    if (fs::is_directory(dropped, ec)) {
        p.problem = file_name + " is a folder. Mods install from a .zip file: zip the folder and drop the .zip.";
        return p;
    }
    if (lower(utf8(dropped.extension())) != ".zip") {
        p.problem = file_name + " is not a .zip file. Mods install from a .zip file.";
        return p;
    }
    Zip zip(dropped);
    if (!zip.open) {
        p.problem = file_name + " could not be opened as a zip. It may be damaged or still downloading.";
        return p;
    }
    std::string manifest;
    const std::string root = zip_root(zip, manifest);
    if (!manifest.empty()) parse_manifest(manifest, p.info);
    const std::string manifest_id = p.info.id;
    const std::string stem = utf8(dropped.stem());
    if (p.info.name.empty()) p.info.name = stem;
    p.info.id = safe_id(p.info.id.empty() ? stem : p.info.id);
    bool has_textures_ini = false, has_text = false, has_font = false;
    const mz_uint count = mz_zip_reader_get_num_files(&zip.archive);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip.archive, i, &st)) continue;
        const std::string name = st.m_filename;
        if (!safe_entry(name)) {
            p.problem = "This zip has a file that would land outside the mod's folder (" + name + "), so it was refused.";
            return p;
        }
        if (mz_zip_reader_is_file_a_directory(&zip.archive, i)) continue;
        ++p.files;
        p.bytes += st.m_uncomp_size;
        if (lower(name.substr(name.find_last_of('/') + 1)) == "textures.ini") has_textures_ini = true;
        if (name.compare(0, root.size(), root) == 0) {
            const std::string inside = lower(name.substr(root.size()));
            if (inside.rfind("text/", 0) == 0 && inside.size() > 5) has_text = true;
            if (inside.rfind("font/", 0) == 0) has_font = true;
        }
    }
    if (p.info.type.empty() && has_textures_ini) p.info.type = "texture";
    ModInfo checked = p.info;
    checked.id = manifest_id;
    if (p.info.id.empty()) p.problem = "This mod has no usable id or file name.";
    else if (p.files == 0) p.problem = "This zip is empty.";
    else if (!p.info.game_id.empty() && lower(p.info.game_id) != lower(portablekit::game().disc_id))
        p.problem = "This mod is made for " + p.info.game_id + ", not " + portablekit::game().disc_id + ".";
    else if (p.info.type.empty())
        p.problem = "This zip has no mod.json and no textures.ini, so the launcher can't tell what kind of mod it is.";
    else if (std::string why = manifest_problem(checked); !why.empty())
        p.problem = why;
    else if (p.info.type == "texture" && !has_textures_ini)
        p.problem = "This texture mod has no textures.ini inside.";
    else if (p.info.type == "text" && !has_text)
        p.problem = has_font ? "This text mod only has a font\\ folder. Text mods need a text\\ folder with at least "
                               "one file next to their mod.json (a text\\host.txt on its own is enough)."
                             : "This text mod has no files in a text\\ folder next to its mod.json.";
    if (const fs::path existing = mods_root() / install::path_from_utf8(p.info.id); fs::is_directory(existing, ec)) {
        p.replaces = true;
        p.installed_version = read_info(existing).version;
    }
    return p;
}

bool install(const ModPreview &preview, InstallProgress &progress, std::string &error) {
    Zip zip(preview.zip);
    if (!zip.open) {
        error = "The zip could not be opened.";
        return false;
    }
    std::string manifest;
    const std::string root = zip_root(zip, manifest);
    const fs::path root_dir = mods_root();
    const fs::path target = root_dir / install::path_from_utf8(preview.info.id);
    const fs::path staging = root_dir / install::path_from_utf8(".incoming-" + preview.info.id);
    std::error_code ec;
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);
    if (ec) {
        error = "Could not create " + utf8(staging) + ": " + ec.message();
        return false;
    }
    progress.total = preview.bytes;
    const mz_uint count = mz_zip_reader_get_num_files(&zip.archive);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip.archive, i, &st)) continue;
        std::string name = st.m_filename;
        if (!safe_entry(name)) continue;
        if (!root.empty()) {
            if (name.compare(0, root.size(), root) != 0) continue;
            name = name.substr(root.size());
            if (name.empty()) continue;
        }
        const fs::path out = staging / install::path_from_utf8(name);
        if (mz_zip_reader_is_file_a_directory(&zip.archive, i)) {
            fs::create_directories(out, ec);
            continue;
        }
        fs::create_directories(out.parent_path(), ec);
        std::ofstream file(out, std::ios::binary | std::ios::trunc);
        const auto write = [](void *opaque, mz_uint64, const void *buf, size_t n) -> size_t {
            auto *f = static_cast<std::ofstream *>(opaque);
            f->write(static_cast<const char *>(buf), static_cast<std::streamsize>(n));
            return *f ? n : 0;
        };
        if (!file || !mz_zip_reader_extract_to_callback(&zip.archive, i, write, &file, 0)) {
            error = "Could not unpack " + name + ". The disk may be full or the zip damaged.";
            file.close();
            fs::remove_all(staging, ec);
            return false;
        }
        progress.bytes += st.m_uncomp_size;
    }
    if (preview.info.type == "texture") {
        const gpu::TexturePackCheck check = find_pack(staging);
        if (!check.ok()) {
            error = check.found() ? "The texture pack inside doesn't load: " + check.problem
                                  : "No texture pack was found inside the zip.";
            fs::remove_all(staging, ec);
            return false;
        }
        std::ofstream(staging / ".type-textures") << "\n";
    } else if (preview.info.type == "text") {
        if (const std::string why = text_problem(staging); !why.empty()) {
            error = why;
            fs::remove_all(staging, ec);
            return false;
        }
    }
    if (!fs::exists(staging / "mod.json", ec)) {
        // Keep what the pop-up showed for a zip without one.
        std::ofstream json(staging / "mod.json");
        json << "{\"id\": \"" << preview.info.id << "\", \"name\": \"" << preview.info.name << "\", \"type\": \""
             << preview.info.type << "\", \"api_version\": " << kApiVersion << "}\n";
    }
    // A texture mod in use is read from its folder; let go of it first.
    settings::Settings &settings = settings::current();
    const bool was_active = !settings.texture_pack_folder.empty() &&
                            install::path_from_utf8(settings.texture_pack_folder).string().rfind(target.string(), 0) == 0;
    fs::remove_all(target, ec);
    if (ec) {
        error = "Could not replace the installed copy: " + ec.message();
        fs::remove_all(staging, ec);
        return false;
    }
    fs::rename(staging, target, ec);
    if (ec) {
        error = "Could not put the mod in place: " + ec.message();
        return false;
    }
    std::cout << "[mods] installed " << preview.info.id << " " << preview.info.version << " into " << utf8(target)
              << "\n";
    if (was_active) {
        InstalledMod mod;
        mod.info = read_info(target);
        mod.folder = target;
        std::string ignored;
        set_enabled(mod, true, ignored);
    }
    // An overlay replacement is picked up on the next launch. Installation
    // can run on a worker thread; provider callbacks belong to the UI thread.
    // Enabled text/overlay mods keep their ids in enabled.txt.
    return true;
}

std::vector<InstalledMod> installed() {
    std::vector<InstalledMod> list;
    const std::vector<std::string> on = read_enabled();
    std::error_code ec;
    for (fs::directory_iterator it(mods_root(), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        const std::string name = utf8(it->path().filename());
        if (name.empty() || name.front() == '.') continue;
        InstalledMod mod;
        mod.folder = it->path();
        mod.info = read_info(mod.folder);
        mod.info.id = name;
        if (const auto at = std::find(on.begin(), on.end(), name); at != on.end()) {
            mod.enabled = true;
            mod.position = static_cast<int>(at - on.begin());
        }
        if (mod.info.type.empty()) {
            mod.problem = "The launcher can't tell what kind of mod this is.";
        } else if (std::string why = manifest_problem(mod.info); !why.empty()) {
            mod.problem = why;
        } else if (mod.info.type == "texture") {
            const gpu::TexturePackCheck check = find_pack(mod.folder);
            if (check.ok()) mod.content = check.folder;
            else mod.problem = check.found() ? check.problem : "No texture pack found in it.";
        } else if (mod.info.type == "text") {
            mod.problem = text_problem(mod.folder);
            if (mod.problem.empty()) mod.content = mod.folder / "text";
        }
        list.push_back(std::move(mod));
    }
    // Conflicts: warn on both sides whenever both mods are on.
    for (InstalledMod &mod : list) {
        if (!mod.enabled) continue;
        for (const InstalledMod &other : list) {
            if (!other.enabled || &other == &mod) continue;
            const bool listed =
                std::find(mod.info.conflicts.begin(), mod.info.conflicts.end(), lower(other.info.id)) !=
                    mod.info.conflicts.end() ||
                std::find(other.info.conflicts.begin(), other.info.conflicts.end(), lower(mod.info.id)) !=
                    other.info.conflicts.end();
            if (listed) mod.warnings.push_back("Conflicts with " + other.info.name + ", which is also on.");
        }
        if (mod.info.affects_save)
            mod.warnings.push_back("Changes your save. Your save is backed up before the game starts with it.");
    }
    std::sort(list.begin(), list.end(), [](const InstalledMod &a, const InstalledMod &b) {
        const int ta = type_rank(a.info.type), tb = type_rank(b.info.type);
        if (ta != tb) return ta < tb;
        if (a.info.type != b.info.type) return a.info.type < b.info.type;
        if (a.enabled != b.enabled) return a.enabled;
        if (a.enabled) return a.position < b.position;
        return lower(a.info.name) < lower(b.info.name);
    });
    return list;
}

std::vector<InstalledMod> enabled() {
    std::vector<InstalledMod> list;
    for (InstalledMod &mod : installed())
        if (mod.enabled && mod.problem.empty()) list.push_back(std::move(mod));
    std::sort(list.begin(), list.end(),
              [](const InstalledMod &a, const InstalledMod &b) { return a.position < b.position; });
    return list;
}

std::vector<TextMod> enabled_text_mods() {
    std::vector<TextMod> list;
    std::set<std::string> languages;
    for (const InstalledMod &mod : enabled()) {
        if (mod.info.type != "text" || !languages.insert(lower(mod.info.language)).second) continue;
        TextMod text;
        text.id = mod.info.id;
        text.language = mod.info.language;
        text.folder = mod.folder;
        text.text = mod.folder / "text";
        text.font = mod.folder / "font";
        text.host = text.text / "host.txt";
        list.push_back(std::move(text));
    }
    return list;
}

void restore_overlays() {
    for (const auto &mod : installed()) {
        if (!native_type(mod.info.type)) continue;
        if (mod.enabled && mod.problem.empty()) mod_overlay::activate(mod.info.id, mod.folder);
        else mod_overlay::deactivate(mod.info.id);
    }
}

bool set_enabled(const InstalledMod &mod, bool on, std::string &error, std::string *notice) {
    std::vector<std::string> ids = read_enabled();
    settings::Settings &settings = settings::current();
    std::vector<std::string> switched_off;
    bool redraw = false;
    if (on) {
        if (std::string why = manifest_problem(mod.info); !why.empty()) {
            error = why;
            return false;
        }
        if (mod.info.type == "texture") {
            const gpu::TexturePackCheck check = find_pack(mod.folder);
            if (!check.ok()) {
                error = check.found() ? check.problem : "No texture pack found in it.";
                return false;
            }
            // One texture pack at a time: switching one on switches the others off.
            for (const InstalledMod &other : installed())
                if (other.info.type == "texture" && other.info.id != mod.info.id && other.enabled) {
                    erase_id(ids, other.info.id);
                    switched_off.push_back(other.info.name);
                }
            settings.texture_pack_folder = utf8(check.folder);
            settings.texture_pack = true;
            redraw = true;
        } else if (mod.info.type == "text") {
            if (std::string why = text_problem(mod.folder); !why.empty()) {
                error = why;
                return false;
            }
            // One text mod per language.
            for (const InstalledMod &other : installed())
                if (other.info.type == "text" && other.info.id != mod.info.id && other.enabled &&
                    same_language(other.info.language, mod.info.language)) {
                    erase_id(ids, other.info.id);
                    switched_off.push_back(other.info.name);
                }
            std::error_code ec;
            redraw = fs::is_directory(mod.folder / "font", ec);
        } else if (native_type(mod.info.type)) {
            // Validated native provider; activation follows successful persistence.
        } else {
            error = "This mod type isn't supported in this build.";
            return false;
        }
        if (std::find(ids.begin(), ids.end(), mod.info.id) == ids.end()) {
            // Goes after every enabled mod with a load_order no higher than its own.
            std::size_t at = 0;
            const std::vector<InstalledMod> list = installed();
            for (std::size_t i = 0; i < ids.size(); ++i) {
                const auto found = std::find_if(list.begin(), list.end(),
                                                [&](const InstalledMod &m) { return m.info.id == ids[i]; });
                if (found == list.end() || found->info.load_order <= mod.info.load_order) at = i + 1;
            }
            ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(at), mod.info.id);
        }
    } else {
        erase_id(ids, mod.info.id);
        if (mod.info.type == "texture" && !mod.content.empty() && settings.texture_pack_folder == utf8(mod.content)) {
            settings.texture_pack_folder.clear();
            settings.texture_pack = false;
        }
        std::error_code ec;
        redraw = mod.info.type == "texture" || fs::is_directory(mod.folder / "font", ec);
    }
    settings::save();
    if (!write_enabled(ids)) {
        error = "Could not save the mod list.";
        return false;
    }
    if (native_type(mod.info.type)) {
        if (on) mod_overlay::activate(mod.info.id, mod.folder);
        else mod_overlay::deactivate(mod.info.id);
    }
    if (notice != nullptr) {
        notice->clear();
        for (const std::string &name : switched_off)
            *notice += (notice->empty() ? std::string() : " ") + name + " was switched off: only one " +
                       (mod.info.type == "text" ? mod.info.language + " text mod" : std::string("texture mod")) +
                       " can be on at a time.";
        if (on && mod.info.affects_save)
            *notice += (notice->empty() ? std::string() : " ") +
                       mod.info.name + " changes your save. It will be backed up before the game starts with it.";
    }
    return redraw;
}

bool move(const InstalledMod &mod, int delta, std::string &error) {
    std::vector<std::string> ids = read_enabled();
    const auto at = std::find(ids.begin(), ids.end(), mod.info.id);
    if (at == ids.end()) {
        error = "Only mods that are on have a load order.";
        return false;
    }
    const std::ptrdiff_t from = at - ids.begin();
    const std::ptrdiff_t to = from + (delta < 0 ? -1 : 1);
    if (to < 0 || to >= static_cast<std::ptrdiff_t>(ids.size())) return false;
    std::swap(ids[static_cast<std::size_t>(from)], ids[static_cast<std::size_t>(to)]);
    if (!write_enabled(ids)) {
        error = "Could not save the mod list.";
        return false;
    }
    return true;
}

bool remove(const InstalledMod &mod, std::string &error) {
    if (mod.enabled) {
        set_enabled(mod, false, error);
        if (!error.empty()) return false;
    }
    std::error_code ec;
    fs::remove_all(mod.folder, ec);
    if (ec) {
        error = "Could not delete " + utf8(mod.folder) + ": " + ec.message();
        return false;
    }
    std::vector<std::string> ids = read_enabled();
    erase_id(ids, mod.info.id);
    write_enabled(ids);
    return true;
}

} // namespace portablekit::fu::mods
