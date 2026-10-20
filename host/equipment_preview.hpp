#pragma once
#include "save_preview.hpp"
#include <span>
struct ImDrawList;
namespace psprecomp { class GuestMemory; }
namespace psprecomp { class AotFastView; }
namespace portablekit::gpu { struct DrawCall; }
namespace portablekit::fu::equipment {
// Only locally derived data from the player's running game is cached.
void catalogue_seen(std::span<const std::uint8_t> bytes,const psprecomp::AotFastView &memory);
std::string name(const EquippedItem &item);
void observe(const gpu::DrawCall &,const psprecomp::GuestMemory &);
bool needs_icons();
void draw_icon(const EquippedItem &item,float x,float y,float size,ImDrawList *draw=nullptr);
}
