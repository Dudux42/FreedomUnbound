#include "camera_aim.hpp"
#include "psprecomp/runtime.hpp"
#include <chrono>
namespace portablekit::fu::camera_aim {
namespace {
std::atomic<Provider> provider{};
std::atomic<bool> enabled{};
std::atomic<std::uint64_t> seen{};
SiteState bow_state, bowgun_state;
std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()) + 1u;
}
// AotFastView is const in the callback signature but stores are allowed on it
// (modern_camera::geometry does the same).
struct View {
    const psprecomp::AotFastView &m;
    std::uint32_t aot_load32(std::uint32_t a) const { return m.aot_load32(a); }
    std::uint16_t aot_load16(std::uint32_t a) const { return static_cast<std::uint16_t>(m.aot_load16(a)); }
    std::uint8_t aot_load8(std::uint32_t a) const { return static_cast<std::uint8_t>(m.aot_load8(a)); }
    void aot_store16(std::uint32_t a, std::uint16_t v) { m.aot_store16(a, v); }
    void aot_store8(std::uint32_t a, std::uint8_t v) { m.aot_store8(a, v); }
};
}
void set_provider(Provider p) { provider.store(p); }
void set_enabled(bool on) { enabled.store(on); if (!on) seen.store(0); }
bool armed() { return enabled.load() && provider.load() && !bow_state.disarmed.load() && !bowgun_state.disarmed.load(); }
bool aiming() { return enabled.load() && recent(seen.load(), now_ms()); }
void reset() {
    bow_state.disarmed.store(false); bowgun_state.disarmed.store(false);
    bow_state.last_ms.store(0); bowgun_state.last_ms.store(0); seen.store(0);
}
void bow(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    View v{m};
    step(v, bow_site, bow_state, AimRoute::bow, ctx.gpr[17], provider.load(), enabled.load(), now_ms(), seen);
}
void bowgun(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    View v{m};
    step(v, bowgun_site, bowgun_state, AimRoute::bowgun, ctx.gpr[18], provider.load(), enabled.load(), now_ms(), seen);
}
}
