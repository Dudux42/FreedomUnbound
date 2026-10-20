#include "native_60.hpp"
#include "half_step.hpp"
#include "blend_clock.hpp"
#include "message_clock.hpp"
#include "psprecomp/runtime.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace portablekit::fu {
namespace {
bool active{};
bool deferred_for_navigation{};
psprecomp::Runtime *runtime_ptr{};
struct Countdown { std::uint32_t object{}; HalfStep step; };
std::array<Countdown, 64> countdowns{};
std::array<Countdown, 64> fades{};
std::array<Countdown, 64> repeats{};
struct Message { std::uint32_t object{}; MessageClock clock; };
std::array<Message, 64> messages{};
MessageClock &message_step(std::uint32_t object) {
    if (object == 0u || runtime_ptr == nullptr || !runtime_ptr->memory().contains(object, 36u))
        throw std::runtime_error("Native 60 research mode: invalid message object");
    for (auto &message : messages) if (message.object == object) return message.clock;
    for (auto &message : messages) if (message.object == 0u) { message.object = object; return message.clock; }
    throw std::runtime_error("Native 60 research mode: message tracker capacity exceeded");
}
struct Blend { std::uint32_t object{}; BlendClock clock; };
std::array<Blend, 128> blends{};
BlendClock &blend_step(std::uint32_t object) {
    if (object == 0u || runtime_ptr == nullptr || !runtime_ptr->memory().contains(object, 64u))
        throw std::runtime_error("Native 60 research mode: invalid blend channel");
    for (auto &blend : blends) if (blend.object == object) return blend.clock;
    for (auto &blend : blends) if (blend.object == 0u) { blend.object = object; return blend.clock; }
    throw std::runtime_error("Native 60 research mode: blend tracker capacity exceeded");
}
HalfStep &repeat_step(std::uint32_t object) {
    if (object == 0u || runtime_ptr == nullptr || !runtime_ptr->memory().contains(object, 24u))
        throw std::runtime_error("Native 60 research mode: invalid input object");
    for (auto &repeat : repeats) if (repeat.object == object) return repeat.step;
    for (auto &repeat : repeats) if (repeat.object == 0u) { repeat.object = object; return repeat.step; }
    throw std::runtime_error("Native 60 research mode: input tracker capacity exceeded");
}
HalfStep &fade_step(std::uint32_t object) {
    if (object == 0u || runtime_ptr == nullptr || !runtime_ptr->memory().contains(object, 0x30u))
        throw std::runtime_error("Native 60 research mode: invalid fade object");
    for (auto &fade : fades) if (fade.object == object) return fade.step;
    for (auto &fade : fades) if (fade.object == 0u) { fade.object = object; return fade.step; }
    throw std::runtime_error("Native 60 research mode: fade tracker capacity exceeded");
}
}

bool native_60_requested() {
#if defined(MHFU_NATIVE_60_EXPERIMENT)
    const char *flag = std::getenv("MHFU_NATIVE_60_EXPERIMENT");
    return flag && flag[0] == '1' && flag[1] == '\0';
#else
    return false;
#endif
}

void initialize_native_60(psprecomp::Runtime &runtime) {
    active = false;
    deferred_for_navigation = false;
    runtime_ptr = nullptr;
    countdowns = {};
    fades = {};
    repeats = {};
    messages = {};
    blends = {};
    if (!native_60_requested()) return;
    // Independently measured USA instructions. Fail closed on an unsupported
    // image rather than running with only some timing corrections active.
    constexpr std::pair<std::uint32_t, std::uint32_t> signatures[]{
        {0x08863894u, 0x46020840u}, {0x088638A0u, 0x2442FFFFu},
        {0x088638BCu, 0xA220003Eu}, {0x0886401Cu, 0xACB10030u},
        {0x088640B0u, 0xA080003Eu},
        {0x08864368u, 0xC4600014u},
        {0x0885A120u, 0xA6030018u}, {0x0885A838u, 0x2463FFFFu},
        {0x0885A8F0u, 0x27BDFFF0u},
        {0x0885A8E4u, 0x24630001u},
        {0x0885A7E0u, 0x46010000u}, {0x0885A8A8u, 0x46020840u},
        {0x08869398u, 0x24630001u}, {0x088693D4u, 0xA2000014u},
        {0x0884D348u, 0x24630001u}, {0x0884D418u, 0x24630001u},
        {0x0884D754u, 0xA4850024u},
        {0x0883B728u, 0x8C63A554u}, {0x08841B58u, 0x8C42A554u},
        {0x088F5FFCu, 0x8C63A554u},
        {0x088591A4u, 0x28830002u}, {0x088591B4u, 0x90632D64u},
        {0x0886DFACu, 0x3C03089Du}, {0x0886DFCCu, 0x00651823u},
        {0x0886DFE8u, 0x00651823u}, {0x0886EAC4u, 0x24050001u},
        {0x088638C0u, 0xC6210014u}, {0x088638D4u, 0xC6200014u},
        {0x08863974u, 0x56800008u}, {0x08863978u, 0xC6210014u},
        {0x08863994u, 0xC6210014u},
        {0x08863BE8u, 0xC6200014u}, {0x08863C44u, 0xC6200014u}, {0x08863CFCu, 0xC6200014u}
    };
    for (const auto &[address, word] : signatures)
        if (!runtime.memory().contains(address, 4u) || runtime.memory().load32(address) != word)
            throw std::runtime_error("Native 60 research mode: unsupported USA timing signature");
    runtime_ptr = &runtime;
    active = true;
    std::printf("[fu-native60] RESEARCH ONLY: one-vblank updates; countdown and animation-consumer corrections; independent movement, blend, hit-stop and UI timing INCOMPLETE\n");
    const auto navigation = std::getenv("MHFU_TIMING_NAVIGATION");
    const auto baseline_navigation = std::getenv("MHFU_NATIVE60_BASELINE_NAVIGATION");
    const auto tracing = std::getenv("MHFU_TRACE_TIMING");
    const auto enabled = [](const char *flag) { return flag && flag[0] == '1' && flag[1] == '\0'; };
    if (enabled(navigation) && enabled(baseline_navigation) && enabled(tracing)) {
        active = false;
        deferred_for_navigation = true;
        std::printf("[fu-native60] DIAGNOSTIC ONLY: baseline navigation; native hooks deferred until active quest countdown\n");
    }
    std::fflush(stdout);
}

