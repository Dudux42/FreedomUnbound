#pragma once
#include <cstdint>
#include "camera_aim.hpp"
namespace psprecomp { class AotFastView; class Runtime; struct AllegrexContext; }
namespace portablekit { struct CameraDriver; }
namespace portablekit::fu::modern_camera {
struct Target { std::uint32_t identity{}; unsigned species{}, area{}; std::uint64_t timestamp{}; };
struct Controls {
    float horizontal_sensitivity{1.f}, vertical_sensitivity{1.f}, smoothing{};
    bool invert_horizontal{}, invert_vertical{}, targeting{true};
    float response_curve{1.8f}, pitch_min{-20.f}, pitch_max{70.f};
    // Independent ranged-aim settings; exposed only while the hook is armed.
    bool aim_enabled{true};
    float aim_sensitivity{1.f}, aim_curve{2.f}, aim_smoothing{30.f}, aim_deadzone{.12f};
    bool aim_invert_horizontal{}, aim_invert_vertical{};
};
void configure_aim_input(float speed,float base_deadzone,bool input_invert_x,bool input_invert_y);
camera_aim::AimStep game_camera_aim(std::uint32_t hunter,camera_aim::AimRoute route,float dt);
Target target();
unsigned triggers(bool left, bool right, bool available = true);
void set_enabled(bool);
void set_controls(const Controls &);
Controls controls();
void prepare(const psprecomp::AotFastView &, bool controls_enabled = true);
bool driving();
void yaw(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void geometry(const psprecomp::AllegrexContext &, const psprecomp::AotFastView &);
void register_mod();
extern const CameraDriver driver;
}
