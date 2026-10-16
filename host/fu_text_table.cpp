#include "fu_text_table.hpp"

#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <charconv>
#include <cstdio>

namespace portablekit::fu::text {
namespace {

std::string at_line(std::size_t line, const std::string &what) {
    return "line " + std::to_string(line) + ": " + what;
}

bool parse_uint(std::string_view s, std::uint32_t &out, int base = 10) {
    if (s.empty()) return false;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out, base);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

// Splits on \n; strips a UTF-8 BOM (noted by the caller).
std::vector<std::string_view> lines_of(std::string_view file, bool &had_bom) {
    had_bom = file.size() >= 3 && file.substr(0, 3) == "\xEF\xBB\xBF";
    if (had_bom) file.remove_prefix(3);
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (start <= file.size()) {
        const auto end = file.find('\n', start);
        if (end == std::string_view::npos) { out.push_back(file.substr(start)); break; }
        out.push_back(file.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

bool utf8_one(std::string_view s, char32_t &cp, std::size_t &len) {
    if (s.empty()) return false;
    const auto b = static_cast<unsigned char>(s[0]);
    if (b < 0x80) { cp = b; len = 1; return true; }
    std::size_t n = (b & 0xE0) == 0xC0 ? 2 : (b & 0xF0) == 0xE0 ? 3 : (b & 0xF8) == 0xF0 ? 4 : 0;
    if (n == 0 || s.size() < n) return false;
    cp = b & (0x7F >> n);
    for (std::size_t i = 1; i < n; ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if ((c & 0xC0) != 0x80) return false;
        cp = cp << 6 | (c & 0x3F);
    }
    len = n;
    return true;
}

std::uint32_t le32(std::span<const std::uint8_t> d, std::size_t at) {
    return d[at] | d[at + 1] << 8 | d[at + 2] << 16 | static_cast<std::uint32_t>(d[at + 3]) << 24;
}
void put32(std::vector<std::uint8_t> &d, std::size_t at, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) d[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
}

} // namespace

bool decode_escapes(std::string_view in, std::string &out, std::string &error) {
    out.clear();
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];
        if (c == '\\') {
            if (i + 1 >= in.size()) { error = "lone \\ at end"; return false; }
            const char e = in[++i];
            if (e == 'n') out += '\n';
            else if (e == '\\') out += '\\';
            else if (e == '{') out += '{';
            else { error = std::string("unknown escape \\") + e; return false; }
        } else if (c == '{') {
            const auto close = in.find('}', i);
            if (close == std::string_view::npos || close < i + 3 || in[i + 2] != ':') {
                error = "bad {..} code"; return false;
            }
            const char kind = in[i + 1];
            const auto arg = in.substr(i + 3, close - i - 3);
            std::uint32_t v{};
            if (kind == 'x') {
                if (arg.size() != 2 || !parse_uint(arg, v, 16)) { error = "{x:HH} needs two hex digits"; return false; }
                out += static_cast<char>(v);
            } else if (kind == 'c' || kind == 'v') {
                if (!parse_uint(arg, v) || v > 99) { error = std::string("{") + kind + ":N} needs 0..99"; return false; }
                char buf[8];
                // {c:N} is the game's ~Cnn colour/style code, {v:N} its %nn variable.
                std::snprintf(buf, sizeof buf, kind == 'c' ? "~C%02u" : "%%%02u", static_cast<unsigned>(v));
                out += buf;
            } else { error = std::string("unknown code {") + kind + ":"; return false; }
            i = close;
        } else {
            out += c;
        }
    }
    return true;
}

ParsedText parse_text(std::string_view file) {
    ParsedText result;
    bool bom = false;
    const auto lines = lines_of(file, bom);
    if (bom) result.errors.push_back("file starts with a UTF-8 BOM (ignored; save without BOM)");
    for (std::size_t n = 0; n < lines.size(); ++n) {
        std::string_view line = lines[n];
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (trim(line).empty() || trim(line).front() == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string_view::npos) { result.errors.push_back(at_line(n + 1, "no '='")); continue; }
        const auto key = trim(line.substr(0, eq));
        const auto dot = key.find('.');
        Key k;
        if (dot == std::string_view::npos || !parse_uint(key.substr(0, dot), k.section) ||
            !parse_uint(key.substr(dot + 1), k.index)) {
            result.errors.push_back(at_line(n + 1, "index must be <section>.<string>, e.g. 0.12"));
            continue;
        }
        std::string text, error;
        if (!decode_escapes(line.substr(eq + 1), text, error)) { result.errors.push_back(at_line(n + 1, error)); continue; }
        if (text.find('\0') != std::string::npos) { result.errors.push_back(at_line(n + 1, "{x:00} is not allowed")); continue; }
        if (!result.lines.emplace(k, std::move(text)).second) result.errors.push_back(at_line(n + 1, "duplicate index (first kept)"));
    }
    return result;
}

GlyphMap parse_font_map(std::string_view file) {
    GlyphMap result;
    bool bom = false;
    const auto lines = lines_of(file, bom);
    if (bom) result.errors.push_back("file starts with a UTF-8 BOM (ignored; save without BOM)");
    for (std::size_t n = 0; n < lines.size(); ++n) {
        const auto line = trim(lines[n]);
        if (line.empty() || line.front() == '#') continue;
        char32_t ch{};
        std::size_t len{};
        if (!utf8_one(line, ch, len) || line.size() <= len || line[len] != '=') {
            result.errors.push_back(at_line(n + 1, "expected <char>=U+XXXX cell=N")); continue;
        }
        auto rest = trim(line.substr(len + 1));
        std::uint32_t cp{}, cell{};
        const auto space = rest.find(' ');
        const auto code = rest.substr(0, space);
        const auto tail = space == std::string_view::npos ? std::string_view{} : trim(rest.substr(space + 1));
        if (code.size() < 3 || code.substr(0, 2) != "U+" || !parse_uint(code.substr(2), cp, 16) ||
            tail.substr(0, 5) != "cell=" || !parse_uint(tail.substr(5), cell)) {
            result.errors.push_back(at_line(n + 1, "expected <char>=U+XXXX cell=N")); continue;
        }
        if (cp != ch) { result.errors.push_back(at_line(n + 1, "character and U+ code differ")); continue; }
        // 84 game glyphs sit in cells 0..83; rows y 154..175 (cells 84..95) are
        // drawn into by the game at run time, so mod glyphs start at cell 96.
        if (cell < 96 || cell >= 132) { result.errors.push_back(at_line(n + 1, "cell must be 96..131")); continue; }
        if (!result.cells.emplace(ch, cell).second) result.errors.push_back(at_line(n + 1, "duplicate character"));
    }
    return result;
}

std::optional<StringTable> read_table(std::span<const std::uint8_t> d, std::string *error) {
    auto fail = [&](const char *what) -> std::optional<StringTable> { if (error) *error = what; return std::nullopt; };
    if (d.size() < 8) return fail("entry too small");
    const auto count = le32(d, 0), table = le32(d, 4);
    if (count == 0 || count > 4096 || table < 8 || table + std::size_t{count} * 4 > d.size()) return fail("bad header");
    std::vector<std::uint32_t> starts(count);
    for (std::uint32_t s = 0; s < count; ++s) {
        starts[s] = le32(d, table + s * 4);
        if (starts[s] < table + count * 4 || starts[s] >= d.size() || (s && starts[s] < starts[s - 1]))
            return fail("bad section offset");
    }
    StringTable t;
    t.table_offset = table;
    t.sections.resize(count);
    for (std::uint32_t s = 0; s < count; ++s) {
        const std::size_t base = starts[s];
        const std::size_t end = s + 1 < count ? starts[s + 1] : d.size();
        if (end - base < 4) return fail("section too small");
        // M = lowest valid string offset / 4.
        std::size_t m = (end - base) / 4;
        for (std::size_t i = 0; i < m; ++i) {
            const auto off = le32(d, base + i * 4);
            if (off == 0xFFFFFFFFu) continue;
            if (off < 4 || base + off >= end) return fail("bad string offset");
            m = std::min<std::size_t>(m, off / 4);
        }
        auto &out = t.sections[s];
        for (std::size_t i = 0; i < m; ++i) {
            const auto off = le32(d, base + i * 4);
            if (off == 0xFFFFFFFFu) { out.emplace_back(); continue; }
            const auto *p = d.data() + base + off;
            const auto *stop = d.data() + end;
            const auto *nul = std::find(p, stop, std::uint8_t{0});
            if (nul == stop) return fail("unterminated string");
            out.emplace_back(std::string(reinterpret_cast<const char *>(p), nul - p));
        }
    }
    return t;
}

std::optional<std::vector<std::uint8_t>> write_table(const StringTable &t, std::size_t max_bytes) {
    const auto count = static_cast<std::uint32_t>(t.sections.size());
    std::vector<std::uint8_t> out(t.table_offset + std::size_t{count} * 4, 0);
    put32(out, 0, count);
    put32(out, 4, t.table_offset);
    for (std::uint32_t s = 0; s < count; ++s) {
        const auto &sec = t.sections[s];
        const std::size_t base = out.size();
        put32(out, t.table_offset + s * 4, static_cast<std::uint32_t>(base));
        out.resize(base + sec.size() * 4);
        // Identical strings in a section share one stored copy, so a longer
        // translation still fits the game's buffer (the English table leaves
        // only a few hundred bytes free). The game only reads these strings.
        std::unordered_map<std::string_view, std::uint32_t> stored;
        for (std::size_t i = 0; i < sec.size(); ++i) {
            if (!sec[i]) { put32(out, base + i * 4, 0xFFFFFFFFu); continue; }
            const auto [it, added] = stored.try_emplace(*sec[i], static_cast<std::uint32_t>(out.size() - base));
            put32(out, base + i * 4, it->second);
            if (!added) continue;
            out.insert(out.end(), sec[i]->begin(), sec[i]->end());
            out.push_back(0);
        }
        if (sec.empty()) out.resize(out.size() + 4, 0xFF);  // keep a non-empty section
        if (out.size() > max_bytes) return std::nullopt;
    }
    return out;
}

std::size_t apply(StringTable &t, const std::map<Key, std::string> &lines, std::vector<Key> *unknown) {
    std::size_t applied = 0;
    for (const auto &[k, text] : lines) {
        if (k.section >= t.sections.size() || k.index >= t.sections[k.section].size() ||
            !t.sections[k.section][k.index]) {
            if (unknown) unknown->push_back(k);
            continue;
        }
        t.sections[k.section][k.index] = text;
        ++applied;
    }
    return applied;
}

} // namespace portablekit::fu::text
