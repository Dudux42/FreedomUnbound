#include "solo_guildhall_scaling.hpp"
#include "psprecomp/runtime.hpp"
#include <cstdio>
#include <stdexcept>
using namespace portablekit::fu::solo_guildhall_scaling;
void check(bool value) { if (!value) throw std::runtime_error("solo scaling regression"); }
int main() {
    psprecomp::Runtime rt; auto &m=rt.memory(); auto v=m.aot_fast_view();
    constexpr unsigned state=0x09000000, data=0x08801000, actor=0x09800000;
    m.store32(0x089CC558,state); m.store32(0x08A632C0,data);
    m.store32(0x09AC9120,0x908601E8); m.store32(0x0887CA08,0x3C0208A6);
    m.store16(state+0x6AF12,1001); m.store8(state+46,2);
    m.store8(actor+0x1E8,1); m.store32(data+0xF7A57C,0);
    psprecomp::AllegrexContext c{}; c.gpr[4]=actor;
    check(!eligible(c,v)); set_enabled(true); check(eligible(c,v));
    for (unsigned id : {1001u,2001u,3001u}) {m.store16(state+0x6AF12,id);check(eligible(c,v));}
    for (unsigned id : {0u,999u,4000u,10001u,11001u,61000u}) {m.store16(state+0x6AF12,id);check(!eligible(c,v));}
    m.store16(state+0x6AF12,1001);
    // Online hosts, participants and solo online play all fail the same gate.
    for (unsigned mode : {1u,3u,0xFFFFFFFFu}) {m.store32(data+0xF7A57C,mode);check(!eligible(c,v));}
    m.store32(data+0xF7A57C,0);check(eligible(c,v));
    for (unsigned phase : {0u,1u,3u,4u,5u,6u}) {m.store8(state+46,phase);check(!eligible(c,v));}
    m.store8(state+46,2);
    c.gpr[5]=1;check(!eligible(c,v));c.gpr[5]=0;
    m.store8(actor+0x1E8,0);check(!eligible(c,v));m.store8(actor+0x1E8,1);
    c.gpr[4]=0xFFFFFFFC;check(!eligible(c,v));c.gpr[4]=actor;
    m.store32(0x08A632C0,0x09900000);check(!eligible(c,v));m.store32(0x08A632C0,data);
    m.store32(0x09AC9120,0);check(!eligible(c,v));m.store32(0x09AC9120,0x908601E8);
    check(scaled_hp(10000)==6500 && scaled_hp(1)==1 && scaled_hp(0)==0);
    check(scaled_hp(32767)==21299 && scaled_hp(0xFFFFFFFF)==0xFFFFFFFF);
    for(unsigned hp=1;hp<=32767;++hp) check(scaled_hp(hp)>=1 && scaled_hp(hp)<=hp);
    set_enabled(false);check(!eligible(c,v));
    std::puts("Offline Hall eligibility and 65% HP checks passed");
}
