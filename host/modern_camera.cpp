#include "modern_camera.hpp"
#include "modern_camera_math.hpp"
#include "monster_catalog.hpp"
#include "camera/camera_input.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <mutex>
#include <optional>
namespace portablekit::fu::modern_camera {
namespace {
std::atomic<bool> enabled{}, active{}, target_controls{};
std::atomic<bool> invert_horizontal{}, invert_vertical{}, targeting_enabled{true};
std::atomic<float> horizontal_sensitivity{1.f}, vertical_sensitivity{1.f}, smoothing_strength{};
std::atomic<float> response_curve{1.8f}, pitch_min{-20.f}, pitch_max{70.f};
std::uint64_t last_supported{};
std::atomic<bool> orbit_available{}, aim_enabled{true}, aim_invert_x{}, aim_invert_y{};
std::atomic<float> aim_sensitivity{1.f}, aim_curve{2.f}, aim_smoothing{30.f}, aim_deadzone{.12f};
float aim_speed=90.f, input_deadzone=.15f;
bool input_invert_x{},input_invert_y{},was_aiming{};
std::atomic<bool> aim_controls{};
std::optional<psprecomp::AotFastView> aim_memory;
struct AimMotion {
    std::uint32_t hunter{};camera_aim::AimRoute route{};
    float yaw_velocity{},pitch_velocity{},yaw_remainder{},pitch_remainder{};
    std::uint64_t last{};
} aim_motion;
std::atomic<unsigned> held_triggers{}, requests{};
std::mutex target_mutex;
Target published;
std::uint32_t selected_actor{};
unsigned selected_species{};
std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Orbit {
    std::uint32_t root{}, table{};
    unsigned area{}, preset{};
    float elevation{}, yaw_remainder{};
    float smooth_yaw{}, smooth_pitch{};
    std::uint16_t yaw{};
    bool initialized{}, pitch_initialized{};
    float dt{}, transition_time{.2f};
    math::Geometry previous_geometry{}, transition_from{};
    math::Framing framing{math::Framing::Normal};
    bool geometry_initialized{}, native_pitch{true};
} orbit;
bool valid(std::uint32_t p, unsigned bytes) {
    return !(p & 3u) && p >= 0x08800000u && p <= 0x0A000000u - bytes;
}
float number(const psprecomp::AotFastView &m, std::uint32_t p) {
    return std::bit_cast<float>(m.aot_load32(p));
}
std::uint32_t context(const psprecomp::AotFastView &m) {
    if (!enabled.load() || m.aot_load32(0x08887614u) != 0x86820080u ||
        m.aot_load32(0x08887834u) != 0xC6600200u ||
        m.aot_load32(0x09A6F490u) != 0x27BDFFF0u) return 0;
    const auto state = m.aot_load32(0x089CC558u), r = m.aot_load32(0x089CBD04u);
    if (!valid(state, 0x430) || !valid(r, 0xB00)) return 0;
    // Decimal 46 is the byte used by USA quest-state stores (not offset 0x46).
    const auto phase = m.aot_load8(state + 46);
    if ((phase != 2 && phase != 3) || m.aot_load8(r + 0xA8D) || m.aot_load8(r + 0x961) ||
        m.aot_load8(r + 0xB0 + 0x91) != 1) return 0;
    const auto hunter = m.aot_load32(r + 0xA84), c = r + 0x150;
    if (!valid(hunter, 0x700)) return 0;
    const auto table = m.aot_load32(c + 0x6C);
    const auto preset = m.aot_load8(r + 0x14);
    if (!valid(table, 0xC0) || preset > 4) return 0;
    const auto selected = m.aot_load32(c + 0x70);
    if (selected < table || selected > table + 160 || (selected - table) % 32) return 0;
    return r;
}
struct Parameters { float height{}, distance{}, focus{}, angle{}; };
bool parameters(const psprecomp::AotFastView &m, std::uint32_t table, std::array<Parameters, 5> &p) {
    for (unsigned i = 0; i < p.size(); ++i) {
        const auto record = table + (i ? i * 32 : 160);
        auto &v = p[i];
        v.height = number(m, record + 4); v.distance = number(m, record + 8); v.focus = number(m, record + 16);
        if (!std::isfinite(v.height) || !std::isfinite(v.distance) || !std::isfinite(v.focus) ||
            std::abs(v.height) > 2000 || v.distance < 1 || v.distance > 3000 || std::abs(v.focus) > 2000) return false;
        v.angle = std::atan2(v.height - v.focus, v.distance) * (180.0f / 3.14159265f);
    }
    return true;
}
void reset() {
    orbit = {}; aim_motion={}; was_aiming=false; camera::discard(); requests.store(0);
    selected_actor = 0; selected_species = 0;
    std::lock_guard lock(target_mutex); published = {};
}
struct Candidate { std::uint32_t actor{}; unsigned species{}; float x{}, y{}, z{}; double distance{}; };
Candidate select_target(const psprecomp::AotFastView &m, std::uint32_t hunter, unsigned area, bool cycle) {
    std::array<Candidate, 20> candidates{};
    unsigned count = 0;
    const auto manager = m.aot_load32(0x09C122B0u);
    const float hx = number(m, hunter + 0x200), hy = number(m, hunter + 0x204), hz = number(m, hunter + 0x208);
    if (valid(manager, 4724) && std::isfinite(hx) && std::isfinite(hy) && std::isfinite(hz)) {
        for (unsigned slot = 0; slot < 20; ++slot) {
            const auto actor = m.aot_load32(manager + 4640 + slot * 4);
            if (!valid(actor, 0x420) || actor == hunter || !m.aot_load32(actor) ||
                m.aot_load16(actor + 0x29A) != area) continue;
            const auto species = m.aot_load8(actor + 0x1E8);
            const int hp = static_cast<std::int16_t>(m.aot_load16(actor + 0x2E4));
            const auto maximum = m.aot_load16(actor + 0x41E);
            if (!monster_hp::large_monster_name(species) || hp <= 0 || !maximum || maximum > 32767 || hp > maximum) continue;
            if (std::any_of(candidates.begin(), candidates.begin() + count,
                [actor](const Candidate &v) { return v.actor == actor; })) continue;
            const float x = number(m, actor + 0x200), y = number(m, actor + 0x204), z = number(m, actor + 0x208);
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                std::abs(x) > 10000000 || std::abs(y) > 10000000 || std::abs(z) > 10000000) continue;
            const double dx = double(x) - hx, dy = double(y) - hy, dz = double(z) - hz;
            candidates[count++] = {actor, species, x, y, z, dx*dx + dy*dy + dz*dz};
        }
    }
    // Explicit developer fixture for native input/projection checks in an
    // empty camp. No fake actors enter the guest registry or receive writes.
    static const bool preview = [] {
        const auto *s = std::getenv("MHFU_TARGET_CAMERA_PREVIEW");
        return s && s[0] == '1' && !s[1];
    }();
    if (preview && std::isfinite(hx) && std::isfinite(hy) && std::isfinite(hz)) {
        count = 2;
        candidates[0] = {1, 1, hx + 1000, hy, hz, 1000000};
        candidates[1] = {2, 11, hx, hy, hz - 2000, 4000000};
    }
    std::sort(candidates.begin(), candidates.begin() + count, [](const Candidate &a, const Candidate &b) {
        return a.distance == b.distance ? a.actor < b.actor : a.distance < b.distance;
    });
    unsigned index = 0;
    for (; index < count; ++index)
        if (candidates[index].actor == selected_actor && candidates[index].species == selected_species) break;
    if (index == count) index = 0; // Acquire nearest; keep it stable until cycled or lost.
    else if (cycle && count > 1) index = (index + 1) % count;
    const Candidate result = count ? candidates[index] : Candidate{};
    static const bool trace = std::getenv("MHFU_TARGET_CAMERA_TRACE") != nullptr;
    if (trace && (selected_actor != result.actor || selected_species != result.species || cycle)) {
        std::printf("[target-camera] selected=%08X species=%u area=%u candidates=%u cycle=%u\n",
            result.actor, result.species, area, count, cycle);
        std::fflush(stdout);
    }
    selected_actor = result.actor; selected_species = result.species;
    { std::lock_guard lock(target_mutex); published = {result.actor, result.species, area, now_ns()}; }
    return result;
}
}
void set_enabled(bool on) { enabled.store(on); active.store(false); target_controls.store(false); held_triggers.store(3); last_supported=0; orbit_available=false; reset(); }
void set_controls(const Controls &value) {
    horizontal_sensitivity.store(std::clamp(std::isfinite(value.horizontal_sensitivity) ? value.horizontal_sensitivity : 1.f, .25f, 2.f));
    vertical_sensitivity.store(std::clamp(std::isfinite(value.vertical_sensitivity) ? value.vertical_sensitivity : 1.f, .25f, 2.f));
    smoothing_strength.store(std::clamp(std::isfinite(value.smoothing) ? value.smoothing : 0.f, 0.f, 100.f));
    invert_horizontal.store(value.invert_horizontal); invert_vertical.store(value.invert_vertical);
    targeting_enabled.store(value.targeting);
    aim_enabled.store(value.aim_enabled);aim_invert_x.store(value.aim_invert_horizontal);aim_invert_y.store(value.aim_invert_vertical);
    const auto bounded=[](float x,float fallback,float low,float high){return std::clamp(std::isfinite(x)?x:fallback,low,high);};
    aim_sensitivity.store(bounded(value.aim_sensitivity,1.f,.25f,2.f));
    aim_curve.store(bounded(value.aim_curve,2.f,1.f,3.f));
    aim_smoothing.store(bounded(value.aim_smoothing,30.f,0.f,100.f));
    aim_deadzone.store(bounded(value.aim_deadzone,.12f,.05f,.4f));
    response_curve.store(std::clamp(std::isfinite(value.response_curve)?value.response_curve:1.8f,1.f,3.f));
    pitch_min.store(std::clamp(std::isfinite(value.pitch_min)?value.pitch_min:-20.f,-45.f,0.f));
    pitch_max.store(std::clamp(std::isfinite(value.pitch_max)?value.pitch_max:70.f,30.f,85.f));
    if (!value.targeting) { target_controls.store(false); requests.store(0); std::lock_guard lock(target_mutex); published = {}; }
}
Controls controls() { return {horizontal_sensitivity.load(), vertical_sensitivity.load(), smoothing_strength.load(),
    invert_horizontal.load(), invert_vertical.load(), targeting_enabled.load(),
    response_curve.load(), pitch_min.load(), pitch_max.load(),aim_enabled.load(),
    aim_sensitivity.load(),aim_curve.load(),aim_smoothing.load(),aim_deadzone.load(),aim_invert_x.load(),aim_invert_y.load()}; }
