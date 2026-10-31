#pragma once
// FU Saves: picks the launcher portrait file for one save slot. Header-only
// so the launcher and tests/portrait_fallback_tests.cpp share it.
#include <filesystem>
#include <string>
namespace portablekit::fu::portrait_file {
inline bool valid_key(const std::string &key) {
    return key.size()==64 && key.find_first_not_of("0123456789abcdef")==std::string::npos;
}
// Exact slot-N-<key>.fum (when models are on) or .png first. If neither exists,
// fall back to the slot's newest slot-N-*.fum/.png by write time, so an
// equipment change shows the previous portrait instead of the placeholder.
// Empty result means draw the placeholder. Never deletes or writes files.
inline std::filesystem::path choose(const std::filesystem::path &dir,unsigned slot,const std::string &key,bool models) {
    namespace fs=std::filesystem;
    if(slot>=3 || !valid_key(key)) return {};
    const auto stem="slot-"+std::to_string(slot+1)+"-";
    std::error_code ec;
    const auto exact=dir/(stem+key+".png");
    if(models) { auto mesh=exact; mesh.replace_extension(".fum"); if(fs::is_regular_file(mesh,ec)) return mesh; }
    if(fs::is_regular_file(exact,ec)) return exact;
    fs::path best; fs::file_time_type best_time{}; bool best_mesh=false;
    for(fs::directory_iterator it(dir,ec),end;!ec && it!=end;it.increment(ec)) {
        const auto &p=it->path(); const auto name=p.filename().string(); const auto ext=p.extension().string();
        const bool mesh=ext==".fum";
        if((!mesh && ext!=".png") || (mesh && !models)) continue;
        if(name.size()!=stem.size()+64+ext.size() || name.compare(0,stem.size(),stem)) continue;
        if(!valid_key(name.substr(stem.size(),64))) continue;
        std::error_code fe; if(!it->is_regular_file(fe) || fe) continue;
        const auto t=fs::last_write_time(p,fe); if(fe) continue;
        if(best.empty() || t>best_time || (t==best_time && mesh && !best_mesh)) { best=p; best_time=t; best_mesh=mesh; }
    }
    return best;
}
}