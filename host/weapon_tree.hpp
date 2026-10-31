#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
namespace psprecomp {class AotFastView;struct AllegrexContext;}
namespace portablekit::fu::weapon_tree {
struct Material {unsigned item{},quantity{};};
struct Weapon {
    unsigned type{},id{},category{},rarity{},power{};
    unsigned recoil{},reload{},charges{},ammo_mask{},weapon_variable{};float deviation{};
    std::array<unsigned,4> shots{};
    std::array<unsigned,32> ammo{};
    std::array<unsigned,2> elements{},values{};
    std::array<unsigned,7> sharpness{};
    std::array<Material,4> materials{};
    unsigned material_count{},recipe{},operation{};
    bool owned{};
    std::vector<unsigned> next;
};
struct Node {Weapon weapon;unsigned column{},row{};};
struct Chart {unsigned type{},selected{};std::vector<Node> nodes;std::vector<std::array<unsigned,2>> edges;};
std::optional<Weapon> read_weapon(const psprecomp::AotFastView &,unsigned type,unsigned id);
Chart build(const psprecomp::AotFastView &,unsigned type,unsigned id);
void set_enabled(bool);
void capture_menu(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &);
void capture_grid_text(const psprecomp::AllegrexContext &,const psprecomp::AotFastView &,unsigned point);
void frame(const psprecomp::AotFastView &);
std::optional<Chart> snapshot();
bool available();
bool panel(bool first);
void register_renderer();
}
