#include "return_timer.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/interpreter.hpp"
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace portablekit::fu::return_timer;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok){std::fprintf(stderr,"return timer regression at line %u\n",at.line());throw std::runtime_error("return timer regression");}
}
int main(int argc,char **argv) {
    psprecomp::Runtime rt;auto &m=rt.memory();const auto v=m.aot_fast_view();
    constexpr unsigned state=0x09000000,quest=0x09800000;
    auto setup=[&](bool signatures=true){
        if(signatures) {m.store32(0x0886B494,0x2442FFFF);m.store32(0x0886B49C,0xAE220710);}
        m.store32(0x09A6B154,0x106005F7);
        m.store32(0x089CC558,state);m.store32(0x08A62D9C,quest);
        m.store8(state+46,3);m.store8(quest,1);m.store8(quest+1,2);m.store32(quest+0x710,1800);
    };
    setup();refresh(v);check(!snapshot().frames);
    set_enabled(true);
    for(unsigned frames : {1u,29u,30u,31u,599u,600u,1799u,1800u}) {
        m.store32(quest+0x710,frames);refresh(v);
        check(snapshot().frames==frames && snapshot().seconds==(frames+29)/30);
        check(m.load32(quest+0x710)==frames); // Observer never changes native time.
    }
    for(unsigned frames : {0u,1801u,0xFFFFFFFFu,0x80000000u}) {m.store32(quest+0x710,frames);refresh(v);check(!snapshot().frames);}
    m.store32(quest+0x710,600);
    for(unsigned phase : {0u,1u,2u,4u,5u,6u,7u}) {m.store8(state+46,phase);refresh(v);check(!snapshot().frames);}
    m.store8(state+46,3);
    for(unsigned mode : {0u,2u,3u}) {m.store8(quest,mode);refresh(v);check(!snapshot().frames);}
    m.store8(quest,1);
    for(unsigned stage : {0u,1u,3u,4u,6u}) {m.store8(quest+1,stage);refresh(v);check(!snapshot().frames);}
    m.store8(quest+1,2);refresh(v);check(snapshot().seconds==20);
    // Re-reading frozen native time cannot advance a host-side countdown.
    for(unsigned i=0;i<30;i++)refresh(v);
    check(snapshot().seconds==20 && m.load32(quest+0x710)==600);
    m.store32(0x08A62D9C,0xFFFFFFFC);refresh(v);check(!snapshot().frames);
    setup();m.store32(0x089CC558,0);refresh(v);check(!snapshot().frames);
    setup();m.store32(0x0886B494,0);refresh(v);check(!snapshot().frames);
    setup();m.store32(0x09A6B154,0);refresh(v);check(!snapshot().frames);
    setup();refresh(v);set_enabled(false);check(!snapshot().frames);
    if(argc==2) {
        psprecomp::Elf32Image::from_file(argv[1]).load_into(m);
        check(m.load32(0x0886B494)==0x2442FFFF && m.load32(0x0886B49C)==0xAE220710);
        setup(false);set_enabled(true);
        // Interpret the owner's actual decrement/branch/store in private RAM.
        // Compare snapshots to each native counter value, including boundaries.
        for(unsigned start : {1800u,600u}) {
            m.store32(quest+0x710,start);
            for(unsigned remaining=start;remaining>1;--remaining) {
                psprecomp::AllegrexContext c{};c.pc=0x0886B494;c.gpr[17]=quest;c.gpr[2]=remaining;
                unsigned steps=0;
                while(c.pc!=0x0886B7A8 && ++steps<8) {
                    const auto result=psprecomp::interpret_allegrex(rt,c,1);
                    check(result==psprecomp::InterpreterExit::Budget || result==psprecomp::InterpreterExit::Dispatch);
                }
                check(c.pc==0x0886B7A8 && m.load32(quest+0x710)==remaining-1);
                refresh(v);check(snapshot().seconds==seconds_left(remaining-1));
            }
            std::printf("Native return countdown verified: %u frames (%u seconds)\n",start,seconds_left(start));
        }
        set_enabled(false);
    }
}
