#include "launcher_ini.hpp"

#include "install/user_data.hpp"

#include <fstream>
#include <vector>

namespace portablekit::fu::launcher_ini {

std::filesystem::path path() { return install::user_data_directory() / "launcher.ini"; }

std::optional<std::string> get(const std::string &key) {
    std::ifstream file(path());
    std::string line;
    std::optional<std::string> value;
    const std::string prefix = key + "=";
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0) == 0) value = line.substr(prefix.size());  // last one wins
    }
    return value;
}

bool set(const std::string &key, const std::string &value) {
    std::vector<std::string> lines;
    const std::string prefix = key + "=";
    {
        std::ifstream in(path());
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind(prefix, 0) != 0) lines.push_back(line);
        }
    }
    lines.push_back(prefix + value);
    std::ofstream out(path(), std::ios::trunc);
    for (const auto &line : lines) out << line << '\n';
    out.flush();
    return static_cast<bool>(out);
}

} // namespace portablekit::fu::launcher_ini
