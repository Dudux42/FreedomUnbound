#pragma once
#include <cstdint>
namespace psprecomp { class AotFastView; class Runtime; struct AllegrexContext; }
namespace portablekit { struct CameraDriver; }
namespace portablekit::fu::modern_camera {
struct Target { std::uint32_t identity{}; unsigned species{}, area{}; std::uint64_t timestamp{}; };
struct Controls {
    float horizontal_sensitivity{1.f}, vertical_sensitivity{1.f}, smoothing{};
    bool invert_horizontal{}, invert_vertical{}, targeting{true};
};
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
