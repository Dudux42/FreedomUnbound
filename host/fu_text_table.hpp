#pragma once
// Text mods: the text\<entry>.txt / font\map.txt parsers and the DATA.BIN
// string-table reader/writer. Pure code (no runtime, no game data) so the
// unit tests can cover it. Format: build/research/TEXT_HOOK_RESEARCH.md (to move into MODDING.md).
//
// String table layout (entry 19 and the same family), confirmed from the
// game's own lookup at 0x0885148C..0x08851510:
//   +0   u32 section count N
//   +4   u32 offset of the section table (8)
//   +8   u32 section offset[N], relative to the entry start, NOT aligned
//   section: u32 string offset[M] relative to the section (read unaligned,
//            0xFFFFFFFF = no string), then NUL-terminated UTF-8 strings.
//   M is not stored; it is the lowest valid string offset / 4.
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace portablekit::fu::text {

// <section>.<string> as written in a text file, e.g. "0.12".
struct Key {
    std::uint32_t section{}, index{};
    auto operator<=>(const Key &) const = default;
};

struct ParsedText {
    std::map<Key, std::string> lines;  // decoded bytes, ready for the game
    std::vector<std::string> errors;   // "line 7: ..." (bad lines are skipped)
};

// Decodes \n \\ \{ {c:N} {v:N} {x:HH}; ~B01-style codes pass through.
bool decode_escapes(std::string_view in, std::string &out, std::string &error);
// Whole text\NNNN.txt file. UTF-8 without BOM (a BOM is tolerated and noted).
ParsedText parse_text(std::string_view file);

struct GlyphMap {
    std::map<char32_t, std::uint32_t> cells;  // code point -> atlas cell
    std::vector<std::string> errors;
};
// font\map.txt lines: <char>=U+XXXX cell=N   ('#' comments).
GlyphMap parse_font_map(std::string_view file);

struct StringTable {
    std::uint32_t table_offset{8};
    // sections[s][i]: nullopt = 0xFFFFFFFF in the game data.
    std::vector<std::vector<std::optional<std::string>>> sections;
};
std::optional<StringTable> read_table(std::span<const std::uint8_t> entry, std::string *error = nullptr);
// Lays the table out again. nullopt if it would not fit in max_bytes.
std::optional<std::vector<std::uint8_t>> write_table(const StringTable &table, std::size_t max_bytes);
// Replaces strings that have a translation. Returns how many were applied;
// keys outside the table are listed in unknown.
std::size_t apply(StringTable &table, const std::map<Key, std::string> &lines, std::vector<Key> *unknown = nullptr);

// The game's glyph cache (text object at a0 of 0x08893198), read from
// 0x08893198..0x08893330 and the order-string preload at 0x08891E0C:
//   +0x2628 + code*2   u16 slot for a glyph code, 0xFFFF = not cached
//   +0x22608 + slot*2  u16 code held by a slot (432 slots)
//   +0x22968 + slot    u8 in-use flag (the game writes 2; 0 = may be evicted)
// The 84 order-string glyphs are preloaded into slots 0..83, so slot == atlas
// cell. Glyphs not in the atlas get dynamic slots from 84 and are drawn into
// the texture by the game (the strip at y 154..175). Pinning a mod code point
// to a slot 96..131 makes the game draw that cell for it.
struct GlyphCacheLayout {
    static constexpr std::uint32_t kCodeToSlot = 0x2628u;
    static constexpr std::uint32_t kSlotCode = 0x22608u;
    static constexpr std::uint32_t kSlotFlag = 0x22968u;
    static constexpr std::uint32_t kSlots = 432u;
    static constexpr std::uint32_t kNoSlot = 0xFFFFu;
};
// Mem: load16(a), store16(a, v), store8(a, v). Returns false if refused.
template <class Mem>
bool pin_glyph(Mem &mem, std::uint32_t obj, std::uint16_t code, std::uint32_t cell) {
    using L = GlyphCacheLayout;
    if (cell < 96u || cell >= 132u || code >= 0xFFF0u) return false;
    const std::uint32_t current = mem.load16(obj + L::kCodeToSlot + code * 2u);
    if (current != cell) {
        if (current != L::kNoSlot && current < L::kSlots && mem.load16(obj + L::kSlotCode + current * 2u) == code)
            mem.store8(obj + L::kSlotFlag + current, 0u);  // free the slot it had
        // Evict whatever other code the game put in our cell.
        const std::uint32_t other = mem.load16(obj + L::kSlotCode + cell * 2u);
        if (other != code && other < 0xFFF0u && mem.load16(obj + L::kCodeToSlot + other * 2u) == cell)
            mem.store16(obj + L::kCodeToSlot + other * 2u, L::kNoSlot);
        mem.store16(obj + L::kCodeToSlot + code * 2u, static_cast<std::uint16_t>(cell));
        mem.store16(obj + L::kSlotCode + cell * 2u, code);
    }
    mem.store8(obj + L::kSlotFlag + cell, 2u);  // in use, as the game marks it
    return true;
}

} // namespace portablekit::fu::text
