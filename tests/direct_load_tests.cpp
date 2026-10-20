#include "direct_load_policy.hpp"
#include <iostream>
#include <stdexcept>
using namespace portablekit::fu::direct_load;
void require(bool value) { if(!value) throw std::runtime_error("direct-load navigation check failed"); }
std::uint32_t advance(Navigation &n,MenuState state) {
    for(unsigned i=0;i<60;++i) { const auto buttons=n.step(state); n.finish_frame(); if(buttons) return buttons; }
    return 0;
}
int main() {
    SelectorObserver observer;
    require(observer.step(5,0,true,false).screen==MenuState::Screen::Waiting);
    require(observer.step(6,0,true,false).screen==MenuState::Screen::Waiting);
    require(observer.step(3,1,true,false).screen==MenuState::Screen::Waiting);
    require(observer.step(3,1,true,true).screen==MenuState::Screen::Select);
    require(observer.step(5,1,true,true).screen==MenuState::Screen::Notice);
    require(observer.step(6,1,true,true).screen==MenuState::Screen::Accepted);
    portablekit::fu::HunterSummary a,b;
    a.occupied=b.occupied=true; a.name=b.name="Hunter"; a.appearance_key=b.appearance_key=std::string(64,'a');
    require(same_hunter(a,b));
    b.zenny=500; require(same_hunter(a,b));
    b.name="Other"; require(!same_hunter(a,b)); b=a;
    b.appearance_key[0]='b'; require(!same_hunter(a,b)); b=a;
    b.occupied=false; require(!same_hunter(a,b)); b=a;
    a.appearance_key=b.appearance_key=""; require(!same_hunter(a,b));
    for(unsigned slot=0;slot<3;++slot) {
        Navigation n(slot);
        require(advance(n,{MenuState::Screen::Waiting})==0);
        require(advance(n,{MenuState::Screen::Boot})==0x8);
        unsigned selected=2;
        while(selected!=slot) { require(advance(n,{MenuState::Screen::Select,selected})==0x10); --selected; }
        require(advance(n,{MenuState::Screen::Select,selected})==0x4000);
        require(advance(n,{MenuState::Screen::Confirm,selected,false})==0x80);
        require(advance(n,{MenuState::Screen::Confirm,selected,true})==0x4000);
        require(advance(n,{MenuState::Screen::Notice,selected})==0x4000);
        require(advance(n,{MenuState::Screen::Accepted,selected,true})==0);
        require(n.done());
        require(advance(n,{MenuState::Screen::Boot})==0);
    }
    Navigation down(2);
    require(advance(down,{MenuState::Screen::Select,0})==0x40);
    Navigation invalid(3);
    require(advance(invalid,{MenuState::Screen::Boot})==0 && invalid.failed());
    Navigation unexpected(0);
    require(advance(unexpected,{MenuState::Screen::Unexpected})==0 && unexpected.failed());
    Navigation invalid_cursor(0);
    require(advance(invalid_cursor,{MenuState::Screen::Select,3})==0 && invalid_cursor.failed());
    Navigation wrong_confirm(0);
    advance(wrong_confirm,{MenuState::Screen::Select,0});
    require(advance(wrong_confirm,{MenuState::Screen::Confirm,1,true})==0 && wrong_confirm.failed());
    Navigation returned(0);
    advance(returned,{MenuState::Screen::Select,0});
    require(advance(returned,{MenuState::Screen::Boot})==0 && returned.failed());
    Navigation premature(0);
    require(advance(premature,{MenuState::Screen::Accepted,0})==0 && premature.failed());
    Navigation edges(0);
    unsigned held=0,gaps=0;
    for(unsigned i=0;i<80;++i) {
        const auto b=edges.step({MenuState::Screen::Boot}); edges.finish_frame();
        if(b) { ++held; require(held<=4); } else { if(held) ++gaps; held=0; }
    }
    require(gaps>=2);
    std::cout<<"Direct-load navigation checks passed\n";
}
