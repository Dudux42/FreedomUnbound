// Parser and string-table tests. Synthetic data only: no game data here.
#include "fu_text_table.hpp"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace portablekit::fu::text;

static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++failures; } } while (0)

static void put32(std::vector<std::uint8_t> &d, std::size_t at, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) d[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
}

// Builds a table like the game's: unaligned section offsets, -1 entries.
static std::vector<std::uint8_t> sample() {
    StringTable t;
    t.sections = {{std::string("Yes"), std::string("No")},
                  {std::string("Press the ~B01 button"), std::nullopt, std::string("NEW GAME")}};
    auto bytes = *write_table(t, 4096);
    return bytes;
}

int main() {
    // escapes
    std::string out, err;
    CHECK(decode_escapes("a\\nb\\\\c\\{d", out, err) && out == "a\nb\\c{d");
    CHECK(decode_escapes("{c:3}x{v:12}{x:41}", out, err) && out == "~C03x%12A");
    CHECK(decode_escapes("Pressione ~B01 agora", out, err) && out == "Pressione ~B01 agora");
    CHECK(!decode_escapes("bad\\q", out, err));
    CHECK(!decode_escapes("{x:4}", out, err));
    CHECK(!decode_escapes("{z:1}", out, err));
    CHECK(!decode_escapes("end\\", out, err));

    // text file
    const auto parsed = parse_text("# comment\n0.0=Sim\r\n\n0.1=N\xC3\xA3o\n1.2=NOVO JOGO\nbad line\n0.0=dup\n1=x\n");
    CHECK(parsed.lines.size() == 3);
    CHECK(parsed.lines.at({0, 1}) == "N\xC3\xA3o");
    CHECK(parsed.lines.at({0, 0}) == "Sim");
    CHECK(parsed.errors.size() == 3);  // bad line, duplicate, "1=x"
    CHECK(!parse_text("\xEF\xBB\xBF" "0.0=a").errors.empty());
    CHECK(!parse_text("0.0={x:00}").errors.empty() && parse_text("0.0={x:00}").lines.empty());

    // font map
    const auto map = parse_font_map("# PT-BR\n\xC3\xA1=U+00E1 cell=96\n\xC3\x87=U+00C7 cell=119\n\xC3\xA9=U+00E9 cell=84\nx=U+0079 cell=97\n");
    CHECK(map.cells.size() == 2 && map.cells.at(0xE1) == 96 && map.cells.at(0xC7) == 119);
    CHECK(map.errors.size() == 2);  // cell 84 reserved, char/code mismatch

    // table round trip
    auto bytes = sample();
    auto table = read_table(bytes);
    CHECK(table && table->sections.size() == 2);
    CHECK(table && table->sections[0][1] == std::optional<std::string>("No"));
    CHECK(table && !table->sections[1][1]);
    CHECK(table && *write_table(*table, bytes.size()) == bytes);

    // unaligned section start like entry 19 (0x8C99): shift section 1 by 1 byte
    {
        auto shifted = bytes;
        const std::uint32_t s1 = shifted[12] | shifted[13] << 8;
        shifted.insert(shifted.begin() + s1, 0xEE);
        put32(shifted, 12, s1 + 1);
        auto t2 = read_table(shifted);
        CHECK(t2 && t2->sections[1][2] == std::optional<std::string>("NEW GAME"));
    }

    // apply: missing index keeps English, unknown/-1 keys reported
    std::vector<Key> unknown;
    auto t = *table;
    const auto n = apply(t, {{{0, 0}, "Sim"}, {{1, 1}, "x"}, {{5, 0}, "y"}}, &unknown);
    CHECK(n == 1 && unknown.size() == 2);
    CHECK(t.sections[0][0] == std::optional<std::string>("Sim") && t.sections[0][1] == std::optional<std::string>("No"));
    auto rebuilt = write_table(t, bytes.size() + 64);
    CHECK(rebuilt && read_table(*rebuilt)->sections[0][0] == std::optional<std::string>("Sim"));

    // too big: refused
    auto big = *table;
    big.sections[0][0] = std::string(5000, 'a');
    CHECK(!write_table(big, bytes.size()));

    // malformed input
    CHECK(!read_table(std::vector<std::uint8_t>{1, 0, 0, 0}));
    auto broken = bytes;
    put32(broken, 8, 0xFFFFFF);
    CHECK(!read_table(broken));

    // glyph cache pinning against a fake guest memory
    {
        struct Fake {
            std::map<std::uint32_t, std::uint32_t> m;
            std::uint32_t load16(std::uint32_t a) { auto f = m.find(a); return f == m.end() ? 0xFFFFu : f->second; }
            void store16(std::uint32_t a, std::uint32_t v) { m[a] = v & 0xFFFFu; }
            void store8(std::uint32_t a, std::uint32_t v) { m[a] = v & 0xFFu; }
        } mem;
        using L = GlyphCacheLayout;
        const std::uint32_t obj = 0x09971400u;
        // the game had put U+3042 into cell 96 dynamically
        mem.store16(obj + L::kCodeToSlot + 0x3042u * 2u, 96u);
        mem.store16(obj + L::kSlotCode + 96u * 2u, 0x3042u);
        CHECK(pin_glyph(mem, obj, 0xE1u, 96u));
        CHECK(mem.load16(obj + L::kCodeToSlot + 0xE1u * 2u) == 96u);
        CHECK(mem.load16(obj + L::kSlotCode + 96u * 2u) == 0xE1u);
        CHECK(mem.m[obj + L::kSlotFlag + 96u] == 2u);
        CHECK(mem.load16(obj + L::kCodeToSlot + 0x3042u * 2u) == 0xFFFFu);  // evicted
        // again: idempotent, re-pins the flag
        mem.store8(obj + L::kSlotFlag + 96u, 0u);
        CHECK(pin_glyph(mem, obj, 0xE1u, 96u) && mem.m[obj + L::kSlotFlag + 96u] == 2u);
        // the game's own cells and out-of-range cells are refused
        CHECK(!pin_glyph(mem, obj, 0xE9u, 84u) && !pin_glyph(mem, obj, 0xE9u, 132u));
    }

    std::printf(failures ? "text_table_tests: %d failure(s)\n" : "text_table_tests: ok\n", failures);
    {
        // Identical strings in a section share one copy; reads back the same and is smaller.
        StringTable dup;
        dup.table_offset = 8;
        dup.sections = {{std::string("Nothing equipped."), std::string("Iron Sword"), std::string("Nothing equipped."),
                         std::nullopt, std::string("Iron Sword"), std::string("Nothing equipped.")},
                        {std::string("Nothing equipped."), std::string("dummy"), std::string("dummy")}};
        std::size_t unique_bytes = 8 + 2 * 4 + 6 * 4 + 3 * 4;
        for (const char *s : {"Nothing equipped.", "Iron Sword", "Nothing equipped.", "dummy"}) unique_bytes += std::string(s).size() + 1;
        std::size_t naive_bytes = 8 + 2 * 4 + 6 * 4 + 3 * 4;
        for (const auto &sec : dup.sections)
            for (const auto &s : sec) if (s) naive_bytes += s->size() + 1;
        const auto packed = write_table(dup, 4096);
        CHECK(packed && packed->size() == unique_bytes && packed->size() < naive_bytes);
        CHECK(!write_table(dup, unique_bytes - 1));
        const auto back = packed ? read_table(*packed) : std::nullopt;
        CHECK(back && back->sections == dup.sections);
        // Fits a buffer the unshared layout would overflow.
        CHECK(write_table(dup, naive_bytes - 1).has_value());
    }
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