Target target() {
    if (!targeting_enabled.load()) return {};
    std::lock_guard lock(target_mutex);
    return now_ns() - published.timestamp < 500000000u ? published : Target{};
}
unsigned triggers(bool left, bool right, bool available) {
    if (!available) { held_triggers.store(3); requests.store(0); return 0; }
    const unsigned held = (left ? 1u : 0u) | (right ? 2u : 0u);
    const unsigned pressed = held & ~held_triggers.exchange(held);
    if (!enabled.load() || !target_controls.load() || !targeting_enabled.load()) return 0;
    if (orbit_available) requests.fetch_or(pressed);
    return 3; // Reserve both extras, including when no target is present.
}
bool driving() { return enabled.load() && (active.load() || (aim_controls && camera_aim::aiming())); }
void configure_aim_input(float speed,float deadzone,bool invert_x,bool invert_y) {
    aim_speed=std::isfinite(speed)?std::clamp(speed,0.f,1000.f):90.f;
    input_deadzone=std::isfinite(deadzone)?std::clamp(deadzone,0.f,.99f):.15f;input_invert_x=invert_x;input_invert_y=invert_y;
}
camera_aim::AimStep game_camera_aim(std::uint32_t hunter,camera_aim::AimRoute route,float dt) {
    const auto clear=[] {aim_motion={};return camera_aim::AimStep{};};
    if(!enabled.load() || !aim_enabled.load() || !aim_controls || !aim_memory ||
       (route!=camera_aim::AimRoute::bow && route!=camera_aim::AimRoute::bowgun) ||
       !std::isfinite(dt) || dt<=0)return clear();
    const auto &m=*aim_memory;
    const auto state=m.aot_load32(0x089CC558u),root=m.aot_load32(0x089CBD04u);
    if(!valid(state,0x6ADE3) || !valid(root,0xB00) || !valid(hunter,0x700) ||
       m.aot_load32(root+0xA84)!=hunter || (m.aot_load8(state+46)!=2 && m.aot_load8(state+46)!=3))return clear();
    const auto now=now_ns();
    if(aim_motion.hunter!=hunter || aim_motion.route!=route || now-aim_motion.last>=100000000)aim_motion={};
    aim_motion.hunter=hunter;aim_motion.route=route;aim_motion.last=now;
    auto stick=camera::rate(camera::Source::Stick);
    if(!std::isfinite(stick.yaw) || !std::isfinite(stick.pitch))return clear();
    // Undo the host orbit inversion; aim has its own inversion controls.
    if(input_invert_x)stick.yaw=-stick.yaw;if(input_invert_y)stick.pitch=-stick.pitch;
    const float length=std::hypot(stick.yaw,stick.pitch);
    const float extra=std::max(0.f,(aim_deadzone.load()-input_deadzone)/(1-input_deadzone));
    if(length<=extra)stick={};
    else {const float scale=std::min(1.f,(length-extra)/(1-extra))/length;stick.yaw*=scale;stick.pitch*=scale;}
    // Match the game's A/B1 or B2 invert byte. No input/repeat bits are written.
    const bool nub=route==camera_aim::AimRoute::bowgun && !(m.aot_load32(hunter+0x614)&0x1000);
    const unsigned invert=m.aot_load8(state+(nub?0x6ADDF:0x6ADE2));
    const float xsign=(invert==2?-1.f:1.f)*(aim_invert_x.load()?-1.f:1.f);
    const float ysign=(invert==1 || invert==2?1.f:-1.f)*(aim_invert_y.load()?-1.f:1.f);
    const float speed=aim_speed*aim_sensitivity.load(),tau=aim_smoothing.load()*.0015f;
    const float seconds=std::min(dt,.1f);
    const float yaw=math::travel(aim_motion.yaw_velocity,math::curve(stick.yaw,aim_curve.load())*speed*xsign,seconds,tau);
    const float pitch=math::travel(aim_motion.pitch_velocity,math::curve(stick.pitch,aim_curve.load())*speed*ysign,seconds,tau);
    const auto quantise=[](float value,float &remainder) {
        const float total=value+remainder;const auto units=static_cast<std::int32_t>(total);remainder=total-units;return units;
    };
    // Aim-camera pitch angle advances by 34 angle units for each stored pitch unit.
    const int y=quantise(yaw*(65536.f/360.f),aim_motion.yaw_remainder);
    int p=quantise(pitch*(65536.f/(360.f*34.f)),aim_motion.pitch_remainder);
    const int current=static_cast<std::int8_t>(m.aot_load8(hunter+0x67E));
    const int limited=std::clamp(current+p,-100,100)-current;
    if(limited!=p) {aim_motion.pitch_remainder=0;aim_motion.pitch_velocity=0;}p=limited;
    return {y,p};
}
void prepare(const psprecomp::AotFastView &m, bool controls_enabled) {
    aim_memory=m;
    if(camera_aim::aiming())was_aiming=true;
    const auto state = m.aot_load32(0x089CC558u);
    const bool quest = enabled.load() && m.aot_load32(0x09A6F490u) == 0x27BDFFF0u && valid(state, 0x430) &&
        (m.aot_load8(state + 46) == 2 || m.aot_load8(state + 46) == 3);
    // Extras must not leak to PSP attacks/recenter while a quest's camera is
    // temporarily suspended (aiming, menus, special/scripted camera states).
    aim_controls=controls_enabled && quest;
    target_controls.store(quest && targeting_enabled.load());
    const bool supported = controls_enabled && context(m);
    orbit_available=supported && !camera_aim::aiming();
    const auto now=now_ns();
    const auto root=m.aot_load32(0x089CBD04u);
    // A recognised quest keeps the stick captured through aim/menu/special
    // states. Grace covers a temporarily unavailable root, never a town exit.
    const bool quest_root=quest && valid(root,0xB00);
    if(supported || quest_root)last_supported=now;
    active.store(controls_enabled && quest && (quest_root || math::within_grace(now,last_supported)));
    if(!supported) {
        camera::discard(); requests.store(0);
        orbit.smooth_yaw=orbit.smooth_pitch=0;
        if(!active.load()) {last_supported=0;reset();}
        else {std::lock_guard lock(target_mutex);published={};}
    }
}
void yaw(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    if (!driving()) return;
    if(camera_aim::aiming()) {
        was_aiming=true;orbit.smooth_yaw=orbit.smooth_pitch=0;orbit.yaw_remainder=0;
        camera::discard();requests.store(0);return;
    }
    if(was_aiming) {
        was_aiming=false;orbit.native_pitch=true;orbit.pitch_initialized=false;
        orbit.geometry_initialized=false;camera::discard();requests.store(0);
    }
    const auto r = context(m), c = r + 0x150;
    if (!r || ctx.gpr[20] != c) { camera::discard(); requests.store(0); return; }
    std::array<Parameters, 5> p;
    const auto table = m.aot_load32(c + 0x6C);
    const auto preset = m.aot_load8(r + 0x14);
    const bool pitch_valid = parameters(m, table, p);
    const auto held = m.aot_load16(c + 0x84);
    const auto area = m.aot_load16(m.aot_load32(r + 0xA84) + 0x29A);
    if (!orbit.initialized || orbit.root != r || orbit.table != table || orbit.area != area) {
        selected_actor = 0; selected_species = 0;
        orbit = {};
        orbit.root = r; orbit.table = table; orbit.area = area; orbit.preset = preset;
        orbit.yaw = m.aot_load16(c + 0x80); orbit.initialized = true;
        orbit.pitch_initialized = pitch_valid;
        if (pitch_valid) orbit.elevation = p[preset].angle;
        camera::discard(); // Enter from the game's current camera, without queued scene motion.
    }
    // Terrain can change the native vertical preset without a button press.
    // Only explicit D-pad/L control may replace the player's chosen elevation.
    if (pitch_valid && (!orbit.pitch_initialized || (orbit.preset != preset && (held & 0x150)))) {
        orbit.elevation = p[preset].angle; orbit.pitch_initialized = true;
        orbit.native_pitch=true;
    }
    orbit.preset = preset;
    orbit.yaw = m.aot_load16(c + 0x80); // Accept wall, obstruction, follow and L corrections.
    const auto turn = camera::take();
    orbit.dt=turn.seconds;
    const auto request = requests.exchange(0);
    const auto hunter = m.aot_load32(r + 0xA84);
    const auto hunter_state=m.aot_load8(hunter+0x29C);
    if(hunter_state!=0 && hunter_state!=3) {
        orbit.smooth_yaw=orbit.smooth_pitch=0;
        return; // Preserve special-state orientation as well as its framing.
    }
    const auto monster = targeting_enabled.load() ? select_target(m, hunter, area, (request & 2u) != 0) : Candidate{};
    if ((request & 1u) && monster.actor) {
        const float dx = monster.x - number(m, hunter + 0x200);
        const float dz = monster.z - number(m, hunter + 0x208);
        if (std::hypot(dx, dz) > .01f) {
            // Native orbit offset rotates +Z around the hunter; face forward
            // toward the target by placing the camera on the opposite side.
            const auto angle = static_cast<std::uint16_t>(static_cast<int>(std::lround(
                std::atan2(-dx, -dz) * (65536.0f / (2 * 3.14159265f)))));
            m.aot_store16(c + 0x80, angle); m.aot_store16(c + 0x82, angle);
            orbit.yaw = angle;
            if (pitch_valid) {
                const float focus = number(m, hunter + 0x204) + p[preset].focus;
                orbit.elevation = std::clamp(std::atan2(focus - (monster.y + 100), std::hypot(dx, dz)) *
                    (180.0f / 3.14159265f), pitch_min.load(), pitch_max.load());
                orbit.pitch_initialized = true;
                orbit.native_pitch=false;
            }
            orbit.yaw_remainder = 0;
            orbit.smooth_yaw = orbit.smooth_pitch = 0;
            if (std::getenv("MHFU_TARGET_CAMERA_TRACE")) {
                std::printf("[target-camera] snap=%08X yaw=%u elevation=%.3f\n", monster.actor, angle, orbit.elevation);
                std::fflush(stdout);
            }
        }
        return; // One snap, no follow, turn, attack, selection change or zoom.
    }
    // Physical D-pad and L keep their original actions and take priority.
    const float yaw_scale = horizontal_sensitivity.load() * (invert_horizontal.load() ? -1.f : 1.f);
    const float pitch_scale = vertical_sensitivity.load() * (invert_vertical.load() ? -1.f : 1.f);
    const float strength = smoothing_strength.load();
    const auto shaped=[&](float all,float stick) {
        return turn.full_scale_degrees>0 ? all-stick+turn.full_scale_degrees*
            math::curve(stick/turn.full_scale_degrees,response_curve.load()):all;
    };
    const float requested_yaw=shaped(turn.yaw_degrees,turn.stick_yaw_degrees)*yaw_scale;
    const float requested_pitch=shaped(turn.pitch_degrees,turn.stick_pitch_degrees)*.5f*pitch_scale;
    const auto move=[&](float &velocity,float delta) {
        if(!std::isfinite(delta))return 0.f;
        return turn.seconds>0 ? math::travel(velocity,delta/turn.seconds,turn.seconds,strength*.0015f):delta;
    };
    const float yaw_delta=move(orbit.smooth_yaw,requested_yaw);
    const float pitch_delta=move(orbit.smooth_pitch,requested_pitch);
    if (!(held & 0x1A0) && std::isfinite(orbit.smooth_yaw)) {
        const float units = orbit.yaw_remainder - yaw_delta * (65536.0f / 360.0f);
        const int delta = static_cast<int>(std::clamp(units, -65536.0f, 65536.0f));
        orbit.yaw_remainder = units - delta;
        if (delta) {
            const auto target = static_cast<std::uint16_t>(m.aot_load16(c + 0x80) + delta);
            orbit.yaw = target;
            m.aot_store16(c + 0x80, target);
            // Apply the same delta to displayed yaw: responsive input without
            // changing the original recenter/collision smoothing difference.
            m.aot_store16(c + 0x82, static_cast<std::uint16_t>(m.aot_load16(c + 0x82) + delta));
        }
    } else {orbit.yaw_remainder = 0;orbit.smooth_yaw=0;}
    if (pitch_valid && !(held & 0x150) && std::isfinite(orbit.smooth_pitch)) {
        if(pitch_delta!=0) {
            orbit.elevation=math::pitch_step(orbit.elevation,pitch_delta,pitch_min.load(),pitch_max.load());
            orbit.native_pitch=false;
        }
    } else if(pitch_valid) {
        orbit.smooth_pitch=0;orbit.elevation=p[preset].angle;orbit.native_pitch=true;
    }
    static const bool trace = std::getenv("MHFU_MODERN_CAMERA_TRACE") != nullptr;
    if (trace && (turn.yaw_degrees || turn.pitch_degrees)) {
        std::printf("[modern-camera] area=%u yaw=%u elevation=%.3f request=%.3f,%.3f\n", area,
                    m.aot_load16(c + 0x80), orbit.elevation, turn.yaw_degrees, turn.pitch_degrees);
        std::fflush(stdout);
    }
}
void geometry(const psprecomp::AllegrexContext &ctx, const psprecomp::AotFastView &m) {
    if (camera_aim::aiming() || !driving() || !orbit.initialized || !orbit.pitch_initialized) return;
    const auto r = context(m), c = r + 0x150;
    if (!r || orbit.root != r || ctx.gpr[20] != c || !valid(ctx.gpr[29], 464)) return;
    const auto hunter = m.aot_load32(r + 0xA84);
    if (ctx.gpr[19] != hunter) return;
    const auto record=m.aot_load32(c+0x70);
    const float distance=number(m,record+8), preset_focus=number(m,record+16);
    const float hy=number(m,hunter+0x204);
    const math::Geometry native{number(m,ctx.gpr[29]+340),number(m,ctx.gpr[29]+344)};
    const float native_focus=number(m,c+0x54)-hy;
    if(!std::isfinite(distance) || distance<1 || distance>3000 ||
       !std::isfinite(preset_focus) || !std::isfinite(hy) ||
       !std::isfinite(native.height) || !std::isfinite(native.distance))return;
    const unsigned state=m.aot_load8(hunter+0x29C);
    const auto framing=math::framing(state,native.height,native_focus);
    const bool excluded=framing==math::Framing::Excluded;
    const float f=excluded?native_focus:framing==math::Framing::Low?30.f:preset_focus;
    auto desired=(excluded || orbit.native_pitch || (m.aot_load16(c+0x84)&0x150))
        ?native:math::geometry(orbit.elevation,distance,f);
    if(!orbit.geometry_initialized) {
        orbit.previous_geometry=native;orbit.transition_from=native;
        orbit.framing=framing;orbit.geometry_initialized=true;
    } else if(framing!=orbit.framing) {
        orbit.transition_from=orbit.previous_geometry;orbit.transition_time=0;
        orbit.framing=framing;
        if(std::getenv("MHFU_MODERN_CAMERA_TRACE"))
            std::printf("[modern-camera] state=%u framing=%u native-height=%.3f native-focus=%.3f\n",
                state,unsigned(framing),native.height,native_focus);
    }
    // Vanilla focus remains exact; only temporary height/distance ease on a
    // state change. After 200 ms excluded states are wholly vanilla again.
    if(orbit.transition_time<.2f && !(m.aot_load16(c+0x84)&0x150)) {
        orbit.transition_time=std::min(.2f,orbit.transition_time+orbit.dt);
        const float t=orbit.transition_time/.2f, blend=t*t*(3-2*t);
        desired.height=orbit.transition_from.height+(desired.height-orbit.transition_from.height)*blend;
        desired.distance=orbit.transition_from.distance+(desired.distance-orbit.transition_from.distance)*blend;
    }
    orbit.previous_geometry=desired;
    const float height=desired.height, horizontal_distance=desired.distance, focus=hy+f;
    if(!std::isfinite(height) || !std::isfinite(horizontal_distance) || !std::isfinite(focus))return;
    // Original routine's temporary geometry, before trigonometry and native
    // environment queries. No shared preset table or instruction is modified.
    m.aot_store32(ctx.gpr[29] + 340, std::bit_cast<std::uint32_t>(height));
    m.aot_store32(ctx.gpr[29] + 344, std::bit_cast<std::uint32_t>(horizontal_distance));
    m.aot_store32(c + 0x54, std::bit_cast<std::uint32_t>(focus));
}
}
