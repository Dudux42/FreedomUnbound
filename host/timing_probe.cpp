#include "timing_probe.hpp"
#include "native_60.hpp"

#include "kernel/kernel.hpp"
#include "settings/settings.hpp"
#include "psprecomp/runtime.hpp"

#include <chrono>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <tuple>

namespace portablekit::fu {
namespace {
using Clock = std::chrono::steady_clock;
// Measured from the supported USA BOOT.BIN, not inferred from another region.
constexpr std::uint32_t kGate = 0x08859188u;
constexpr std::uint32_t kBusy = 0x08A62D64u;
constexpr std::uint32_t kTicks = 0x08A62D68u;
constexpr std::uint32_t kWakeImport = 0x0890E070u;
constexpr std::uint32_t kWakeReturn = 0x088591F8u;
constexpr std::uint32_t kFlipImport = 0x0890E230u;

struct Probe {
    Clock::time_point start;
    std::uint64_t virtual_start{}, vblank_start{}, gates{}, wakes{}, flips{};
    std::uint64_t tick_zero{}, tick_one{}, tick_other{}, busy{};
    std::map<std::uint32_t, std::uint64_t> flip_callers;
};
Probe probe;
psprecomp::Runtime *action_runtime{};
struct ActionSample {
    std::uint64_t calls{};
    std::int32_t first0{}, last0{}, first1{}, last1{};
    std::uint32_t argument{}, id{};
    float minimum{}, maximum{}, last{};
    float base_min{}, base_max{}, step54_min{}, step54_max{};
    float effective_min{}, effective_max{};
    bool have_effective{};
    bool have_base{}, have_step54{};
    bool have_float{};
};
using ActionKey = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>;
std::map<ActionKey, ActionSample> actions;
struct EventWindowSample {
    std::uint64_t calls{}, interval_hits{}, original_interval_hits{}, comparable{};
    float effective_min{}, effective_max{};
};
std::map<std::pair<std::uint32_t, std::uint32_t>, EventWindowSample> event_windows;
std::uint64_t action_overflow{}, action_invalid{}, blend_samples{};
struct QuestSample {
    std::uint64_t last_us{}, calls{};
    std::uint32_t pause{UINT32_MAX};
    bool have{};
};
std::map<std::uint32_t, QuestSample> quest_samples;
struct MessageSample {
    std::uint64_t last_us{}, calls{};
    std::uint32_t active{}, kind{}, phase{}, substate{}, text{}, pause{}, queued{};
    bool have{};
};
std::map<std::uint32_t, MessageSample> message_samples;
Clock::time_point quest_start;
bool navigate_to_quest{};
bool quest_message_only{};
bool player_state_trace{};
struct PlayerStateSample {
    std::uint64_t last_us{}, calls{};
    std::uint32_t id{}, state{}, dispatch{};
    bool have{};
};
PlayerStateSample player_state;
std::uint64_t player_motion_second{};
std::uint32_t player_motion_samples{};
std::uint64_t overlay_trace_second{};
std::array<std::uint32_t, 5> overlay_trace_samples{};
struct UiSample { std::uint64_t calls{}; std::uint32_t raw_first{}, raw_last{}, effective_first{}, effective_last{}; };
std::map<std::uint32_t, UiSample> ui_samples;

void report_actions() {
    for (const auto &[key, value] : event_windows)
        std::printf("[fu-event-window] caller=0x%08x channel=0x%08x calls=%llu "
                    "effective_min=%.9g effective_max=%.9g comparable=%llu interval_hits=%llu original_interval_hits=%llu\n",
                    key.first, key.second, static_cast<unsigned long long>(value.calls),
                    value.effective_min, value.effective_max,
                    static_cast<unsigned long long>(value.comparable),
                    static_cast<unsigned long long>(value.interval_hits),
                    static_cast<unsigned long long>(value.original_interval_hits));
    event_windows.clear();
    for (const auto &[point, value] : ui_samples)
        std::printf("[fu-ui] point=0x%08x calls=%llu raw_first=%u raw_last=%u effective_first=%u effective_last=%u\n",
                    point, static_cast<unsigned long long>(value.calls), value.raw_first, value.raw_last,
                    value.effective_first, value.effective_last);
    ui_samples.clear();
    for (const auto &[key, value] : actions) {
        const auto &[point, object, caller, channel] = key;
        std::printf("[fu-action] point=0x%08x object=0x%08x caller=0x%08x channel=0x%08x calls=%llu "
                    "arg=%u id=%u int10_first=%d int10_last=%d int678_first=%d int678_last=%d "
                    "float14_min=%.6g float14_max=%.6g float14_last=%.6g "
                    "actor69c_min=%.6g actor69c_max=%.6g float54_min=%.6g float54_max=%.6g "
                    "effective_min=%.6g effective_max=%.6g\n",
                    point, object, caller, channel, static_cast<unsigned long long>(value.calls),
                    value.argument, value.id, value.first0, value.last0, value.first1, value.last1,
                    value.minimum, value.maximum, value.last,
                    value.base_min, value.base_max, value.step54_min, value.step54_max,
                    value.effective_min, value.effective_max);
    }
    if (action_overflow || action_invalid)
        std::printf("[fu-action-limit] omitted=%llu invalid=%llu\n",
                    static_cast<unsigned long long>(action_overflow),
                    static_cast<unsigned long long>(action_invalid));
    actions.clear();
    action_overflow = action_invalid = 0u;
}

void observe(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &ctx,
             std::uint32_t pc, std::int32_t) {
    if (pc != kGate && pc != kWakeImport && pc != kFlipImport) return;
    if (pc == kGate) {
        ++probe.gates;
        const auto ticks = runtime.memory().load32(kTicks);
        probe.tick_zero += ticks == 0u;
        probe.tick_one += ticks == 1u;
        probe.tick_other += ticks > 1u;
        probe.busy += runtime.memory().load8(kBusy) != 0u;
    } else if (pc == kWakeImport) {
        probe.wakes += ctx.gpr[31] == kWakeReturn;
    } else {
        ++probe.flips;
        if (probe.flip_callers.size() < 16u || probe.flip_callers.contains(ctx.gpr[31]))
            ++probe.flip_callers[ctx.gpr[31]];
    }
    const auto now = Clock::now();
    const double seconds = std::chrono::duration<double>(now - probe.start).count();
    if (seconds < 1.0) return;
    const auto virtual_now = kernel().now_us(), vblank_now = kernel().vblank_count();
    std::printf("[fu-timing] real_s=%.3f virtual_s=%.3f vblanks=%llu gate_calls=%llu gate_wakes=%llu "
                "flip_imports=%llu tick0=%llu tick1=%llu tick_other=%llu busy=%llu\n",
                seconds, static_cast<double>(virtual_now - probe.virtual_start) / 1000000.0,
                static_cast<unsigned long long>(vblank_now - probe.vblank_start),
                static_cast<unsigned long long>(probe.gates), static_cast<unsigned long long>(probe.wakes),
                static_cast<unsigned long long>(probe.flips), static_cast<unsigned long long>(probe.tick_zero),
                static_cast<unsigned long long>(probe.tick_one), static_cast<unsigned long long>(probe.tick_other),
                static_cast<unsigned long long>(probe.busy));
    for (const auto &[caller, count] : probe.flip_callers)
        std::printf("[fu-timing-caller] return=0x%08x flips=%llu\n", caller, static_cast<unsigned long long>(count));
    report_actions();
    std::fflush(stdout);
    probe = {};
    probe.start = now;
    probe.virtual_start = virtual_now;
    probe.vblank_start = vblank_now;
}
}

void sample_action_timing(std::uint32_t point, const psprecomp::AllegrexContext &ctx) {
    if (point == 0x09A8CE54u || point == 0x09A905F0u || point == 0x09A675ACu ||
        point == 0x09A9AA08u || point == 0x09A9C530u) {
        if (!action_runtime || !player_state_trace) return;
        const bool flight = point == 0x09A905F0u || point == 0x09A9C530u;
        const bool category = point == 0x09A675ACu;
        const bool normal = point == 0x09A9AA08u;
        const auto object = ctx.gpr[flight || category || normal ? 4u : 16u];
        if (object != 0x090B3580u) return;
        auto &memory = action_runtime->memory();
        if (!memory.contains(object, 0x6A0u)) { ++action_invalid; return; }
        const auto now = kernel().now_us();
        if (overlay_trace_second != now / 1000000u) {
            overlay_trace_second = now / 1000000u;
            overlay_trace_samples = {};
        }
        const auto slot = point == 0x09A9C530u ? 4u : normal ? 3u : category ? 2u : flight ? 1u : 0u;
        if (overlay_trace_samples[slot]++ >= 64u) return;
        const auto table_entry = ctx.gpr[category ? 3u : 2u];
        const auto handler = !flight && memory.contains(table_entry, 4u) ? memory.load32(table_entry) : 0u;
        const auto value = [&](std::uint32_t offset) { return std::bit_cast<float>(memory.load32(object + offset)); };
        std::printf("[fu-overlay-action] point=0x%08x object=0x%08x state298=%u state299=%u field1e8=%u id=%u "
                    "state1d5=%u handler=0x%08x caller=0x%08x argument=%u counter414=%u "
                    "x=%.9g y=%.9g z=%.9g motion244=%.9g accel254=%.9g virtual_us=%llu\n",
                    point, object, memory.load8(object + 0x298u), memory.load8(object + 0x299u), memory.load8(object + 0x1E8u), memory.load16(object + 0x324u),
                    memory.load8(object + 0x1D5u), handler, ctx.gpr[31], ctx.gpr[5], memory.load32(object + 0x414u),
                    value(0x200u), value(0x204u), value(0x208u), value(0x244u), value(0x254u),
                    static_cast<unsigned long long>(now));
        return;
    }
    if (action_runtime == nullptr) return;
    if (point == 0x08823A50u) {
        auto &memory = action_runtime->memory();
        const auto root = ctx.gpr[4];
        const auto state = memory.load32(0x089CC558u);
        if (!memory.contains(root, 4347u) || !memory.contains(state, 0x6AF1Bu)) { ++action_invalid; return; }
        const auto now = kernel().now_us();
        for (std::uint32_t slot = 0u; slot < 20u; ++slot) {
            const auto object = root + 3624u + slot * 36u;
            const auto active = memory.load8(object), kind = memory.load8(object + 1u);
            const auto phase = memory.load8(object + 2u);
            const auto substate = memory.load8(object + 3u);
            const auto text = memory.load32(object + 32u);
            // Actual message-pause predicate at 0x088567B4. The smaller
            // countdown guard at +0x422 is a different flag.
            const auto pause = static_cast<std::uint32_t>(memory.load8(state + 0x6AF1Au) != 0u);
            const auto queued = memory.load8(root + 4344u);
            // Ignore unused slots, retaining a final transition to inactive.
            if (active == 0u && !message_samples.contains(object)) continue;
            if (!message_samples.contains(object) && message_samples.size() >= 160u) { ++action_overflow; continue; }
            auto &sample = message_samples[object];
            ++sample.calls;
            const bool changed = !sample.have || sample.active != active || sample.kind != kind ||
                                 sample.phase != phase || sample.substate != substate || sample.text != text || sample.pause != pause || sample.queued != queued;
            if (changed || (active != 0u && now - sample.last_us >= 500000u)) {
                std::printf("[fu-message] point=0x%08x object=0x%08x slot=%u active=%u kind=%u phase=%u substate=%u remaining=%u rate=%.9g position=%.9g text=0x%08x queued=%u pause=%u calls=%llu virtual_us=%llu real_us=%llu paced=%u\n",
                            point, object, slot, active, kind, phase, substate,
                            memory.load16(object + 24u), std::bit_cast<float>(memory.load32(object + 4u)),
                            std::bit_cast<float>(memory.load32(object + 16u)), text, queued, static_cast<unsigned>(pause),
                            static_cast<unsigned long long>(sample.calls), static_cast<unsigned long long>(now),
                            static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - quest_start).count()),
                            !settings::current().unthrottled && !settings::current().fast_loading);
                sample.last_us = now;
                sample.active = active; sample.kind = kind; sample.phase = phase; sample.substate = substate;
                sample.text = text; sample.pause = pause; sample.queued = queued; sample.have = true;
                std::fflush(stdout);
            }
        }
        return;
    }
    if (point == 0x0886DFACu || point == 0x0886DFF8u) {
        auto &memory = action_runtime->memory();
        const auto object = ctx.gpr[4];
        const auto state = memory.load32(0x089CC558u);
        if (!memory.contains(object, 0x67Cu) || !memory.contains(state, 0x423u)) { ++action_invalid; return; }
        const auto pause = memory.load8(state + 0x422u);
        if (point == 0x0886DFACu) {
            // Diagnostic navigation only. Normal kernel pacing restarts from
            // this moment, before the first active quest countdown sample.
            if (navigate_to_quest && pause == 0u && memory.load8(state + 46u) == 2u &&
                std::bit_cast<std::int32_t>(memory.load32(object + 16u)) > 0) {
                settings::current().unthrottled = false;
                settings::current().fast_loading = false;
                navigate_to_quest = false;
                std::printf("[fu-navigation] normal pacing restored at active countdown; virtual_us=%llu\n",
                            static_cast<unsigned long long>(kernel().now_us()));
                std::fflush(stdout);
            }
            return;
        }
        if (!quest_samples.contains(object) && quest_samples.size() >= 64u) { ++action_overflow; return; }
        auto &sample = quest_samples[object];
        ++sample.calls;
        const auto now = kernel().now_us();
        if (!sample.have || sample.pause != pause || now - sample.last_us >= 500000u) {
            std::printf("[fu-quest-clock] object=0x%08x remaining=%d secondary=%d pause=%u state=%u step=%u calls=%llu virtual_us=%llu real_us=%llu paced=%u\n",
                        object, std::bit_cast<std::int32_t>(memory.load32(object + 16u)),
                        std::bit_cast<std::int32_t>(memory.load32(object + 0x678u)), pause,
                        memory.load8(state + 46u), ctx.gpr[5],
                        static_cast<unsigned long long>(sample.calls), static_cast<unsigned long long>(now),
                        static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - quest_start).count()),
                        !settings::current().unthrottled && !settings::current().fast_loading);
            sample.last_us = now; sample.pause = pause; sample.have = true;
            std::fflush(stdout);
        }
        return;
    }
    // A quest/message lap does not need the already measured animation traces.
    // Keep the village readiness sample used by its navigation runner.
    if (quest_message_only && point != 0x088D5E30u && point != 0x08864370u) return;
    if (player_state_trace && point == 0x088D5E30u && ctx.gpr[17] == 0x090B3580u) {
        // This player object is measured in the supported USA image and in
        // copied-save runs. Raw offset names avoid guessing field semantics.
        auto &memory = action_runtime->memory();
        const auto object = ctx.gpr[17];
        if (!memory.contains(object, 0x6A0u)) { ++action_invalid; return; }
        const auto now = kernel().now_us();
        const auto id = memory.load16(object + 0x324u);
        const auto state = memory.load8(object + 0x1D5u);
        const auto dispatch = memory.load8(object + 0x299u);
        const auto value = [&](std::uint32_t offset) { return std::bit_cast<float>(memory.load32(object + offset)); };
        const auto second = now / 1000000u;
        if (second != player_motion_second) {
            player_motion_second = second;
            player_motion_samples = 0;
        }
        // Short flight phases need individual updates rather than the ordinary
        // four-Hz state samples. Keep optional diagnostic output bounded.
        const bool moving = value(0x240u) != 0.0f || value(0x244u) != 0.0f || value(0x248u) != 0.0f;
        const bool motion_sample = moving && player_motion_samples < 64u;
        ++player_state.calls;
        if (!player_state.have || player_state.id != id || player_state.state != state || player_state.dispatch != dispatch ||
            motion_sample || now - player_state.last_us >= 250000u) {
            if (motion_sample) ++player_motion_samples;
            const auto table = memory.load32(object);
            const auto callback = memory.contains(table, 36u) ? memory.load32(table + 32u) : 0u;
            if (!player_state.have && memory.contains(table, 64u)) {
                std::printf("[fu-player-vtable] object=0x%08x table=0x%08x", object, table);
                for (std::uint32_t offset = 0; offset < 64u; offset += 4u)
                    std::printf(" slot%u=0x%08x", offset, memory.load32(table + offset));
                std::printf("\n");
            }
            std::printf("[fu-player-state] object=0x%08x id=%u state1d5=%u state299=%u callback32=0x%08x counter414=%u "
                        "animation_frame=%.9g stored_rate=%.9g x=%.9g y=%.9g z=%.9g "
                        "motion240=%.9g motion244=%.9g motion248=%.9g "
                        "accel250=%.9g accel254=%.9g accel258=%.9g calls=%llu virtual_us=%llu\n",
                        object, id, state, dispatch, callback, memory.load32(object + 0x414u), value(0x90u), value(0x94u),
                        value(0x200u), value(0x204u), value(0x208u),
                        value(0x240u), value(0x244u), value(0x248u),
                        value(0x250u), value(0x254u), value(0x258u),
                        static_cast<unsigned long long>(player_state.calls), static_cast<unsigned long long>(now));
            player_state = {now, player_state.calls, id, state, dispatch, true};
        }
    }
    if (point == 0x08863894u || point == 0x088638BCu || point == 0x0886401Cu) {
        auto &memory = action_runtime->memory();
        const auto channel = ctx.gpr[point == 0x0886401Cu ? 5u : 17u];
        if (!memory.contains(channel, 64u) || !memory.contains(ctx.gpr[4], 64u)) { ++action_invalid; return; }
        if (blend_samples++ < 4096u)
            std::printf("[fu-blend] point=0x%08x parent=0x%08x channel=0x%08x remaining=%u requested=%u weight=%.9g increment=%.9g virtual_us=%llu\n",
                        point, ctx.gpr[21], channel, memory.load32(channel + 48u),
                        point == 0x0886401Cu ? ctx.gpr[17] : 0u,
                        std::bit_cast<float>(memory.load32(channel + 52u)),
                        std::bit_cast<float>(memory.load32(channel + 36u)),
                        static_cast<unsigned long long>(kernel().now_us()));
        return;
    }
    if (point == 0x088693D8u) {
        auto &memory = action_runtime->memory();
        const auto object = ctx.gpr[16];
        if (!memory.contains(object, 24u)) { ++action_invalid; return; }
        const auto held = memory.load32(0x08A62D88u);
        const auto pressed = memory.load32(0x08A62D8Cu);
        const auto released = memory.load32(0x08A62D90u);
        const auto repeat = memory.load32(0x08A62D94u);
        if (pressed || released || repeat)
            std::printf("[fu-input] object=0x%08x held=0x%08x pressed=0x%08x released=0x%08x repeat=0x%08x counter=%u virtual_us=%llu\n",
                        object, held, pressed, released, repeat, memory.load8(object + 20u),
                        static_cast<unsigned long long>(kernel().now_us()));
        return;
    }
    if (point == 0x0884D348u || point == 0x0884D418u || point == 0x0884D754u) {
        auto &memory = action_runtime->memory();
        const auto object = ctx.gpr[point == 0x0884D754u ? 4u : 17u];
        if (!memory.contains(object, 0x30u)) { ++action_invalid; return; }
        const auto elapsed = memory.load16(object + 38u), duration = memory.load16(object + 36u);
        if (point == 0x0884D754u || elapsed <= 1u || elapsed + 1u == duration || elapsed == duration)
            std::printf("[fu-fade] point=0x%08x object=0x%08x state=%u elapsed=%u duration=%u requested=%u virtual_us=%llu\n",
                        point, object, memory.load32(object + 28u), elapsed, duration, ctx.gpr[5],
                        static_cast<unsigned long long>(kernel().now_us()));
        return;
    }
    if (point == 0x0883B72Cu || point == 0x08841B5Cu || point == 0x088F6000u) {
        auto &memory = action_runtime->memory();
        const auto root = memory.load32(0x08A632C0u);
        const auto address = static_cast<std::uint64_t>(root) + 0xF7A554u;
        if (address > UINT32_MAX || !memory.contains(static_cast<std::uint32_t>(address), 4u)) {
            ++action_invalid; return;
        }
        const auto raw = memory.load32(static_cast<std::uint32_t>(address));
        const auto effective = ctx.gpr[point == 0x08841B5Cu ? 2u : 3u];
        auto &sample = ui_samples[point];
        if (sample.calls++ == 0u) { sample.raw_first = raw; sample.effective_first = effective; }
        sample.raw_last = raw; sample.effective_last = effective;
        return;
    }
    if (point == 0x08864370u) {
        auto &memory = action_runtime->memory();
        const auto channel = ctx.gpr[3];
        if (!memory.contains(channel, 64u)) { ++action_invalid; return; }
        const auto key = std::pair{ctx.gpr[31], channel};
        if (!event_windows.contains(key) && event_windows.size() >= 64u) { ++action_overflow; return; }
        const auto frame = ctx.fpr[3], step = ctx.fpr[0], target = ctx.fpr[12];
        const auto original = std::bit_cast<float>(memory.load32(channel + 20u));
        auto &sample = event_windows[key];
        if (sample.calls++ == 0u) sample.effective_min = sample.effective_max = step;
        sample.effective_min = std::min(sample.effective_min, step);
        sample.effective_max = std::max(sample.effective_max, step);
        // Only compare the ordinary forward interval. The game's separate
        // loop-wrap and blending branches are not modeled by these counts.
        const auto end = std::bit_cast<float>(memory.load32(channel + 28u));
        if (std::isfinite(frame) && std::isfinite(step) && std::isfinite(target) &&
            std::isfinite(original) && std::isfinite(end) && step > 0.0f &&
            memory.load8(ctx.gpr[4] + 62u) == 0u && frame + original <= end) {
            ++sample.comparable;
            sample.interval_hits += frame <= target && target < frame + step;
            sample.original_interval_hits += frame <= target && target < frame + original;
        }
        return;
    }
    const bool root_motion = point == 0x08863D00u;
    const bool consumer = point == 0x088638C8u || point == 0x0886399Cu;
    const bool animation = root_motion || consumer || point == 0x088D5E30u || point == 0x088D5ED4u;
    if (!animation && point != 0x0886DFACu && point != 0x0886EAC8u) return;
    const auto object = root_motion ? ctx.gpr[17] - 0x80u : consumer ? ctx.gpr[21] - 0x80u :
        ctx.gpr[animation ? 17u : point == 0x0886DFACu ? 4u : 18u];
    const auto animation_object = ctx.gpr[consumer || root_motion ? 17u : 16u];
    auto &memory = action_runtime->memory();
    if (!memory.contains(object, animation ? 0x6A0u : 0x67Cu) ||
        (animation && !memory.contains(animation_object, 0x58u))) {
        ++action_invalid;
        return;
    }
    // Exact caller for the countdown entry; stable point for the internal
    // continuation, whose guest RA may have changed in intervening calls.
    // Separate channels: an actor may advance multiple channels in one frame.
    // Combining them would incorrectly report twice the per-channel cadence.
    const ActionKey key{point, object, animation || point == 0x0886EAC8u ? 0u : ctx.gpr[31],
                        animation ? animation_object : 0u};
    auto iter = actions.find(key);
    if (iter == actions.end()) {
        if (actions.size() >= 32u) { ++action_overflow; return; }
        iter = actions.emplace(key, ActionSample{}).first;
    }
    auto &sample = iter->second;
    const auto value0 = std::bit_cast<std::int32_t>(memory.load32(object + 0x10u));
    const auto value1 = std::bit_cast<std::int32_t>(memory.load32(object + 0x678u));
    if (sample.calls++ == 0u) { sample.first0 = value0; sample.first1 = value1; }
    sample.last0 = value0;
    sample.last1 = value1;
    sample.argument = animation ? animation_object : ctx.gpr[5];
    if (animation) {
        const auto range = [](float value, float &minimum, float &maximum, bool &have) {
            if (!std::isfinite(value)) return;
            if (!have) minimum = maximum = value;
            have = true;
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        };
        range(std::bit_cast<float>(memory.load32(object + 0x69Cu)),
              sample.base_min, sample.base_max, sample.have_base);
        if (consumer || root_motion)
            range(ctx.fpr[root_motion ? 0u : 1u], sample.effective_min, sample.effective_max, sample.have_effective);
        range(std::bit_cast<float>(memory.load32(animation_object + 0x54u)),
              sample.step54_min, sample.step54_max, sample.have_step54);
        sample.id = memory.load16(object + 0x324u);
        const auto value = std::bit_cast<float>(memory.load32(animation_object + 0x14u));
        if (std::isfinite(value)) {
            if (!sample.have_float) { sample.minimum = sample.maximum = value; }
            sample.have_float = true;
            sample.minimum = std::min(sample.minimum, value);
            sample.maximum = std::max(sample.maximum, value);
            sample.last = value;
        }
    }
}

