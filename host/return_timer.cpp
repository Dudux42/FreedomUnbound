#include "return_timer.hpp"
#include "psprecomp/runtime.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <cstdlib>
namespace portablekit::fu::return_timer {
namespace {
std::atomic<bool> enabled{};
std::mutex mutex;
Snapshot current;
std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool valid(unsigned p,unsigned length) {
    return !(p&3u) && p>=0x08800000 && p<=0x0A000000-length;
}
}
unsigned seconds_left(unsigned frames) { return frames/30u + (frames%30u != 0); }
void set_enabled(bool on) {
    std::lock_guard lock(mutex);enabled.store(on);current={};
}
void refresh(const psprecomp::AotFastView &m) {
    if(!enabled.load()) return;
    Snapshot next;
    const auto state=m.aot_load32(0x089CC558), quest=m.aot_load32(0x08A62D9C);
    const bool supported=m.aot_load32(0x0886B494)==0x2442FFFF &&
        m.aot_load32(0x0886B49C)==0xAE220710 &&
        m.aot_load32(0x09A6B154)==0x106005F7;
    unsigned phase=0,mode=0,stage=0;int frames=0;
    if(supported && valid(state,0x430) && valid(quest,0x720)) {
        phase=m.aot_load8(state+46);mode=m.aot_load8(quest);stage=m.aot_load8(quest+1);
        frames=static_cast<std::int32_t>(m.aot_load32(quest+0x710));
        // Native success-return state. The preceding five-second success
        // message uses the same field but stage 1, so must never show as return.
        if(phase==3 && mode==1 && stage==2 && frames>0 && frames<=1800)
            next={unsigned(frames),seconds_left(unsigned(frames)),now_ns()};
    }
    static const bool trace=std::getenv("MHFU_RETURN_TIMER_TRACE")!=nullptr;
    if(trace) {
        static unsigned last_phase=~0u,last_mode=~0u,last_stage=~0u,last_seconds=~0u;
        if(phase!=last_phase || mode!=last_mode || stage!=last_stage || next.seconds!=last_seconds) {
            std::printf("[return-timer] phase=%u mode=%u stage=%u frames=%d seconds=%u\n",phase,mode,stage,frames,next.seconds);
            std::fflush(stdout);
            last_phase=phase;last_mode=mode;last_stage=stage;last_seconds=next.seconds;
        }
    }
    std::lock_guard lock(mutex);
    if(enabled.load()) current=next;
}
Snapshot snapshot() {
    std::lock_guard lock(mutex);
    if(!enabled.load() || !current.timestamp || now_ns()-current.timestamp>500000000u) return {};
    return current;
}
}
