#include "camera_aim.hpp"
#include <cstdio>
#include <map>
#include <source_location>
using namespace portablekit::fu::camera_aim;
static int failures = 0;
void check(bool ok, const std::source_location at = std::source_location::current()) {
    if (!ok) { std::fprintf(stderr, "camera_aim regression line %u\n", at.line()); ++failures; }
}
struct Fake {
    std::map<std::uint32_t, std::uint8_t> b;
    unsigned writes = 0;
    std::uint8_t aot_load8(std::uint32_t a) const { auto i = b.find(a); return i == b.end() ? 0 : i->second; }
    std::uint16_t aot_load16(std::uint32_t a) const { return std::uint16_t(aot_load8(a) | aot_load8(a + 1) << 8); }
    std::uint32_t aot_load32(std::uint32_t a) const { return aot_load16(a) | std::uint32_t(aot_load16(a + 2)) << 16; }
    void aot_store8(std::uint32_t a, std::uint8_t v) { b[a] = v; ++writes; }
    void aot_store16(std::uint32_t a, std::uint16_t v) { b[a] = std::uint8_t(v); b[a + 1] = std::uint8_t(v >> 8); ++writes; }
    void put32(std::uint32_t a, std::uint32_t v) { for (int i = 0; i < 4; ++i) b[a + i] = std::uint8_t(v >> 8 * i); }
};
constexpr std::uint32_t H = 0x090B3580u;
void arm(Fake &m) {
    m.put32(overlay_probe, overlay_word);
    for (unsigned i = 0; i < 3; ++i) m.put32(pitch_helper + 4 * i, pitch_helper_words[i]);
    for (const Site *s : {&bow_site, &bowgun_site})
        for (unsigned i = 0; i < 4; ++i) m.put32(s->first + 4 * i, s->words[i]);
}
AimStep g_step{}; unsigned g_calls = 0; AimRoute g_route{}; float g_dt{};
AimStep provider(std::uint32_t, AimRoute r, float dt) { ++g_calls; g_route = r; g_dt = dt; return g_step; }
int main() {
    // Clamp: per field, no s8 wrap on large deltas.
    check(clamp_pitch(95, 10) == 100); check(clamp_pitch(-95, -10) == -100);
    check(clamp_pitch(0, 1000000) == 100); check(clamp_pitch(0, -2147483647) == -100);
    check(clamp_pitch(10, -3) == 7);
    {   // Applied: yaw u16 wraps, pitch clamps, nothing else written, aim seen.
        Fake m; arm(m); SiteState st; std::atomic<std::uint64_t> seen{};
        m.aot_store16(H + yaw_offset, 0xFFF0); m.aot_store8(H + pitch_offset, std::uint8_t(std::int8_t(98))); m.writes = 0;
        g_step = {0x20, 5};
        check(step(m, bow_site, st, AimRoute::bow, H, provider, true, 1000, seen) == Result::applied);
        check(m.aot_load16(H + yaw_offset) == 0x0010);
        check(std::int8_t(m.aot_load8(H + pitch_offset)) == 100);
        check(m.writes == 2); check(seen.load() == 1000); check(g_route == AimRoute::bow);
        check(recent(seen.load(), 1099)); check(!recent(seen.load(), 1100));
        g_step = {-0x20, -300};
        check(step(m, bow_site, st, AimRoute::bow, H, provider, true, 1016, seen) == Result::applied);
        check(m.aot_load16(H + yaw_offset) == 0xFFF0); check(std::int8_t(m.aot_load8(H + pitch_offset)) == -100);
        check(g_dt > 0.0155f && g_dt < 0.0165f);
    }
    {   // No-op when disabled, no provider, or idle stick.
        Fake m; arm(m); SiteState st; std::atomic<std::uint64_t> seen{}; m.writes = 0; g_calls = 0;
        g_step = {100, 1};
        check(step(m, bow_site, st, AimRoute::bow, H, provider, false, 1000, seen) == Result::disabled);
        check(step(m, bow_site, st, AimRoute::bow, H, nullptr, true, 1000, seen) == Result::disabled);
        check(g_calls == 0 && m.writes == 0 && seen.load() == 0);
        g_step = {0, 0};
        check(step(m, bowgun_site, st, AimRoute::bowgun, H, provider, true, 1000, seen) == Result::idle);
        check(m.writes == 0 && g_calls == 1);
        // Mounted routes are never applied.
        g_step = {100, 1}; g_calls = 0;
        check(step(m, bowgun_site, st, AimRoute::cone_d, H, provider, true, 1000, seen) == Result::disabled);
        check(step(m, bowgun_site, st, AimRoute::special_c, H, provider, true, 1000, seen) == Result::disabled);
        check(g_calls == 0 && m.writes == 0);
    }
    {   // Signature mismatch disarms, stays disarmed, never writes.
        Fake m; arm(m); SiteState st; std::atomic<std::uint64_t> seen{};
        m.put32(bowgun_site.first + 8, 0x0E6A2D0Bu); m.writes = 0; g_step = {100, 1}; g_calls = 0;
        check(step(m, bowgun_site, st, AimRoute::bowgun, H, provider, true, 1000, seen) == Result::disarmed);
        check(st.disarmed.load());
        m.put32(bowgun_site.first + 8, bowgun_site.words[2]);
        check(step(m, bowgun_site, st, AimRoute::bowgun, H, provider, true, 1016, seen) == Result::disarmed);
        check(g_calls == 0 && m.writes == 0 && seen.load() == 0);
        // Other overlay (identity probe) and pitch helper mismatch also disarm.
        Fake o; arm(o); SiteState s2, s3; o.put32(overlay_probe, 0);
        check(step(o, bow_site, s2, AimRoute::bow, H, provider, true, 1000, seen) == Result::disarmed);
        Fake q; arm(q); q.put32(pitch_helper + 4, 0);
        check(step(q, bow_site, s3, AimRoute::bow, H, provider, true, 1000, seen) == Result::disarmed);
        check(g_calls == 0);
    }
    if (!failures) std::puts("camera_aim tests passed");
    return failures ? 1 : 0;
}
