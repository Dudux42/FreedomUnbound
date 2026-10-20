#include "key_quests.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include <cstdio>
#include <stdexcept>
#include <source_location>
using namespace portablekit::fu::key_quests;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok) {std::fprintf(stderr,"key quests regression at line %u\n",at.line());throw std::runtime_error("key quests regression");}
}
int main(int argc,char **argv) {
    psprecomp::Runtime rt;auto &m=rt.memory();const auto view=m.aot_fast_view();
    for(unsigned i=0;i<18;i++)m.store32(0x089B2010+i*4,0x089B1EA0);
    m.store16(0x089B1EA0,10003);m.store16(0x089B1EA2,1004);m.store16(0x089B1EA4,0);
    check(is_key(view,10003) && is_key(view,1004));
    for(unsigned id : {0u,1u,1001u,1104u,4001u,10001u,10002u,65535u}) check(!is_key(view,id));
    m.store32(0x089B2010+17*4,0x089B1F73);check(!is_key(view,10003));
    m.store32(0x089B2010+17*4,0xFFFFFFFF);check(!is_key(view,10003));
    m.store32(0x089B2010+17*4,0x089B1EA0);
    for(unsigned i=0;i<16;i++)m.store16(0x089B1EA0+i*2,10003);
    check(!is_key(view,10003)); // Malformed lists never silently classify a key.
    m.store16(0x089B1EA2,0);check(is_key(view,10003));
    m.store16(0x089B1EA0,4001);check(!is_key(view,10003));
    m.store16(0x089B1EA0,10003);
    m.store16(0x089B1EA2,1004);m.store16(0x089B1EA4,0);
    m.store32(0x0889198C,0x27BDFFE0);m.store32(0x09A8CE7C,0x0E224663);
    constexpr unsigned record=0x09800000,title=0x09800100;
    m.store16(record+24,10003);m.store8(title,'Q');
    psprecomp::AllegrexContext ctx{};ctx.gpr[20]=record;ctx.gpr[9]=title;
    ctx.gpr[31]=0x09A8CE84;ctx.gpr[5]=280;ctx.gpr[6]=64;
    capture(ctx,view,0x0889198C);check(!visible());
    set_enabled(true);
    capture(ctx,view,0x0889198C);capture(ctx,view,0x0889198C);
    check(visible());auto marks=take_markers();check(marks.count==1 && marks.markers[0].quest==10003);
    check(marks.markers[0].x==271 && marks.markers[0].y==70 && !visible());
    m.store16(record+24,1004);const auto registers=ctx.gpr;
    capture(ctx,view,0x0889198C);marks=take_markers();
    check(marks.count==1 && marks.markers[0].quest==1004 && ctx.gpr==registers);
    check(m.load16(record+24)==1004); // Presentation never writes progression.
    m.store16(record+24,10001);capture(ctx,view,0x0889198C);check(!visible());
    m.store16(record+24,10003);ctx.gpr[31]=0x09A8CDF0;capture(ctx,view,0x0889198C);check(!visible());
    ctx.gpr[31]=0x09A8CE84;ctx.gpr[20]=0xFFFFFFFC;capture(ctx,view,0x0889198C);check(!visible());
    ctx.gpr[20]=record;m.store32(0x09A8CE7C,0);capture(ctx,view,0x0889198C);check(!visible());
    m.store32(0x09A8CE7C,0x0E224663);ctx.gpr[5]=5;capture(ctx,view,0x0889198C);check(!visible());
    set_enabled(false);check(!visible() && !take_markers().count);
    if(argc==2) {
        psprecomp::Elf32Image::from_file(argv[1]).load_into(m);
        unsigned count=0;
        for(unsigned i=0;i<18;i++) {
            const auto p=m.load32(0x089B2010+i*4);
            for(unsigned j=0;j<16;j++) {
                const auto id=m.load16(p+j*2);if(!id)break;
                check(is_key(view,id));++count;
            }
        }
        check(count==77);
        check(!is_key(view,10001) && !is_key(view,10002) && !is_key(view,1104));
        std::printf("Verified %u native key quests across all 18 Hall/Village tiers\n",count);
    }
}
