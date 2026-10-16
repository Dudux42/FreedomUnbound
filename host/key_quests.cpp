#include "key_quests.hpp"
#include "psprecomp/runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <atomic>
#include <mutex>
#include <chrono>
namespace portablekit::fu::key_quests {
namespace {
std::atomic<bool> enabled{};
std::mutex mutex;
Snapshot pending;
std::chrono::steady_clock::time_point captured;
}
bool is_key(const psprecomp::AotFastView &m, unsigned quest) {
    if (!((quest >= 1000 && quest < 4000) || (quest >= 10000 && quest < 12000))) return false;
    bool found=false;
    // Native Hall (nine tiers) and Village (nine tiers) prerequisite lists.
    // Read the owner's executable data instead of shipping a quest catalogue.
    for(unsigned group=0;group<18;group++) {
        const auto p=m.aot_load32(0x089B2010+group*4);
        if((p&1u) || p<0x089B1EA0 || p>=0x089B1F74) return false;
        bool terminated=false;
        for(unsigned i=0;i<16 && p+i*2<0x089B1F74;i++) {
            const auto id=m.aot_load16(p+i*2);
            if(!id) {terminated=true;break;}
            if(!((id>=1000 && id<4000) || (id>=10000 && id<12000))) return false;
            found |= id==quest;
        }
        if(!terminated) return false;
    }
    return found;
}
void set_enabled(bool on) {
    std::lock_guard lock(mutex);
    enabled.store(on); pending={};
}
bool visible() {
    std::lock_guard lock(mutex);
    return enabled.load() && pending.count && std::chrono::steady_clock::now()-captured<std::chrono::milliseconds(100);
}
Snapshot take_markers() {
    std::lock_guard lock(mutex);
    const auto result=enabled.load() && std::chrono::steady_clock::now()-captured<std::chrono::milliseconds(100) ? pending : Snapshot{};
    pending={};return result;
}
void capture(psprecomp::AllegrexContext &c, const psprecomp::AotFastView &m, unsigned point) {
    // The common Village/Hall quest-card title call keeps its quest record in
    // s4, with the same ID at +24 used by the adjacent native clear-status test.
    // Match the exact caller so item names, objectives and dialogue never flag.
    if(enabled.load() && point==0x0889198C && c.gpr[31]==0x09A8CE84 &&
       m.aot_load32(0x0889198C)==0x27BDFFE0 && m.aot_load32(0x09A8CE7C)==0x0E224663) {
        const auto record=c.gpr[20], title=c.gpr[9];
        if(!(record&3u) && record>=0x08800000 && record<=0x09FFFF00 &&
           title>=0x08800000 && title<0x09FFFF00 && m.aot_load8(title)) {
            const auto quest=m.aot_load16(record+24);
            const int x=static_cast<std::int16_t>(c.gpr[5]), y=static_cast<std::int16_t>(c.gpr[6]);
            if(x>=12 && x<480 && y>=0 && y<266 && is_key(m,quest)) {
                std::lock_guard lock(mutex);
                if(enabled.load()) {
                    const Marker marker{float(x-9),float(y+6),quest};
                    bool duplicate=false;
                    for(std::size_t i=0;i<pending.count;i++)
                        duplicate |= pending.markers[i].x==marker.x && pending.markers[i].y==marker.y;
                    if(!duplicate && pending.count<pending.markers.size()) pending.markers[pending.count++]=marker;
                    captured=std::chrono::steady_clock::now();
                }
            }
        }
    }
    static const bool trace = std::getenv("MHFU_KEY_QUEST_TRACE") != nullptr;
    if (!trace) return;
    auto read = [&](unsigned p) {
        std::string s;
        if (p < 0x08800000 || p >= 0x09FFFF00) return s;
        for(unsigned i=0;i<200;i++) { auto v=m.aot_load8(p+i); if(!v) break; s += v>=32 && v<127 ? char(v) : '.'; }
        return s;
    };
    const bool shadow=point==0x0889198C || point==0x08891A28;
    const auto format=read(c.gpr[shadow ? 9 : point==0x088915B0 ? 8 : 7]);
    const auto arg=read(c.gpr[point==0x08891474 ? 8 : 9]);
    static std::set<std::string> seen;
    const auto key=std::to_string(c.gpr[31])+format+arg;
    if(format.empty() || seen.size()>2000 || !seen.insert(key).second) return;
    std::printf("[key-trace] point=%08X ra=%08X obj=%08X xy=%d,%d fmt=%08X:%s arg=%08X:%s s=%08X,%08X,%08X,%08X,%08X,%08X\n",
        point,c.gpr[31],c.gpr[4],int(c.gpr[5]),int(c.gpr[6]),c.gpr[7],format.c_str(),c.gpr[8],arg.c_str(),
        c.gpr[16],c.gpr[17],c.gpr[18],c.gpr[19],c.gpr[20],c.gpr[21]);
    std::fflush(stdout);
}
}
