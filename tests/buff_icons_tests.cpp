#include "buff_icons.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/interpreter.hpp"
#include <cmath>
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace portablekit::fu::buff_icons;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok){std::fprintf(stderr,"buff icons regression at line %u\n",at.line());throw std::runtime_error("buff icons regression");}
}
int main(int argc,char **argv) {
    psprecomp::Runtime rt;auto &m=rt.memory();auto v=m.aot_fast_view();
    constexpr unsigned state=0x09000000,hunter=0x090B3580;
    constexpr std::array<unsigned,item_count> offsets{982,986,1570,1576,1574,1572,1578};
    auto setup=[&]{
        m.store32(0x09A6B154,0x106005F7);m.store32(0x09A693E0,0xA60203D6);m.store32(0x09A69404,0xA60203DA);m.store32(0x09A69EC0,0xA4620000);
        m.store32(0x089CC558,state);m.store32(0x099998F4,hunter);m.store8(state+46,2);
        m.store32(0x08A62D9C,0x09800000);m.store8(0x09800000,1);m.store8(0x09800001,2);
        m.store16(hunter+0x2E4,100);m.store16(hunter+0x41E,100);
        m.store8(hunter+980,10);m.store8(hunter+984,40);
        for(auto off:offsets)m.store16(hunter+off,1800);
    };
    setup();refresh(v);check(!snapshot().count);set_enabled(true);refresh(v);check(snapshot().count==item_count);
    for(unsigned i=0;i<item_count;i++)check(snapshot().buffs[i].kind==static_cast<Kind>(i));
    // Middle-slot expiry packs every following buff to the left immediately.
    m.store16(hunter+offsets[2],0);refresh(v);auto s=snapshot();check(s.count==item_count-1);
    check(s.buffs[2].kind==Kind::Recovery && s.buffs[5].kind==Kind::Vision);
    m.store16(hunter+offsets[2],901);refresh(v);check(snapshot().buffs[2].kind==Kind::Stamina);
    for(auto frames:{901u,1800u,18000u})check(warning(frames)==Warning::Normal);
    for(auto frames:{301u,899u,900u})check(warning(frames)==Warning::Pulse);
    for(auto frames:{1u,299u,300u})check(warning(frames)==Warning::Urgent);
    check(warning(0)==Warning::Normal);
    check(appearance(901,.25).scale==1 && !appearance(901,.25).red);
    check(appearance(900,.25).scale>1.1f && appearance(900,.75).scale<.9f);
    check(!appearance(900,.25).red && appearance(300,0).red && !appearance(300,.3).red);
    for(unsigned i=0;i<100;i++)refresh(v);check(m.load16(hunter+offsets[2])==901);
    // Negative modifiers and lasting drugs are not timed positive buffs.
    setup();m.store8(hunter+984,255);refresh(v);check(snapshot().count==item_count-1);
    setup();m.store8(hunter+980,0);refresh(v);check(snapshot().count==item_count-1);
    setup();for(auto off:offsets)m.store16(hunter+off,0);
    m.store8(hunter+906,5);m.store8(hunter+907,20);refresh(v);check(!snapshot().count);
    for(unsigned phase:{0u,1u,4u,5u,6u,7u}){setup();m.store8(state+46,phase);refresh(v);check(!snapshot().count);}
    setup();m.store8(state+46,3);refresh(v);check(snapshot().count==item_count);
    m.store8(0x09800000,2);refresh(v);check(!snapshot().count);
    m.store8(0x09800000,1);m.store8(0x09800001,3);refresh(v);check(!snapshot().count);
    setup();m.store16(hunter+0x2E4,0);refresh(v);check(!snapshot().count);
    setup();m.store32(0x099998F4,0xFFFFFFFC);refresh(v);check(!snapshot().count);
    setup();m.store32(0x089CC558,0);refresh(v);check(!snapshot().count);
    setup();m.store32(0x09A6B154,0);refresh(v);check(!snapshot().count);
    setup();m.store32(0x09A693E0,0);refresh(v);check(!snapshot().count);
    setup();refresh(v);set_enabled(false);check(!snapshot().count);
    setup();set_enabled(true);
    for(unsigned i=0;i<melody_count;i++){m.store16(hunter+1696+2*i,900);m.store8(hunter+1527+i,1);}
    refresh(v);check(snapshot().count==capacity);
    m.store16(hunter+1696+10,0);refresh(v);check(snapshot().count==capacity-1);
    check(snapshot().buffs[item_count+5].kind==static_cast<Kind>(item_count+6));
    for(unsigned i=0;i<melody_count;i++){m.store16(hunter+1696+2*i,0);m.store8(hunter+1527+i,0);}
    // Severe-cold double depletion reaches the warnings at actual 30/10 seconds.
    m.store16(hunter+1574,1801);refresh(v);m.store16(hunter+1574,1799);refresh(v);
    check(snapshot().buffs[4].frames==900);
    m.store16(hunter+1574,601);refresh(v);m.store16(hunter+1574,599);refresh(v);
    check(snapshot().buffs[4].frames==300);refresh(v);check(snapshot().buffs[4].frames==300);
    if(argc==3) {
        psprecomp::Elf32Image::from_file(argv[1]).load_into(m);
        psprecomp::Elf32Image::from_file(argv[2]).load_into(m);
        check(m.load32(0x09A693E0)==0xA60203D6 && m.load32(0x09A69404)==0xA60203DA);
        // Interpret the real increment/store pair for each countdown in private
        // RAM, including both animation thresholds and the final zero.
        constexpr std::array<unsigned,capacity> pcs{0x09A693DC,0x09A69400,0x09A6907C,
            0x09A692C4,0x09A69248,0x09A69160,0x09A69340};
        setup();set_enabled(true);
        for(unsigned kind=0;kind<item_count;kind++) {
            for(unsigned frames:{901u,900u,301u,300u,1u}) {
                m.store16(hunter+offsets[kind],frames);
                psprecomp::AllegrexContext c{};c.pc=pcs[kind];c.gpr[16]=hunter;c.gpr[2]=frames;
                for(unsigned step=0;step<2;step++)psprecomp::interpret_allegrex(rt,c,1);
                check(m.load16(hunter+offsets[kind])==frames-1);
                refresh(v);const auto live=snapshot();bool found=false;
                for(unsigned j=0;j<live.count;j++)if(live.buffs[j].kind==static_cast<Kind>(kind)) {
                    found=true;check(live.buffs[j].frames==frames-1);
                    check(warning(live.buffs[j].frames)==warning(frames-1));
                }
                check(found==(frames>1));
            }
        }
        for(unsigned kind=0;kind<melody_count;kind++) {
            m.store16(hunter+1696+2*kind,300);m.store8(hunter+1527+kind,1);
            psprecomp::AllegrexContext c{};c.pc=0x09A69EBC;c.gpr[3]=hunter+1696+2*kind;c.gpr[2]=300;
            for(unsigned step=0;step<2;step++)psprecomp::interpret_allegrex(rt,c,1);
            check(m.load16(hunter+1696+2*kind)==299);refresh(v);
            bool found=false;const auto data=snapshot();
            for(unsigned j=0;j<data.count;j++)if(data.buffs[j].kind==static_cast<Kind>(item_count+kind)) {
                found=true;check(data.buffs[j].frames==299);
            }
            check(found);
        }
        std::puts("Native buff decrements verified: seven item counters and 21 melody counters");
    }
}

