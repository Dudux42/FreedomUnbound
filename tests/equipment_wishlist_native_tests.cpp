#include "equipment_wishlist.hpp"
#include "psprecomp/runtime.hpp"
#include "wishlist_feedback.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/interpreter.hpp"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <source_location>
#include <stdexcept>
#include "install/user_data.hpp"
// The wishlist reads its equipment-name table from the user data directory.
// This test supplies names directly, so keep the lookup in the working directory.
std::filesystem::path portablekit::install::user_data_directory() {return std::filesystem::current_path();}
using namespace portablekit::fu::wishlist;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok){std::fprintf(stderr,"wishlist native regression at line %u\n",at.line());throw std::runtime_error("wishlist native regression");}
}
int main(int argc,char **argv) {
    psprecomp::Runtime rt;auto &m=rt.memory();const auto v=m.aot_fast_view();
    const auto directory=std::filesystem::current_path()/("fu-wishlist-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    constexpr unsigned state=0x09000000,menu=0x09700000,rows=0x09800000,name=0x09600000;
    auto text=[&](unsigned address,const char *s){do{m.store8(address++,*s);}while(*s++);};
    m.store32(0x089CC558,state);m.store8(state+0x4A0,'T');m.store8(state+46,0);
    m.store32(0x09AA5770,0x27BDFDC0);m.store32(0x09AA49F8,0x27BDFFF0);
    m.store32(menu+16,rows);m.store32(menu+24,1);m.store32(menu+64,0);m.store8(menu+37,0);m.store8(menu+32,2);
    text(name,"Iron Sword");m.store32(rows+4,name);m.store16(rows+10,5);m.store16(rows+12,1);m.store16(rows+14,1);
    constexpr unsigned recipe=0x0894944C+26;
    m.store8(recipe,5);m.store16(recipe+2,1);m.store16(recipe+4,42);m.store16(recipe+6,3);
    psprecomp::AllegrexContext c{};c.gpr[4]=menu;
    set_enabled(true,directory);capture_menu(c,v);frame(v);
    check(view().smithy && view().selected->name=="Iron Sword" && !view().pinned);
    m.store8(menu+32,1);capture_menu(c,v);frame(v);check(!view().smithy && triggers(false,true,true)==0);
    m.store8(menu+32,2);capture_menu(c,v);frame(v);
    triggers(false,false,true);check(triggers(false,true,true)==3);frame(v);check(view().pinned);
    auto feedback=take_pin_feedback();check(feedback && feedback->pinned && feedback->equipment=="Iron Sword" && !take_pin_feedback());
    check(m.load16(recipe+6)==3 && m.load16(state+0x3428)==0); // No guest writes.
    item_name(42,"Iron Ore");text(name+1024,"Iron Ore");text(name+2048,"%s");
    c.gpr[5]=240;c.gpr[6]=170;c.gpr[7]=name+2048;c.gpr[8]=name+1024;
    capture_text(c,v,0x08891474);check(view().markers.size()==1 && view().markers[0].x==232);
    view(true);check(view().markers.empty());
    // The equipment name gets a pin too, independently of material rows.
    c.gpr[7]=name;c.gpr[8]=0;capture_text(c,v,0x08891474);check(view().markers.size()==1);view(true);
    c.gpr[18]=recipe;c.gpr[16]=250;c.gpr[17]=190;capture_material(c,v,false);
    check(view().markers.size()==1 && view().markers[0].x==243);view(true);
    // Upgrade requirements come from a distinct target-indexed 28-byte record.
    constexpr unsigned upgrade=0x0894FAE4+28;
    m.store16(upgrade,43);m.store16(upgrade+2,2);m.store8(menu+37,1);
    c.gpr[4]=menu;capture_menu(c,v);frame(v);
    check(view().selected->key.operation==1 && view().selected->materials[0].item==43 && view().selected->materials[0].required==2);
    m.store8(menu+37,0);capture_menu(c,v);frame(v);
    // The second slot with an identical name has a separate persisted list.
    select_slot(1);frame(v);check(!view().pinned);select_slot(0);frame(v);check(view().pinned);
    set_enabled(false);check(triggers(false,true,true)==0 && !view().smithy);
    set_enabled(true,directory);capture_menu(c,v); // Restore actual menu pointer below.
    c.gpr[4]=menu;capture_menu(c,v);frame(v);check(view().pinned);
    // Quest-entry inventory is a baseline, then the pickup generates both notices.
    m.store32(0x09AA5770,0);m.store32(0x09A6B154,0x106005F7);m.store8(state+46,2);frame(v);
    check(view().quest && !view().smithy && view().notices.empty());
    m.store16(state+0x43C8,42);m.store16(state+0x43CA,3);frame(v);
    auto notices=view(true).notices;check(notices.size()==2 && notices[1].complete);
    check(m.load16(state+0x43CA)==3);
    m.store32(0x08A62D9C,0x09810000);m.store8(0x09810000,1);m.store8(0x09810001,2);m.store8(state+46,3);
    m.store16(state+0x43CA,1);frame(v);m.store16(state+0x43CA,3);frame(v);
    check(view(true).notices.size()==2); // Carving after quest success still counts.
    m.store8(state+46,4);frame(v);check(!view().quest && view().notices.empty());
    m.store32(0x09AA5770,0x27BDFDC0);c.gpr[4]=menu;capture_menu(c,v);frame(v);
    notices=view(true).notices;check(notices.size()==1 && notices[0].complete && view().finish_notices);
    frame(v);check(view().notices.empty());
    set_enabled(false);set_enabled(true,directory);capture_menu(c,v);frame(v);
    notices=view(true).notices;check(notices.size()==1 && notices[0].complete);
    triggers(false,false,true);triggers(true,false,true);frame(v);check(!view().pinned);
    feedback=take_pin_feedback();check(feedback && !feedback->pinned && feedback->equipment=="Iron Sword");
    // Adjacent native row flags must not become part of the one-byte type.
    for(unsigned flags: {0u,1u,128u,255u}) {
        m.store8(rows+11,flags);capture_menu(c,v);frame(v);
        check(view().smithy && view().selected->key.type==5);
    }
    triggers(false,false,true);triggers(false,true,true);frame(v);check(view().pinned);
    take_pin_feedback();
    psprecomp::AllegrexContext craft{};craft.gpr[20]=menu;craft.gpr[19]=5;craft.gpr[18]=1;
    craft.gpr[17]=65535;capture_craft(craft,v);check(view().pinned); // Failed box insertion.
    craft.gpr[17]=2;capture_craft(craft,v);check(view().pinned); // No written target.
    constexpr unsigned made=state+0x4A0+168+2*12;
    m.store8(made,1);m.store8(made+1,5);m.store16(made+2,2);
    capture_craft(craft,v);check(view().pinned); // Different equipment.
    m.store16(made+2,1);capture_craft(craft,v);check(!view().pinned);
    // Crafting raises the crafted notice instead of pin feedback, once.
    check(!take_pin_feedback() && take_crafted_notice() && !take_crafted_notice());
    set_enabled(false);set_enabled(true,directory);c.gpr[4]=menu;capture_menu(c,v);frame(v);
    check(!view().pinned); // Automatic removal persists across reload.
    // A successful upgrade removes its own target pin, not a creation pin.
    m.store8(menu+37,1);capture_menu(c,v);frame(v);
    triggers(false,false,true);triggers(false,true,true);frame(v);check(view().pinned);
    capture_craft(craft,v);check(!view().pinned);
    m.store8(menu+37,0);
    // Armor rows use the same byte type plus independent availability flags.
    constexpr unsigned armor=0x0893B938+26;
    m.store8(armor,3);m.store16(armor+2,12);m.store16(armor+4,42);m.store16(armor+6,1);
    m.store8(rows+10,3);m.store8(rows+11,1);m.store16(rows+12,12);
    capture_menu(c,v);frame(v);check(view().smithy && view().selected->key.type==3);
    triggers(false,false,true);triggers(false,true,true);frame(v);check(view().pinned);
    craft.gpr[19]=3;craft.gpr[18]=12;m.store8(made+1,3);m.store16(made+2,12);
    capture_craft(craft,v);check(!view().pinned);
    set_enabled(false);
    if(argc==2) {
        psprecomp::Elf32Image::from_file(argv[1]).load_into(m);
        m.store32(0x089CC558,state);m.store8(state+0x4A0,'T');m.store8(state+46,0);
        m.store32(0x09AA5770,0x27BDFDC0);m.store32(0x09AA49F8,0x27BDFFF0);
        m.store32(menu+16,rows);m.store32(menu+24,1);m.store32(menu+64,0);m.store8(menu+37,0);m.store8(menu+32,2);
        text(name,"Native recipe");m.store32(rows+4,name);set_enabled(true,directory);
        unsigned checked=0;
        for(unsigned category=0;category<2;category++)for(unsigned id=1;id<=100;id++) {
            const unsigned address=(category==0 ? 0x0893B938u : 0x0894944Cu)+26*id;
            const unsigned type=m.load8(address),equipment=m.load16(address+2);
            if(type>5 || !equipment || (category==0 ? type==5 : type!=5))continue;
            psprecomp::AllegrexContext native{};native.pc=0x0885DFA0;native.gpr[29]=0x09FF0000;
            native.gpr[5]=category;native.gpr[6]=id;
            unsigned steps=0;
            while(native.pc!=0x0885E078 && ++steps<100)psprecomp::interpret_allegrex(rt,native,1);
            check(native.pc==0x0885E078 && native.gpr[30]==address);
            m.store8(rows+10,type);m.store8(rows+11,1);m.store16(rows+12,equipment);m.store16(rows+14,id);
            c.gpr[4]=menu;capture_menu(c,v);frame(v);const auto observed=view();
            check(observed.smithy && observed.selected->key.recipe==id && observed.selected->key.category==category);
            unsigned material=0;
            for(unsigned row=0;row<4;row++)if(m.load16(address+4+row*4)) {
                check(observed.selected->materials[material].item==m.load16(address+4+row*4));
                check(observed.selected->materials[material].required==m.load16(address+6+row*4));material++;
            }
            check(observed.selected->materials.size()==material);checked++;
        }
        std::printf("Native smithy recipe selection verified against actual instructions: %u armor/weapon recipes\n",checked);
        set_enabled(false);
    }
    // The directory was created only for this test and contains our pin files.
    std::filesystem::remove_all(directory);
}
