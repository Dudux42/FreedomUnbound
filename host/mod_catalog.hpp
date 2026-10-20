#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace portablekit::fu::catalog {
inline constexpr const char *repository="https://github.com/Dudux42/FreedomUnboundmods";
inline constexpr const char *index_url="https://raw.githubusercontent.com/Dudux42/FreedomUnboundmods/main/releases.json";
struct Source { std::string name,url,repository; };
Source source_from_url(std::string url);
Source default_source();
std::vector<Source> load_sources(const std::filesystem::path &file, std::string &error);
void save_sources(const std::filesystem::path &file, const std::vector<Source> &sources);
std::filesystem::path cache_directory(const std::filesystem::path &root, const Source &source);
struct Entry { std::string id,name,version,file,sha256,url,guide,origin_repository,release; std::uint64_t bytes{}; };
struct Index { std::string release; std::vector<Entry> mods; };
struct Progress { std::atomic<std::uint64_t> bytes{},total{}; std::atomic<bool> cancel{}; };
struct Listing { Index index; std::string error; bool cached{}; };
struct Download { std::filesystem::path file; std::string error; };
// Strict, bounded parsing. Throws on malformed or unsupported catalogue data.
Index parse(const std::string &json, const Source &source = default_source());
bool matches(const Entry &, const std::filesystem::path &file);
Listing fetch(const std::filesystem::path &cache, Progress &, const Source &source = default_source());
Download download(const Entry &, const std::filesystem::path &cache, Progress &);
}
