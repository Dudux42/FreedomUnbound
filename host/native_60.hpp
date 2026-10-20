#pragma once
#include <cstdint>

namespace psprecomp { class Runtime; struct AllegrexContext; }
namespace portablekit::fu {
// Research mode: these hooks cover the measured main-image routines only.
bool native_60_requested();
void initialize_native_60(psprecomp::Runtime &);
std::uint32_t native_frame_threshold();
float native_animation_step(float);
std::uint32_t native_ui_clock(std::uint32_t);
void native_fade_reset(std::uint32_t);
std::uint32_t native_fade_tick(std::uint32_t);
void native_repeat_reset(std::uint32_t);
std::uint32_t native_repeat_tick(std::uint32_t);
void native_blend_reset(std::uint32_t);
void native_blend_finish(std::uint32_t);
float native_blend_increment(float, std::uint32_t);
std::uint32_t native_blend_tick(std::uint32_t);
void native_countdown_step(psprecomp::AllegrexContext &);
void native_message_reset(std::uint32_t);
void native_message_finish(std::uint32_t);
std::uint32_t native_message_tick(std::uint32_t);
float native_message_motion(float);
}
