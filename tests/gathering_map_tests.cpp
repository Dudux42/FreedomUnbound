#include "gathering_map.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/interpreter.hpp"
#include <bit>
#include <source_location>
#include <cstdio>
#include <stdexcept>
using namespace portablekit::fu::gathering_map;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok){std::fprintf(stderr,"gathering map regression at %u\n",at.line());throw std::runtime_error("gathering map regression");}
}
int main(int argc,char **argv) {
    psprecomp::Runtime rt;auto &m=rt.memory();auto v=m.aot_fast_view();psprecomp::AllegrexContext c{};
    if(argc==2)psprecomp::Elf32Image::from_file(argv[1]).load_into(m);
    constexpr unsigned state=0x09000000,quest=0x09100000,hud=0x09200000,hunter=0x09300000,ids=0x09400000,offsets=0x09400100,data=0x09500000;
    auto f=[&](unsigned p,float n){m.store32(p,std::bit_cast<unsigned>(n));};
    m.store32(0x089CC558,state);m.store32(0x08A62D9C,quest);m.store32(0x099998F4,hunter);m.store32(hud+1328,hunter);
    m.store32(0x09A6F490,0x27BDFFF0);m.store8(state+46,2);m.store8(state+0x6AF10,1);
    m.store32(0x09CE4810,2);m.store32(0x09CE4814,ids);m.store16(ids,7);m.store16(ids+2,8);
    m.store32(quest+0x60,offsets);m.store32(quest+0x50,data);m.store32(offsets,24);m.store32(offsets+4,120);
    auto node=[&](unsigned p,float x,float z,unsigned type){f(p,x);f(p+8,z);f(p+12,30);m.store16(p+16,1);m.store16(p+18,3);m.store16(p+20,type);};
    f(0x09CDF040+7*32,100);f(0x09CDF044+7*32,200);f(0x09CDF040+8*32,120);f(0x09CDF044+8*32,210);
    f(hunter+608,100);f(hunter+616,200);
    m.store16(hunter+0x29A,7);
    node(data+24,0,0,3);node(data+48,10,5,4);node(data+72,0,0,2);f(data+96,-1);
    node(data+120,0,0,1);f(data+144,-1);
    c.gpr[21]=hud;c.gpr[22]=300;c.gpr[23]=20;c.fpr[24]=1;c.fpr[23]=1;c.fpr[22]=1;c.fpr[12]=320;c.fpr[13]=40;
    capture(c,v,false);check(snapshot().markers.empty());set_enabled(true);capture(c,v,false);
    auto result=snapshot();check(result.markers.size()==3 && result.markers[0].x==320 && result.markers[0].y==40);
    check(result.markers[1].kind==Kind::Bugs && result.markers[2].kind==Kind::Fishing);
    set_filters(1u<<unsigned(Kind::Mine));check(snapshot().markers.size()==1 && snapshot().markers[0].kind==Kind::Mine);
    set_filters(1u<<unsigned(Kind::Bugs));check(snapshot().markers.size()==1 && snapshot().markers[0].kind==Kind::Bugs);
    set_filters(1u<<unsigned(Kind::Fishing));check(snapshot().markers.size()==1 && snapshot().markers[0].kind==Kind::Fishing);
    set_filters(0);check(snapshot().markers.empty());
    set_filters(31);check(snapshot().markers.size()==3); // Restore without recapturing.
    constexpr unsigned temporary=0x09600000;
    m.store32(0x09A4F1A0,temporary);m.store32(0x0886CC24,0x27BDFFE0);
    for(unsigned i=0;i<20;++i)m.store16(temporary+4+i*32+16,0xFFFF);
    const auto carve=temporary+4;
    node(carve,20,10,2);m.store16(carve+24,7);
    capture(c,v,false);check(snapshot().markers.size()==4 && snapshot().markers.back().kind==Kind::Carve);
    check(snapshot().markers.back().x==340 && snapshot().markers.back().y==50);
    if(argc==2) {
        // Verify the format against actual USA query/decrement instructions,
        // without executing the loot-award path or modifying a player's save.
        psprecomp::AllegrexContext native{};native.pc=0x0886CEA8;native.gpr[5]=hunter;native.gpr[31]=0x0BADF00D;
        unsigned steps=0;
        while(native.pc!=0x0BADF00D && ++steps<2048)psprecomp::interpret_allegrex(rt,native,1);
        check(native.pc==0x0BADF00D && native.gpr[2]==carve);
        for(unsigned used=1;used<=3;++used) {
            native={};native.pc=0x0886CFE0;native.gpr[16]=carve;steps=0;
            while(native.pc!=0x0886D010 && ++steps<32)psprecomp::interpret_allegrex(rt,native,1);
            check(native.pc==0x0886D010 && m.load16(carve+18)==3-used);
            capture(c,v,false);check(snapshot().markers.size()==(used==3?3:4));
        }
        m.store16(carve+18,3);capture(c,v,false);
        std::printf("Native temporary-point query and three carve decrements verified.\n");
    }
    for(unsigned mask=0;mask<32;++mask) {
        set_filters(mask);const auto items=snapshot().markers;
        const unsigned count=((mask>>1)&1)+((mask>>2)&1)+((mask>>3)&1)+((mask>>4)&1);
        check(items.size()==count);
        for(const auto &item:items)check(mask & (1u<<unsigned(item.kind)));
    }
    set_filters(31);
    m.store16(carve+18,0);capture(c,v,false);check(snapshot().markers.size()==3);
    m.store16(carve+18,3);m.store16(carve+24,8);capture(c,v,false);check(snapshot().markers.size()==3);
    m.store16(carve+24,7);m.store16(carve+20,1);capture(c,v,false);check(snapshot().markers.size()==3); // Dropped item is not carving.
    m.store16(carve+20,2);m.store16(carve+16,0xFFFF);capture(c,v,false);check(snapshot().markers.size()==3);
    m.store16(carve+16,1);f(carve,9000);capture(c,v,false);check(snapshot().markers.size()==3); // Clipped.
    f(carve,20);node(temporary+4+32,20,10,2);m.store16(temporary+4+32+24,7);
    capture(c,v,false);check(snapshot().markers.size()==4); // Duplicate carving entry.
    m.store32(offsets,0);capture(c,v,false);check(snapshot().markers.size()==1); // No static nodes in this area.
    m.store32(quest+0x50,0);capture(c,v,false);check(snapshot().markers.size()==1); // Carving is independent of static node data.
    m.store32(quest+0x50,data);
    m.store32(offsets,24);m.store32(0x09A4F1A0,0xFFFFFFFC);capture(c,v,false);check(snapshot().markers.size()==3);
    m.store32(0x09A4F1A0,0);
    m.store16(hunter+0x29A,8);capture(c,v,false);check(snapshot().markers.size()==1 && snapshot().markers[0].kind==Kind::Gather && snapshot().markers[0].x==340);
    set_filters(31 & ~(1u<<unsigned(Kind::Gather)));check(snapshot().markers.empty());
    set_enabled(false);set_enabled(true);check(!(filters() & 1));set_filters(31);
    m.store16(hunter+0x29A,7);
    m.store16(data+24+18,0);m.store16(data+72+18,0);capture(c,v,false);check(snapshot().markers.size()==1);
    m.store32(hunter+0x410,8);capture(c,v,false);check(snapshot().markers.size()==1);
    m.store8(state+46,4);capture(c,v,false);check(snapshot().markers.empty());
    m.store8(state+46,2);m.store32(offsets+4,0xFFFFFFFF);capture(c,v,false);check(snapshot().markers.size()==1);
    m.store32(offsets+4,120);c.gpr[30]=300;c.gpr[29]=0x09700000;m.store16(c.gpr[29]+110,20);
    f(hud+1380,1.f/760);f(hud+1384,1.f/760);c.fpr[21]=13.3f;
    capture(c,v,true);check(snapshot().markers.size()==1 && snapshot().right>367 && snapshot().right<368);
    c.fpr[12]=900;capture(c,v,true);check(snapshot().markers.empty());
    set_enabled(false);capture(c,v,false);check(snapshot().markers.empty());
}
