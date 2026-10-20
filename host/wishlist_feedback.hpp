#pragma once
#include <string>
#include <optional>
#include <cmath>
#include <algorithm>
#include <utility>
namespace portablekit::fu::wishlist {
struct PinFeedback {bool pinned{};std::string equipment;};
std::optional<PinFeedback> take_pin_feedback();
bool has_pin_feedback();
class FeedbackTimer {
    PinFeedback feedback_;double age_{3};
public:
    void show(PinFeedback value){feedback_=std::move(value);age_=0;}
    void advance(double seconds){if(std::isfinite(seconds))age_+=std::max(0.,seconds);}
    bool visible() const{return age_<3;}
    double age() const{return age_;}
    const PinFeedback &value() const{return feedback_;}
    void clear(){age_=3;}
};
}
