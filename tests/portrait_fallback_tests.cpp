// Tests for the launcher portrait fallback (FU Saves, 0.1.6):
//   cl /std:c++20 /EHsc /I host tests/portrait_fallback_tests.cpp
// Uses made-up files in a per-process temporary folder; no real data.
#include "portrait_file.hpp"
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
namespace fs=std::filesystem;
namespace pf=portablekit::fu::portrait_file;
namespace {
int failures=0;
#define CHECK(cond) do { if(!(cond)) { std::cerr<<__FILE__<<":"<<__LINE__<<": check failed: " #cond "\n"; ++failures; } } while(0)
std::string key(char c) { return std::string(64,c); }
fs::path make(const fs::path &dir,unsigned slot,const std::string &k,const char *ext,int age_minutes) {
    const auto p=dir/("slot-"+std::to_string(slot)+"-"+k+ext);
    std::ofstream(p,std::ios::binary)<<"x";
    // Fixed times relative to one base: no dependence on the clock's resolution.
    static const auto base=fs::file_time_type::clock::now();
    fs::last_write_time(p,base-std::chrono::minutes(age_minutes));
    return p;
}
}
int main() {
#if defined(_WIN32)
    const auto pid=_getpid();
#else
    const auto pid=getpid();
#endif
    const auto root=fs::temp_directory_path()/("mhfu_portrait_fallback_"+std::to_string(pid));
    fs::remove_all(root); fs::create_directories(root);
    // None: placeholder.
    CHECK(pf::choose(root,0,key('a'),true).empty());
    CHECK(pf::choose(root/"missing",0,key('a'),true).empty());
    // Stale files only for slot 1 (index 0); other slot's newer file must be ignored.
    const auto old_png=make(root,1,key('b'),".png",30);
    const auto newer_fum=make(root,1,key('c'),".fum",10);
    const auto newer_png=make(root,1,key('c'),".png",10);
    make(root,2,key('d'),".fum",1);
    make(root,1,"notakey",".png",0);
    CHECK(pf::choose(root,0,key('a'),true)==newer_fum);
    CHECK(pf::choose(root,0,key('a'),false)==newer_png);
    fs::remove(newer_fum); fs::remove(newer_png);
    CHECK(pf::choose(root,0,key('a'),true)==old_png);
    // Matching key wins even when older than a stale file.
    const auto exact_png=make(root,1,key('e'),".png",60);
    make(root,1,key('f'),".fum",0);
    CHECK(pf::choose(root,0,key('e'),true)==exact_png);
    const auto exact_fum=make(root,1,key('e'),".fum",60);
    CHECK(pf::choose(root,0,key('e'),true)==exact_fum);
    CHECK(pf::choose(root,0,key('e'),false)==exact_png);
    // Bad input: placeholder, files untouched.
    CHECK(pf::choose(root,3,key('e'),true).empty());
    CHECK(pf::choose(root,0,"",true).empty());
    CHECK(pf::choose(root,0,std::string(64,'G'),true).empty());
    CHECK(fs::exists(old_png) && fs::exists(exact_png) && fs::exists(exact_fum));
    std::error_code ec; fs::remove_all(root,ec);
    if(failures) { std::cerr<<failures<<" failure(s)\n"; return 1; }
    std::cout<<"portrait fallback tests passed\n"; return 0;
}