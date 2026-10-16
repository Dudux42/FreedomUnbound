#pragma once
// In-game log file: a copy of everything FreedomUniteNative writes to stdout
// and stderr (host and game output alike) in <data>\logs\FreedomUnbound-
// <yyyyMMdd-HHmmss>.log. The console sees exactly the same bytes as before.
// Lines that mention savedata or carry a long hex run are never written to
// the file, so a session with MHFU_TRACE_SAVEDATA on cannot leak a key.
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace portablekit::fu::game_log {

inline constexpr std::size_t kKeepFiles = 10u;
inline constexpr std::string_view kPrefix = "FreedomUnbound-";
inline constexpr std::string_view kSuffix = ".log";

// False for a line that must not reach the file: "savedata" in any case, or
// 32 or more hex digits in a row.
bool keep_line(std::string_view line);

// "FreedomUnbound-20261013-235100.log"
std::string file_name(const std::tm &local_time);

// Our log files among `names`, oldest first by name (the names sort by time).
std::vector<std::string> log_files(const std::vector<std::string> &names);
// The files to delete so that `keep` of ours remain (other names untouched).
std::vector<std::string> files_to_delete(const std::vector<std::string> &names, std::size_t keep = kKeepFiles);
// Deletes the oldest of our files in `dir` beyond `keep`. Returns how many went.
std::size_t rotate(const std::filesystem::path &dir, std::size_t keep = kKeepFiles);

// Starts the tee (Windows; elsewhere a no-op). Called once, early; safe to call again.
void start();

} // namespace portablekit::fu::game_log
