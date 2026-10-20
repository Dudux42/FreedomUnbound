#pragma once
#include <string>
#include <string_view>
#include "input/bindings.hpp"
namespace portablekit::fu::controller_prompts {
enum class Family { Keyboard, Xbox, PlayStation, Nintendo, Generic };
enum class Action { ExtraLeft, ExtraRight };
inline std::string label(Family family, Action action, std::string_view keyboard) {
    const bool right=action==Action::ExtraRight;
    switch(family) {
    case Family::Keyboard:return keyboard.empty() ? "Unbound" : std::string(keyboard);
    case Family::Nintendo:return right ? "ZR" : "ZL";
    case Family::PlayStation:return right ? "R2" : "L2";
    case Family::Xbox:return right ? "RT" : "LT";
    default:return right ? "RT" : "LT";
    }
}
// UI-thread API for mod overlays. Resolves live device and current bindings.
std::string current_label(Action);
Family current_family();
// Actual keyboard or controller assignment for a native PSP control.
std::string mapped_label(input::Action);
float draw(Action,float x,float y,float size);
void register_renderer();
}
