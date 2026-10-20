#include "mods_used.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <system_error>

namespace portablekit::fu::mods_used {
namespace {

constexpr const char *kMagic = "FUMODSUSED 1";

std::string clean(std::string s) {
    for (char &c : s)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

std::string label(const Mod &mod) {
    const std::string name = mod.name.empty() ? mod.id : mod.name;
    return mod.version.empty() ? name : name + " " + mod.version;
}

} // namespace

std::string format(const std::vector<Mod> &mods) {
    std::string out = "# Freedom Unbound: the mods on when the save was last written or loaded (FU Saves).\n";
    out += kMagic;
    out += '\n';
    for (const Mod &mod : mods)
        out += clean(mod.id) + '\t' + clean(mod.version) + '\t' + (mod.affects_save ? "1" : "0") + '\t' +
               clean(mod.name) + '\n';
    return out;
}

std::optional<std::vector<Mod>> parse(const std::string &text) {
    std::istringstream in(text);
    std::string line;
    bool magic = false;
    std::vector<Mod> mods;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        if (!magic) {
            if (line != kMagic) return std::nullopt;
            magic = true;
            continue;
        }
        std::vector<std::string> fields;
        std::size_t start = 0;
        for (std::size_t tab; (tab = line.find('\t', start)) != std::string::npos; start = tab + 1)
            fields.push_back(line.substr(start, tab - start));
        fields.push_back(line.substr(start));
        if (fields.size() < 3 || fields[0].empty() || (fields[2] != "0" && fields[2] != "1")) return std::nullopt;
        mods.push_back({fields[0], fields[1], fields.size() > 3 ? fields[3] : std::string(), fields[2] == "1"});
    }
    if (!magic) return std::nullopt;
    return mods;
}

std::filesystem::path file_for(const std::filesystem::path &memory_stick) {
    auto stick = memory_stick.lexically_normal();
    if (!stick.has_filename()) stick = stick.parent_path();
    return stick.parent_path() / "mods-used.txt";
}

std::optional<std::vector<Mod>> read(const std::filesystem::path &file) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size > 256 * 1024) return std::nullopt;
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    return parse(std::string(std::istreambuf_iterator<char>(in), {}));
}

bool write(const std::filesystem::path &file, const std::vector<Mod> &mods) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    auto temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << format(mods);
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(temp, ec);
            return false;
        }
    }
    std::filesystem::rename(temp, file, ec);
    if (ec) std::filesystem::remove(temp, ec);
    return !ec;
}

Difference compare(const std::optional<std::vector<Mod>> &recorded, const std::vector<Mod> &current) {
    const auto key = [](const Mod &mod) { return mod.id + '\t' + mod.version; };
    std::set<std::string> before, now;
    if (recorded)
        for (const Mod &mod : *recorded)
            if (mod.affects_save) before.insert(key(mod));
    for (const Mod &mod : current)
        if (mod.affects_save) now.insert(key(mod));
    Difference diff;
    for (const Mod &mod : current)
        if (mod.affects_save && !before.contains(key(mod))) diff.added.push_back(label(mod));
    if (recorded)
        for (const Mod &mod : *recorded)
            if (mod.affects_save && !now.contains(key(mod))) diff.removed.push_back(label(mod));
    return diff;
}

} // namespace portablekit::fu::mods_used
