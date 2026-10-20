#include "buff_icons.hpp"
#include "psprecomp/runtime.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace portablekit::fu::buff_icons {
namespace {
std::atomic<bool> enabled{};
std::mutex mutex;
Snapshot current;
unsigned previous_hunter{},previous_state{},previous_heat{},heat_rate=1;
std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool valid(unsigned p,unsigned bytes) { return !(p&3u) && p>=0x08800000u && p<=0x0A000000u-bytes; }
// USA local hunter: native item-effect setters and per-frame decrements.
constexpr std::array<unsigned,item_count> offsets{982,986,1570,1576,1574,1572,1578};
}
Warning warning(unsigned frames) {
    if(frames && frames<=300)return Warning::Urgent;
    if(frames && frames<=900)return Warning::Pulse;
    return Warning::Normal;
}
Appearance appearance(unsigned frames,double time) {
    const auto state=warning(frames);
    if(state==Warning::Normal)return {};
    // Reserved slot widths accommodate the pulse without moving neighboring icons.
    const double cycle=time*(state==Warning::Urgent ? 2.0 : 1.0);
    return {static_cast<float>(1.0+.12*std::sin(cycle*6.283185307179586)),
            state==Warning::Urgent && cycle-std::floor(cycle)<.5};
}
void set_enabled(bool on) {
    std::lock_guard lock(mutex);enabled.store(on);current={};
    previous_hunter=previous_state=previous_heat=0;heat_rate=1;
}
void refresh(const psprecomp::AotFastView &m) {
    if(!enabled.load())return;
    std::lock_guard lock(mutex);if(!enabled.load())return;
    Snapshot next;
    const auto state=m.aot_load32(0x089CC558),hunter=m.aot_load32(0x099998F4);
    // Require the actual quest overlay and its attack/defense decrement stores.
    if(m.aot_load32(0x09A6B154)==0x106005F7 &&
       m.aot_load32(0x09A693E0)==0xA60203D6 && m.aot_load32(0x09A69404)==0xA60203DA &&
       m.aot_load32(0x09A69EC0)==0xA4620000 && valid(state,0x430) && valid(hunter,0x700)) {
        const auto phase=m.aot_load8(state+46);
        const auto quest=m.aot_load32(0x08A62D9C);
        const bool success_play=phase==3 && valid(quest,4) &&
            m.aot_load8(quest)==1 && m.aot_load8(quest+1)<=2;
        const auto hp=m.aot_load16(hunter+0x2E4),maximum=m.aot_load16(hunter+0x41E);
        if((phase==2 || success_play) && hp && maximum && maximum<=150 && hp<=maximum) {
            for(unsigned i=0;i<item_count;i++) {
                const auto frames=m.aot_load16(hunter+offsets[i]);
                // Timed attack/defense require a positive benefit, not an expired
                // modifier or a negative defense effect sharing these fields.
                if(i<2 && static_cast<std::int8_t>(m.aot_load8(hunter+(i==0 ? 980 : 984)))<=0)continue;
                unsigned remaining=frames;
                if(i==static_cast<unsigned>(Kind::Heat)) {
                    // Severe cold consumes Hot Drink at two native ticks per
                    // frame. Learn that rate from consecutive native samples;
                    // a frozen counter retains it, never using wall time.
                    if(hunter!=previous_hunter || state!=previous_state || frames>previous_heat || !frames)heat_rate=1;
                    else if(previous_heat>frames && previous_heat-frames<=2)heat_rate=previous_heat-frames;
                    previous_heat=frames;remaining=(frames+heat_rate-1)/heat_rate;
                }
                if(frames && frames<=32767)next.buffs[next.count++]={static_cast<Kind>(i),remaining};
            }
            // Native hunter update iterates exactly 21 melody timers at +0x6A0
            // and clears the matching +0x5F7 effect byte on expiration.
            for(unsigned i=0;i<melody_count;i++) {
                const auto frames=m.aot_load16(hunter+1696+2*i);
                if(frames && frames<=32767 && m.aot_load8(hunter+1527+i))
                    next.buffs[next.count++]={static_cast<Kind>(item_count+i),frames};
            }
            previous_hunter=hunter;previous_state=state;
            next.timestamp=now_ns();
        }
    }
    static const bool trace=std::getenv("MHFU_BUFF_TRACE")!=nullptr;
    if(trace) {
        static std::array<unsigned,capacity> last{};
        std::array<unsigned,capacity> values{};
        for(unsigned i=0;i<next.count;i++)values[static_cast<unsigned>(next.buffs[i].kind)]=(next.buffs[i].frames+29)/30;
        if(values!=last) {
            std::printf("[buff-icons] attack=%u defense=%u stamina=%u recovery=%u heat=%u cold=%u vision=%u\n",
                        values[0],values[1],values[2],values[3],values[4],values[5],values[6]);
            std::fflush(stdout);last=values;
        }
    }
    if(!next.timestamp){previous_hunter=previous_state=previous_heat=0;heat_rate=1;}
    current=next;
}
Snapshot snapshot() {
    std::lock_guard lock(mutex);
    if(!enabled.load() || !current.timestamp || now_ns()-current.timestamp>500000000u)return {};
    return current;
}
}