void install_timing_probe(psprecomp::Runtime &runtime, const psprecomp::Elf32Image &) {
    initialize_native_60(runtime);
    action_runtime = nullptr;
    quest_message_only = false;
    player_state_trace = false;
    player_state = {};
    player_motion_second = 0;
    player_motion_samples = 0;
    overlay_trace_second = 0;
    overlay_trace_samples = {};
    const auto flag = std::getenv("MHFU_TRACE_TIMING");
    if (flag == nullptr || flag[0] != '1' || flag[1] != '\0') return;
    // Refuse to read region-specific state unless this exact gate is present.
    const std::pair<std::uint32_t, std::uint32_t> signature[]{
        {0x08859190u, 0x3C0308A6u}, {0x08859194u, 0x8C642D68u},
        {0x088591A0u, 0xAC642D68u}, {0x088591A4u, 0x28830002u},
        {0x088591B4u, 0x90632D64u}, {0x088591D8u, 0x0E22417Bu}
    };
    for (const auto &[address, word] : signature) {
        if (!runtime.memory().contains(address, 4u) || runtime.memory().load32(address) != word) {
            std::printf("[fu-timing] unsupported frame-gate signature; probe disabled\n");
            return;
        }
    }
    probe = {};
    probe.start = Clock::now();
    probe.virtual_start = kernel().now_us();
    probe.vblank_start = kernel().vblank_count();
    const std::pair<std::uint32_t, std::uint32_t> action_signature[]{
        {0x08863894u, 0x46020840u}, {0x088638BCu, 0xA220003Eu},
        {0x0886401Cu, 0xACB10030u},
        {0x08864368u, 0xC4600014u}, {0x08864370u, 0x46001800u},
        {0x088693D8u, 0x86040008u},
        {0x0884D348u, 0x24630001u}, {0x0884D418u, 0x24630001u},
        {0x0884D754u, 0xA4850024u},
        {0x0883B72Cu, 0x30630010u}, {0x08841B5Cu, 0x30420004u},
        {0x088F6000u, 0x30630010u},
        {0x0886DFACu, 0x3C03089Du}, {0x0886DFC4u, 0x58600006u},
        {0x0886DFF8u, 0x03E00008u}, {0x0886EAC4u, 0x24050001u},
        {0x08823A50u, 0x27BDFFF0u},
        {0x088567B4u, 0x3C020007u}, {0x088567BCu, 0x9042AF1Au},
        {0x088D5E30u, 0x8E230614u}, {0x088D5ED4u, 0x0E219592u},
        {0x088638C8u, 0x46010000u}, {0x0886399Cu, 0x24130001u}, {0x08863D00u, 0x27A50080u}
    };
    bool action_supported = true;
    for (const auto &[address, word] : action_signature)
        action_supported &= runtime.memory().contains(address, 4u) && runtime.memory().load32(address) == word;
    actions.clear();
    event_windows.clear();
    ui_samples.clear();
    action_overflow = action_invalid = 0u;
    blend_samples = 0u;
    quest_samples.clear();
    message_samples.clear();
    quest_start = Clock::now();
    const auto navigation = std::getenv("MHFU_TIMING_NAVIGATION");
    const auto focused = std::getenv("MHFU_TIMING_QUEST_MESSAGES_ONLY");
    quest_message_only = focused && focused[0] == '1' && focused[1] == '\0';
    const auto player_trace = std::getenv("MHFU_TIMING_PLAYER_STATE");
    player_state_trace = player_trace && player_trace[0] == '1' && player_trace[1] == '\0';
    navigate_to_quest = action_supported && navigation && navigation[0] == '1' && navigation[1] == '\0';
    if (navigate_to_quest) {
        settings::current().unthrottled = true;
        settings::current().fast_loading = false;
        std::printf("[fu-navigation] DIAGNOSTIC ONLY: accelerated navigation until active countdown\n");
    }
    if (action_supported) action_runtime = &runtime;
    else std::printf("[fu-action] unsupported action signatures; internal samples disabled\n");
    psprecomp::set_runtime_pre_dispatch_hook(observe);
    std::printf("[fu-timing] USA observation probe enabled; requested game cadence %u FPS\n",
                native_60_requested() ? 60u : 30u);
}
}
