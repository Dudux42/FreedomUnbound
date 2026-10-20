#include "damage_number_layout.hpp"
#include <limits>
#include <stdexcept>
using namespace portablekit::fu::damage_numbers;
void check(bool ok) { if (!ok) throw std::runtime_error("damage renderer regression"); }
int main() {
    Animation animation;
    animation.add(0, 10); animation.add(-1, 10);
    animation.add(1, std::numeric_limits<double>::quiet_NaN());
    check(animation.size() == 0);
    animation.add(42, 10);
    const auto number = *animation.begin();
    check(pose(number, 10).scale > 1 && pose(number, 10).alpha == 1);
    check(pose(number, 10.5).y < pose(number, 10).y);
    check(pose(number, 10.8).alpha > 0 && pose(number, 10.8).alpha < 1);
    check(pose(number, 11).alpha == 0);
    animation.expire(11); check(animation.size() == 0);
    for (int i = 1; i <= 100; ++i) animation.add(i, 20);
    check(animation.size() == 20 && animation.begin()->amount == 81);
    animation.clear(); check(animation.size() == 0);
    animation.add(1, 30, -5, 5);
    check(animation.begin()->x == 0.0f && animation.begin()->y == 1.0f);
    animation.expire(29); check(animation.size() == 0);
    animation.add_world(123, 40, {1, 2, 3}, Affinity::Positive);
    check(animation.size() == 1 && animation.begin()->positioned);
    check(animation.begin()->world[1] == 2 && animation.begin()->affinity == Affinity::Positive);
    check(pose(*animation.begin(), 40).x == 0.5f); // No global lanes shifting the hit sideways.
    check(color(Affinity::Normal).g == 190 && color(Affinity::Normal).b == 120);
    check(color(Affinity::Positive).r == 255 && color(Affinity::Negative).g == 82);
}