std::uint32_t native_frame_threshold() { return active ? 1u : 2u; }
// Scale the consumed step, never the stored rate: states may retain that rate
// over many frames. The original rate producers and caps remain unchanged.
float native_animation_step(float value) { return active ? value * 0.5f : value; }

// Only called by measured bit-mask UI blink consumers. Never changes the
// shared frame counter or its alternating-work and state consumers.
std::uint32_t native_ui_clock(std::uint32_t value) { return active ? value / 2u : value; }

void native_fade_reset(std::uint32_t object) { if (active) fade_step(object) = {}; }
std::uint32_t native_fade_tick(std::uint32_t object) { return active ? fade_step(object).consume(1u) : 1u; }

// Called only by the stable-held-mask branch. A changed mask still emits
// fresh presses/releases immediately and resets this object's fractional carry.
void native_repeat_reset(std::uint32_t object) { if (active) repeat_step(object) = {}; }
std::uint32_t native_repeat_tick(std::uint32_t object) { return active ? repeat_step(object).consume(1u) : 1u; }

// Preserve the original stored slide rate and 60-tick hold duration. Each
// active hold update consumes half a tick; new/reused slots reset the carry.
void native_message_reset(std::uint32_t object) { if (active) message_step(object) = {}; }
void native_message_finish(std::uint32_t object) {
    if (active) for (auto &message : messages) if (message.object == object) { message = {}; return; }
}
std::uint32_t native_message_tick(std::uint32_t object) {
    if (!active) return 1u;
    auto &clock = message_step(object);
    return clock.tick(runtime_ptr->memory().load8(object + 2u));
}
float native_message_motion(float value) { return active ? value * 0.5f : value; }

// Channels can restart or abandon a transition. Reset fractional carry on
// setup and release it on either completion or the immediate-change path.
void native_blend_reset(std::uint32_t object) { if (active) blend_step(object) = {}; }
void native_blend_finish(std::uint32_t object) {
    if (active) for (auto &blend : blends) if (blend.object == object) { blend = {}; return; }
}
float native_blend_increment(float value, std::uint32_t object) {
    return active ? value * blend_step(object).weight_scale() : value;
}
std::uint32_t native_blend_tick(std::uint32_t object) { return active ? blend_step(object).tick() : 1u; }

void native_countdown_step(psprecomp::AllegrexContext &ctx) {
    if (deferred_for_navigation) {
        auto &memory = runtime_ptr->memory();
        const auto state = memory.load32(0x089CC558u);
        const auto object = ctx.gpr[4];
        if (!memory.contains(state, 0x423u) || !memory.contains(object, 0x67Cu))
            throw std::runtime_error("Native 60 research mode: invalid diagnostic navigation state");
        if (memory.load8(state + 0x422u) != 0u || memory.load8(state + 46u) != 2u ||
            static_cast<std::int32_t>(memory.load32(object + 16u)) <= 0) return;
        active = true;
        deferred_for_navigation = false;
        std::printf("[fu-native60] DIAGNOSTIC ONLY: native hooks activated at active quest countdown\n");
        std::fflush(stdout);
    }
    if (!active) return;
    const auto object = ctx.gpr[4];
    // The measured routine first checks this pause flag. A paused call must
    // not consume the fractional remainder, even though it reaches this hook.
    auto &memory = runtime_ptr->memory();
    const auto state = memory.load32(0x089CC558u);
    if (!memory.contains(state, 0x423u) || !memory.contains(object, 0x67Cu))
        throw std::runtime_error("Native 60 research mode: invalid countdown state");
    if (memory.load8(state + 0x422u) != 0u) return;
    for (auto &counter : countdowns) {
        if (counter.object != 0u && counter.object != object) continue;
        counter.object = object;
        ctx.gpr[5] = counter.step.consume(ctx.gpr[5]);
        return;
    }
    throw std::runtime_error("Native 60 research mode: countdown tracking limit exceeded");
}
}
