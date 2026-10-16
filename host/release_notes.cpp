#include "release_notes.hpp"

#ifndef RELEASE_NOTES_STANDALONE
#include "install/user_data.hpp"
#endif

#include <fstream>
#include <sstream>
#include <system_error>

namespace portablekit::fu::release_notes {
namespace {
void put_utf8(std::string &out, unsigned long cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}
bool hex4(const std::string &s, size_t at, unsigned long &value) {
    if (at + 4 > s.size()) return false;
    value = 0;
    for (size_t i = at; i < at + 4; ++i) {
        const char c = s[i];
        value <<= 4;
        if (c >= '0' && c <= '9') value |= static_cast<unsigned long>(c - '0');
        else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned long>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned long>(c - 'A' + 10);
        else return false;
    }
    return true;
}
std::string strip_v(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    return s;
}
std::optional<std::string> read_all(const std::filesystem::path &file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    std::string s = text.str();
    if (s.size() >= 3 && s.compare(0, 3, "\xEF\xBB\xBF") == 0) s.erase(0, 3);
    return s;
}
} // namespace

std::string json_text(const std::string &json, const std::string &key) {
    const std::string needle = "\"" + key + "\"";
    size_t k = json.find(needle);
    if (k == std::string::npos) return {};
    size_t i = json.find(':', k + needle.size());
    if (i == std::string::npos) return {};
    ++i;
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n')) ++i;
    if (i >= json.size() || json[i] != '"') return {};  // null or not a string
    std::string out;
    for (++i; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] != '\\' || i + 1 >= json.size()) {
            out += json[i];
            continue;
        }
        const char e = json[++i];
        switch (e) {
        case 'n': out += '\n'; break;
        case 'r': break;  // GitHub bodies use \r\n
        case 't': out += '\t'; break;
        case 'b': case 'f': break;
        case 'u': {
            unsigned long cp = 0;
            if (!hex4(json, i + 1, cp)) break;
            i += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 2 < json.size() && json.compare(i + 1, 2, "\\u") == 0) {
                unsigned long low = 0;
                if (hex4(json, i + 3, low) && low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    i += 6;
                }
            }
            put_utf8(out, cp);
            break;
        }
        default: out += e; break;  // \" \\ \/
        }
    }
    return out;
}

bool same_version(const std::string &a, const std::string &b) { return !a.empty() && strip_v(a) == strip_v(b); }

bool write_cache_to(const std::filesystem::path &file, const std::string &tag, const std::string &notes) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    const std::filesystem::path temp = file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << tag << '\n' << notes;
        if (!out) return false;
    }
    std::filesystem::rename(temp, file, ec);
    return !ec;
}

std::optional<std::string> read_cache_from(const std::filesystem::path &file, const std::string &version) {
    const auto text = read_all(file);
    if (!text) return std::nullopt;
    const size_t line = text->find('\n');
    if (line == std::string::npos || !same_version(text->substr(0, line), version)) return std::nullopt;
    std::string notes = text->substr(line + 1);
    if (notes.find_first_not_of(" \r\n\t") == std::string::npos) return std::nullopt;
    return notes;
}

std::optional<std::string> read_bundled(const std::filesystem::path &program_dir) {
    auto text = read_all(program_dir / "WHATSNEW.md");
    if (!text || text->find_first_not_of(" \r\n\t") == std::string::npos) return std::nullopt;
    return text;
}

bool due(const std::optional<std::string> &last_seen, const std::string &version) {
    return !last_seen || !same_version(*last_seen, version);
}

#ifndef RELEASE_NOTES_STANDALONE
bool write_cache(const std::string &tag, const std::string &notes) {
    return write_cache_to(install::user_data_directory() / "whatsnew-cache.txt", tag, notes);
}
std::optional<std::string> read_cache(const std::string &version) {
    return read_cache_from(install::user_data_directory() / "whatsnew-cache.txt", version);
}
#endif
} // namespace portablekit::fu::release_notes
