#pragma once
#include "equipment_wishlist_model.hpp"
#include <filesystem>
#include <optional>
namespace psprecomp { class AotFastView; struct AllegrexContext; }
namespace portablekit::fu::wishlist {
struct Marker {float x{},y{};};
struct WishlistDisplayItem {Key key;std::string equipment;std::vector<Requirement> materials;};
struct View {
    bool quest{},smithy{},pinned{},finish_notices{},village{};
    std::string hunter;
    std::optional<Recipe> selected;
    std::vector<Marker> markers;
    std::vector<Notice> notices;
    std::vector<WishlistDisplayItem> wishlist;
};
void set_enabled(bool,const std::filesystem::path &storage={});
void select_slot(unsigned);
unsigned triggers(bool left,bool right,bool available);
void frame(const psprecomp::AotFastView &);
void capture_menu(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &);
void capture_craft(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &);
void capture_material(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &,bool upgrade);
void capture_text(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &,unsigned point);
View view(bool consume=false);
void item_name(unsigned,const std::string &);
std::string item_name(unsigned);
bool pinned(const Key &);
bool toggle_pin(Recipe);
bool take_crafted_notice();
bool has_crafted_notice();
bool panel(bool first_frame);
// UI-thread extension point for a future controller-presentation provider.
// draw(x,y,size) may render a controller glyph instead of the fallback label.
struct ControlHint { std::string label; void (*draw)(float,float,float){}; float width{}; };
struct ControlHints { ControlHint pin{"ZR / RT / R2"},unpin{"ZL / LT / L2"}; };
using ControlHintProvider=ControlHints (*)();
void set_control_hint_provider(ControlHintProvider);
void register_renderer();
}
