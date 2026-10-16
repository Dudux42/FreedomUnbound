#pragma once
#include <cstdint>
#include "save_preview.hpp"
namespace portablekit::fu::direct_load {
inline bool same_hunter(const HunterSummary &actual,const HunterSummary &expected) {
    return actual.occupied && expected.occupied && !actual.name.empty() && actual.name==expected.name &&
        actual.appearance_key.size()==64 && actual.appearance_key==expected.appearance_key;
}
// Only guarded, interactive menu states may generate input. A transition is
// followed by a neutral gap so one press cannot confirm a different screen.
struct MenuState {
    enum class Screen { Waiting, Boot, Select, Confirm, Notice, Accepted, Unexpected } screen=Screen::Waiting;
    unsigned selected=3;
    bool yes=false;
};
class SelectorObserver {
public:
    MenuState step(unsigned state,unsigned cursor,bool mode,bool model_valid) {
        // A newly installed Continue overlay briefly retains the previous
        // task's object. Arm only after its real save/model buffer is ready.
        if(state==3 && mode && model_valid) ready_=true;
        if(!ready_) return {};
        MenuState result; result.selected=cursor;
        if(state==3 && mode && model_valid) result.screen=MenuState::Screen::Select;
        else if(state==4) { result.screen=MenuState::Screen::Confirm; result.yes=mode; }
        else if(state==5) result.screen=MenuState::Screen::Notice;
        else if(state==6) result.screen=MenuState::Screen::Accepted;
        else if(state>=7) result.screen=MenuState::Screen::Unexpected;
        return result;
    }
    bool ready() const { return ready_; }
private:
    bool ready_=false;
};
class Navigation {
public:
    explicit Navigation(unsigned slot):slot_(slot) {}
    std::uint32_t step(MenuState state) {
        if(done_ || failed_) return 0;
        if(state.screen==MenuState::Screen::Unexpected || slot_>=3) { failed_=true; return 0; }
        if(state.screen==MenuState::Screen::Accepted) {
            if(!selected_ || state.selected!=slot_) failed_=true;
            else done_=true;
            return 0;
        }
        if((state.screen==MenuState::Screen::Boot && selected_) ||
           ((state.screen==MenuState::Screen::Confirm || state.screen==MenuState::Screen::Notice) && (!selected_ || state.selected!=slot_))) {
            failed_=true; return 0;
        }
        if(state.screen==MenuState::Screen::Select) selected_=true;
        const bool changed=state.screen!=previous_.screen || state.selected!=previous_.selected || state.yes!=previous_.yes;
        previous_=state;
        if(changed) { gap_=12; held_=0; }
        if(gap_) { --gap_; return 0; }
        if(held_) { --held_; return buttons_; }
        buttons_=0;
        switch(state.screen) {
        case MenuState::Screen::Boot: buttons_=0x0008; break;
        case MenuState::Screen::Select:
            if(state.selected>=3) { failed_=true; break; }
            buttons_=state.selected==slot_?0x4000:(state.selected<slot_?0x0040:0x0010); break;
        case MenuState::Screen::Confirm: buttons_=state.yes?0x4000:0x0080; break;
        case MenuState::Screen::Notice: buttons_=0x4000; break;
        default: break;
        }
        if(buttons_) { held_=3; gap_=0; cooldown_=true; }
        return buttons_;
    }
    // Called once per game flip, after step(). Finish the pulse before its gap.
    void finish_frame() {
        if(cooldown_ && held_==0) { cooldown_=false; gap_=20; }
    }
    bool done() const { return done_; }
    bool failed() const { return failed_; }
private:
    unsigned slot_, gap_=12, held_=0;
    bool done_=false, failed_=false, cooldown_=false, selected_=false;
    std::uint32_t buttons_=0;
    MenuState previous_;
};
}
