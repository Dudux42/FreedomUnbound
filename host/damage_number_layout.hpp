#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "damage_events.hpp"

namespace portablekit::fu::damage_numbers {
using Affinity = damage_events::Affinity;
struct Color { int r, g, b; };
inline constexpr Color color(Affinity affinity) {
    return affinity == Affinity::Negative ? Color{255, 82, 82} : Color{255, 190, 120};
}
struct Number {
    int amount{};
    double born{};
    float x{0.5f}, y{0.46f};
    unsigned lane{};
    std::array<float, 3> world{};
    Affinity affinity{};
    bool positioned{};
};
struct Pose { float x{}, y{}, alpha{}, scale{}; };
inline constexpr double lifetime = 0.9;
inline Pose pose(const Number &number, double now) {
    const double elapsed = std::clamp(now - number.born, 0.0, lifetime);
    const float age = static_cast<float>(elapsed);
    const float alpha = std::clamp(static_cast<float>((lifetime - elapsed) / 0.25), 0.0f, 1.0f);
    const float pop = 1.0f + 0.2f * std::max(0.0f, 1.0f - age / 0.12f);
    return {number.x, number.y - age * 0.08f, alpha, pop};
}
class Animation {
public:
    void clear() { count_ = 0; next_lane_ = 0; }
    void expire(double now) {
        std::size_t keep = 0;
        for (std::size_t i = 0; i < count_; ++i)
            if (now >= numbers_[i].born && now - numbers_[i].born < lifetime)
                numbers_[keep++] = numbers_[i];
        count_ = keep;
    }
    void add(int amount, double born, float x = 0.5f, float y = 0.46f, Affinity affinity = Affinity::Normal) {
        if (amount <= 0 || !std::isfinite(born) || !std::isfinite(x) || !std::isfinite(y)) return;
        expire(born);
        if (count_ == numbers_.size()) {
            std::move(numbers_.begin() + 1, numbers_.end(), numbers_.begin());
            --count_;
        }
        numbers_[count_++] = {amount, born, std::clamp(x, 0.0f, 1.0f),
                              std::clamp(y, 0.0f, 1.0f), next_lane_++ % 20, {}, affinity, false};
    }
    void add_world(int amount, double born, const std::array<float, 3> &world, Affinity affinity) {
        if (amount <= 0 || !std::isfinite(born)) return;
        for (float v : world) if (!std::isfinite(v) || std::abs(v) > 1e7f) return;
        add(amount, born, 0.5f, 0.5f, affinity);
        numbers_[count_ - 1].world = world;
        numbers_[count_ - 1].positioned = true;
    }
    const Number *begin() const { return numbers_.data(); }
    const Number *end() const { return numbers_.data() + count_; }
    std::size_t size() const { return count_; }
private:
    std::array<Number, 20> numbers_{};
    std::size_t count_{};
    unsigned next_lane_{};
};
}
