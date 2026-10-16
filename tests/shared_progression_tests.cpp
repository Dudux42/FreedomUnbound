#include "shared_progression.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/interpreter.hpp"
#include <array>
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace portablekit::fu::shared_progression;
void check(bool ok, const std::source_location at = std::source_location::current()) {
    if (!ok) { std::fprintf(stderr,"shared progression regression at line %u\n",at.line()); throw std::runtime_error("shared progression regression"); }
}
// Optional local probe: execute the owner's actual quest classification,
// poster check and clear-bit setter in private RAM. No game code/data ships.
void native_probe(const char *elf) {
    psprecomp::Runtime runtime; auto &m = runtime.memory();
    psprecomp::Elf32Image::from_file(elf).load_into(m);
    auto v = m.aot_fast_view();
    constexpr unsigned state=0x09999DA0, data=0x08A8C640, party=0x09800000;
    m.store32(0x089CC558,state); m.store32(0x08A632C0,data); m.store32(data+0xF7A57C,1);
    m.store32(0x09A0B684,party); m.store8(state+46,3);
    auto run = [&](unsigned quest, bool on, bool poster, unsigned participant=0) {
        for(unsigned i=0;i<0x100;i++) m.store8(state+0x4400+i,0xA5);
        for(unsigned i=0;i<64;i++) m.store8(state+0x4428+i,0);
        m.store16(state+0x6AF12,quest); m.store8(state+40,participant);
        m.store8(party+0x12D6,poster); set_enabled(on);
        psprecomp::AllegrexContext ctx{}; ctx.pc=0x08875158; ctx.gpr[29]=0x09F00000;
        unsigned steps=0, gates=0;
        while(ctx.pc!=0x08875224 && steps++<10000) {
            if(ctx.pc==0x088751B4) { completion_gate(ctx,v); ++gates; }
            const auto result=psprecomp::interpret_allegrex(runtime,ctx,1);
            check(result==psprecomp::InterpreterExit::Budget || result==psprecomp::InterpreterExit::Dispatch);
        }
        check(ctx.pc==0x08875224 && steps<10000);
        unsigned index=0;
        while(index<504 && m.load16(0x0899C1C8+index*2)!=quest) ++index;
        check(index<504);
        const bool credited=(m.load8(state+0x4428+index/8)&(1u<<(index%8)))!=0;
        for(unsigned i=0;i<0x100;i++) {
            const unsigned expected=i>=0x28 && i<0x68 ?
                (credited && i==0x28+index/8 ? 1u<<(index%8) : 0u) : 0xA5u;
            check(m.load8(state+0x4400+i)==expected);
        }
        std::printf("native quest=%u mod=%u poster=%u participant=%u gate=%u credited=%u steps=%u\n",
            quest,on,poster,participant,gates,credited,steps);
        return credited;
    };
    for(unsigned i=0;i<64;i++) {
        const auto quest=m.load16(0x089A8684+i*2); if(quest==0xFFFF) break;
        check(!run(quest,false,false)); check(run(quest,true,false));
        check(run(quest,false,true)); check(run(quest,true,true));
    }
    check(run(1001,false,false) && run(1001,true,false)); // Ordinary Hall quest.
    check(run(10001,false,false) && run(10001,true,false)); // Native Village path unchanged.
    const auto urgent=m.load16(0x089A8684);
    for(unsigned participant=0;participant<4;participant++) check(run(urgent,true,false,participant));
    set_enabled(false);
}
int main(int argc,char **argv) {
    psprecomp::Runtime runtime; auto &m=runtime.memory(); auto v=m.aot_fast_view();
    constexpr unsigned state=0x09000000, data=0x08801000, party=0x09800000;
    m.store32(0x088751B4,0x1040001B); m.store32(0x0887CA08,0x3C0208A6); m.store32(0x0884FE10,0x27BDFFF0);
    m.store32(0x089CC558,state); m.store32(0x08A632C0,data); m.store32(data+0xF7A57C,1);
    m.store32(0x09A0B684,party); m.store8(state+46,3);
    for(unsigned i=0;i<3;i++) m.store16(0x089A8684+i*2,std::array<unsigned,3>{1104,2120,3009}[i]);
    m.store16(0x089A8684+6,0xFFFF);
    psprecomp::AllegrexContext ctx{};
    auto gate=[&](unsigned result=0,unsigned caller=0x088751B4) {
        ctx.gpr[2]=result; ctx.gpr[31]=caller; completion_gate(ctx,v); return ctx.gpr[2];
    };
    m.store16(state+0x6AF12,1104); check(gate()==0);
    set_enabled(true);
    for(unsigned id : {1104u,2120u,3009u}) {m.store16(state+0x6AF12,id); check(gate()==1);}
    check(gate(1)==1 && gate(2)==2 && gate(0,0x088766A0)==0); // No reward/authority override.
    for(unsigned phase : {0u,1u,2u,5u,6u,7u}) {m.store8(state+46,phase);check(gate()==0);}
    m.store8(state+46,4); check(gate()==1); m.store8(state+46,3);
    for(unsigned id : {0u,1001u,4001u,10001u,11004u,61000u,65535u}) {m.store16(state+0x6AF12,id);check(gate()==0);}
    m.store16(state+0x6AF12,1104);
    m.store32(data+0xF7A57C,0); check(gate()==0); m.store32(data+0xF7A57C,1);
    m.store8(party+0x12D6,1); check(gate()==0); m.store8(party+0x12D6,0);
    m.store32(0x088751B4,0);check(gate()==0);m.store32(0x088751B4,0x1040001B);
    m.store32(0x089CC558,0xFFFFFFFC);check(gate()==0);m.store32(0x089CC558,state);
    m.store32(0x08A632C0,0xFFFFFFFC);check(gate()==0);m.store32(0x08A632C0,data);
    m.store32(0x09A0B684,0);check(gate()==0);m.store32(0x09A0B684,party);
    for(unsigned i=0;i<64;i++)m.store16(0x089A8684+i*2,1104);
    check(gate()==0); // Unterminated table rejected before any credit.
    set_enabled(false);check(gate()==0);
    if(argc==2) native_probe(argv[1]);
}
