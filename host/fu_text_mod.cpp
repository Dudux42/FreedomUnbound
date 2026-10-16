// See fu_text_mod.hpp and fu_text_table.hpp.
#include "fu_text_mod.hpp"
#include "fu_text_table.hpp"
#include "equipment_preview.hpp"
#include "mod_manager.hpp"

#include "psprecomp/runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

namespace portablekit::fu::text {
namespace {

constexpr std::uint32_t kLoaderObjectRequest = 0x2FBC8u;  // *(obj + this) = current request
constexpr std::uint32_t kRamStart = 0x08800000u, kRamEnd = 0x0A000000u;

struct State {
    std::mutex mu;
    // entry id -> translated lines (first enabled mod that has the entry wins)
    std::map<std::uint32_t, std::map<Key, std::string>> entries;
    std::map<char32_t, std::uint32_t> glyphs;
    // translated string -> English, for original_text
    std::unordered_map<std::string, std::string> english_for;
    std::map<std::uint32_t, StringTable> originals;  // entry id -> English table
    std::set<std::uint32_t> refused;                 // entries kept English
    std::set<std::uint32_t> unmapped_logged;
    bool loaded = false;
};
State &state() { static State s; return s; }

bool in_ram(std::uint32_t a, std::uint32_t n) { return a >= kRamStart && a < kRamEnd && n <= kRamEnd - a; }

std::string read_file(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

// text\0019.txt -> 19
bool entry_of(const std::filesystem::path &p, std::uint32_t &id) {
    const auto stem = p.stem().string();
    if (p.extension() != ".txt" || stem.size() != 4) return false;
    id = 0;
    for (char c : stem) { if (c < '0' || c > '9') return false; id = id * 10 + (c - '0'); }
    return true;
}

// MHFU_TEXT_DUMP=<dir> (research, off by default): append every string of each
// entry the game has finished loading to <dir>\textdump.tsv as
// entry<TAB>section.string<TAB>text, with \ \n \r \t escaped. Reads only the
// table the game already decoded; never writes guest memory.
const std::filesystem::path &dump_dir() {
    static const std::filesystem::path dir = [] {
        const char *v = std::getenv("MHFU_TEXT_DUMP");
        return v && *v ? std::filesystem::path(v) : std::filesystem::path();
    }();
    return dir;
}

std::string dump_escape(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

void dump_entry(std::uint32_t id, const StringTable &t) {
    static std::mutex mu;
    static std::map<std::uint32_t, unsigned> loads;
    std::lock_guard lock(mu);
    std::error_code ec;
    std::filesystem::create_directories(dump_dir(), ec);
    std::ofstream f(dump_dir() / "textdump.tsv", std::ios::binary | std::ios::app);
    if (!f) return;
    std::size_t n = 0;
    for (std::size_t s = 0; s < t.sections.size(); ++s)
        for (std::size_t i = 0; i < t.sections[s].size(); ++i)
            if (t.sections[s][i]) {
                f << id << '\t' << s << '.' << i << '\t' << dump_escape(*t.sections[s][i]) << '\n';
                ++n;
            }
    std::printf("[fu-text] dump: entry %u load %u, %zu strings\n", id, ++loads[id], n);
    std::fflush(stdout);
}
} // namespace

void load_enabled_text_mods() {
    auto &s = state();
    std::lock_guard lock(s.mu);
    if (s.loaded) return;
    s.loaded = true;
    for (const auto &mod : mods::enabled_text_mods()) {
        std::error_code ec;
        if (!mod.text.empty() && std::filesystem::is_directory(mod.text, ec)) {
            for (const auto &item : std::filesystem::directory_iterator(mod.text, ec)) {
                std::uint32_t id{};
                if (!item.is_regular_file()) continue;
                if (!entry_of(item.path(), id)) {
                    const auto name = item.path().filename().string();
                    if (name != "host.txt")
                        std::printf("[fu-text] %s: warning: text\\%s ignored; entry files are named NNNN.txt (e.g. 0019.txt)\n",
                                    mod.id.c_str(), name.c_str());
                    continue;
                }                if (s.entries.count(id)) continue;
                auto parsed = parse_text(read_file(item.path()));
                for (const auto &e : parsed.errors)
                    std::printf("[fu-text] %s %s: %s\n", mod.id.c_str(), item.path().filename().string().c_str(), e.c_str());
                std::printf("[fu-text] %s: entry %u, %zu lines\n", mod.id.c_str(), id, parsed.lines.size());
                if (parsed.lines.empty())
                    std::printf("[fu-text] %s: warning: text\\%s has no usable <section>.<string>=<text> lines; entry %u stays English\n",
                                mod.id.c_str(), item.path().filename().string().c_str(), id);                if (!parsed.lines.empty()) s.entries.emplace(id, std::move(parsed.lines));
            }
        }
        const auto map = mod.font / "map.txt";
        if (!mod.font.empty() && std::filesystem::is_regular_file(map, ec)) {
            auto parsed = parse_font_map(read_file(map));
            for (const auto &e : parsed.errors) std::printf("[fu-text] %s font\\map.txt: %s\n", mod.id.c_str(), e.c_str());
            for (const auto &[cp, cell] : parsed.cells) s.glyphs.emplace(cp, cell);
        }
    }
    std::fflush(stdout);
}

void entry_loaded(psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    auto &s = state();
    const bool dumping = !dump_dir().empty();
    const std::uint32_t obj = ctx.gpr[16];
    if (!in_ram(obj + kLoaderObjectRequest, 4)) return;
    const std::uint32_t req = mem.aot_load32(obj + kLoaderObjectRequest);
    if (!in_ram(req, 32)) return;
    if (mem.aot_load8(req + 26u) == 0u) return;  // not the last piece yet
    const std::uint32_t id = mem.aot_load16(req + 2u);
    const std::uint32_t dest = mem.aot_load32(req + 4u);
    const std::uint32_t total = mem.aot_load32(req + 12u) + mem.aot_load32(req + 8u);  // offset + size of last piece
    if (!in_ram(dest, total) || total < 8u) return;
    if(id==19 && total<=4*1024*1024) {
        std::vector<std::uint8_t> raw(total);
        for(std::uint32_t i=0;i<total;++i) raw[i]=mem.aot_load8(dest+i);
        equipment::catalogue_seen(raw,mem);
    }
    if(s.entries.empty() && !dumping) return;
    if (dumping) {
        std::vector<std::uint8_t> raw(total);
        for (std::uint32_t i = 0; i < total; ++i) raw[i] = mem.aot_load8(dest + i);
        if (auto t = read_table(raw)) dump_entry(id, *t);
        else std::printf("[fu-text] dump: entry %u is not a string table (0x%X bytes)\n", id, total);
    }
    const auto found = s.entries.find(id);
    if (found == s.entries.end()) return;

    std::lock_guard lock(s.mu);
    std::vector<std::uint8_t> bytes(total);
    for (std::uint32_t i = 0; i < total; ++i) bytes[i] = mem.aot_load8(dest + i);
    std::string error;
    auto table = read_table(bytes, &error);
    if (!table) {
        if (s.refused.insert(id).second) std::printf("[fu-text] entry %u: not a string table (%s); kept English\n", id, error.c_str());
        return;
    }
    s.originals.insert_or_assign(id, *table);  // English, kept for original_text
    const auto english = *table;
    std::vector<Key> unknown;
    const auto applied = apply(*table, found->second, &unknown);
    auto out = write_table(*table, total);
    if (!out) {
        if (s.refused.insert(id).second)
            std::printf("[fu-text] entry %u: translation is bigger than the 0x%X-byte buffer; kept English\n", id, total);
        std::fflush(stdout);
        return;
    }
    for (std::uint32_t i = 0; i < out->size(); ++i) mem.aot_store8(dest + i, (*out)[i]);
    for (std::uint32_t i = static_cast<std::uint32_t>(out->size()); i < total; ++i) mem.aot_store8(dest + i, 0u);
    for (const auto &[k, text] : found->second) {
        if (k.section < english.sections.size() && k.index < english.sections[k.section].size() &&
            english.sections[k.section][k.index])
            s.english_for.insert_or_assign(text, *english.sections[k.section][k.index]);
    }
    std::printf("[fu-text] entry %u: %zu strings translated, %zu unknown indexes, 0x%zX of 0x%X bytes\n",
                id, applied, unknown.size(), out->size(), total);
    std::fflush(stdout);
}

bool map_glyph(psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &) {
    auto &s = state();
    const char32_t code = ctx.gpr[5] & 0xFFFFu;
    if (code < 0x80u) return false;  // ASCII: the game's own table
    if (s.glyphs.count(code)) {
        // The glyph code is the code point itself; glyph_cache_lookup pins it
        // to its cell. Return as the original does (jr ra at 0x08816630).
        ctx.gpr[2] = static_cast<std::uint32_t>(code);
        return true;
    }
    std::lock_guard lock(s.mu);
    if (s.unmapped_logged.insert(code).second)
        std::printf("[fu-text] U+%04X has no glyph; shows '?'\n", static_cast<unsigned>(code));
    return false;
}

namespace {
struct GuestMem {
    const psprecomp::AotFastView &m;
    std::uint32_t load16(std::uint32_t a) const { return m.aot_load16(a); }
    void store16(std::uint32_t a, std::uint32_t v) const { m.aot_store16(a, static_cast<std::uint16_t>(v)); }
    void store8(std::uint32_t a, std::uint32_t v) const { m.aot_store8(a, static_cast<std::uint8_t>(v)); }
};
} // namespace

void glyph_cache_lookup(psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &mem) {
    auto &s = state();
    if (s.glyphs.empty()) return;
    const char32_t code = ctx.gpr[5] & 0xFFFFu;
    if (code < 0x80u) return;
    const auto g = s.glyphs.find(code);
    if (g == s.glyphs.end()) return;
    const std::uint32_t obj = ctx.gpr[4];
    if (!in_ram(obj, GlyphCacheLayout::kSlotFlag + GlyphCacheLayout::kSlots)) return;
    GuestMem guest{mem};
    if (!pin_glyph(guest, obj, static_cast<std::uint16_t>(code), g->second)) return;
    std::lock_guard lock(s.mu);
    if (s.unmapped_logged.insert(code | 0x10000u).second)
        std::printf("[fu-text] U+%04X pinned to cell %u (text object 0x%08X)\n", static_cast<unsigned>(code), g->second, obj);
}

const char *original_text(const char *shown) {
    if (shown == nullptr) return shown;
    auto &s = state();
    std::lock_guard lock(s.mu);
    // hle_utility passes a host copy of the guest string, so match by content.
    const auto found = s.english_for.find(shown);
    return found == s.english_for.end() ? shown : found->second.c_str();
}

} // namespace portablekit::fu::text
