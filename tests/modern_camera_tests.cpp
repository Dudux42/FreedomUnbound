#include "modern_camera.hpp"
#include "camera/camera_input.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/interpreter.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace portablekit;
using namespace portablekit::fu::modern_camera;
void check(bool ok, const std::source_location at = std::source_location::current()) {
    if (!ok) { std::fprintf(stderr, "camera regression line %u\n", at.line()); throw std::runtime_error("camera regression"); }
}
int main(int argc,char **argv) {
    psprecomp::Runtime runtime;
    auto &m = runtime.memory(); auto v = m.aot_fast_view();
    if(argc==2)psprecomp::Elf32Image::from_file(argv[1]).load_into(m);
    constexpr auto r = 0x09998E70u, c = r + 0x150, hunter = 0x090B3580u;
    constexpr auto state = 0x09100000u, table = 0x0945B000u, stack = 0x09F00000u;
    m.store32(0x08887614, 0x86820080); m.store32(0x08887834, 0xC6600200); m.store32(0x09A6F490, 0x27BDFFF0);
    m.store32(0x089CBD04, r); m.store32(0x089CC558, state); m.store8(state + 46, 2);
    m.store32(r + 0xA84, hunter); m.store8(r + 0xB0 + 0x91, 1); m.store8(r + 0x14, 3);
    m.store32(c + 0x6C, table); m.store32(c + 0x70, table + 96); m.store16(hunter + 0x29A, 98);
    const std::array<std::array<float, 3>, 5> params{{{400,200,195},{300,160,184},{270,490,170},{140,490,170},{60,450,210}}};
    for (unsigned i = 0; i < 5; ++i) for (unsigned j = 0; j < 3; ++j)
        m.store32(table + (i ? i*32 : 160) + std::array<unsigned,3>{4,8,16}[j], std::bit_cast<std::uint32_t>(params[i][j]));
    psprecomp::AllegrexContext ctx{}; ctx.gpr[20] = c; ctx.gpr[19] = hunter; ctx.gpr[29] = stack;
    auto step = [&](float x, float y) { camera::add_motion(camera::Source::Stick, x, y); yaw(ctx, v); geometry(ctx, v); };
    auto height = [&] { return std::bit_cast<float>(m.load32(stack + 340)); };
    prepare(v); check(!driving()); set_enabled(true); prepare(v); check(driving());
    m.store16(c + 0x80, 10000); m.store16(c + 0x82, 10000);
    step(0,0); check(std::abs(height() - 140) < .001f);
    step(1,1); const auto small = static_cast<std::int16_t>(10000 - m.load16(c + 0x80));
    check(small >= 181 && small <= 183); check(height() > 140 && height() < 270);
    const auto still = m.load16(c + 0x80); const auto held_height = height(); step(0,0);
    check(m.load16(c + 0x80) == still && std::abs(height() - held_height) < .001f);
    step(2,0); check(static_cast<std::int16_t>(still - m.load16(c + 0x80)) >= small*2 - 1);
    m.store16(c + 0x80, 0); m.store16(c + 0x82, 0);
    m.store16(c + 0x84,0x100);step(0,0);m.store16(c + 0x84,0);
    step(360,0); check(m.load16(c + 0x80) <= 1);
    step(0,1000); check(std::abs(height() - 400) < .001f); step(0,-1000); check(std::abs(height() - 60) < .001f);
    // A physical preset change resynchronizes; L and D-pad retain priority.
    m.store8(r + 0x14, 2); m.store32(c + 0x70, table + 64);
    m.store16(c + 0x84,0x10);step(0,0);m.store16(c + 0x84,0);check(std::abs(height() - 270) < .001f);
    m.store16(c + 0x84, 0x100); const auto before_l = m.load16(c + 0x80); step(10,10);
    check(m.load16(c + 0x80) == before_l && std::abs(height() - 270) < .001f); m.store16(c + 0x84, 0);
    // Reproduce the native geometry-triggered turn toward the hunter between
    // updates: it must neither drag the chosen yaw nor oppose new stick motion.
    step(4,0);const auto manual=m.load16(c+0x80);
    m.store8(c+0x89,1);m.store16(c+0x80,manual+2000);step(0,0);
    check(m.load16(c+0x80)==manual && m.load8(c+0x89)==1);
    m.store16(c+0x80,manual+2000);step(1,0);
    check(static_cast<std::int16_t>(manual-m.load16(c+0x80))>=181 && m.load8(c+0x89)==1);
    // Automatic preset changes must not snap an independently chosen pitch.
    step(0,3);const auto chosen_height=height();
    m.store8(r+0x14,3);m.store32(c+0x70,table+96);step(0,0);
    check(std::abs(height()-chosen_height)<.001f);
    // Native preset pointer/index can briefly disagree during area adjustment.
    m.store32(c+0x70,table+64);prepare(v);check(driving());step(1,1);
    check(std::isfinite(height()));m.store32(c+0x70,table+96);
    // Repeated/reordered valid pitch records must not shut horizontal control off.
    const auto restore_params=[&]{for(unsigned i=0;i<5;i++)for(unsigned j=0;j<3;j++)
        m.store32(table+(i?i*32:160)+std::array<unsigned,3>{4,8,16}[j],std::bit_cast<std::uint32_t>(params[i][j]));};
    for(unsigned j=0;j<3;j++)m.store32(table+32+std::array<unsigned,3>{4,8,16}[j],std::bit_cast<std::uint32_t>(params[2][j]));
    prepare(v);step(2,2);check(driving() && std::isfinite(height()));
    m.store32(table+32+4,std::bit_cast<std::uint32_t>(1000.f));
    prepare(v);step(2,2);check(driving() && std::isfinite(height()));restore_params();
    for(unsigned i=0;i<5;i++)for(unsigned j=0;j<3;j++)
        m.store32(table+(i?i*32:160)+std::array<unsigned,3>{4,8,16}[j],std::bit_cast<std::uint32_t>(params[3][j]));
    step(2,10);check(driving() && std::isfinite(height()));restore_params();
    // An unusable pitch record leaves native geometry alone, but yaw still works.
    m.store32(table+32+8,0);const unsigned prior_geometry=m.load32(stack+340),prior_yaw=m.load16(c+0x80);
    step(2,2);check(driving() && m.load16(c+0x80)!=prior_yaw && m.load32(stack+340)==prior_geometry);restore_params();
    m.store16(c+0x84,0x100);m.store16(c+0x80,before_l);step(0,0);m.store16(c+0x84,0);
    // No modern motion in town, aiming, menus, reward/save screens, or unsupported tables.
    for (unsigned phase : {0u, 5u, 6u, 7u}) {
        m.store8(state + 46, phase); prepare(v); check(!driving()); step(10,10); check(m.load16(c + 0x80) == before_l);
    }
    m.store8(state + 46, 2); m.store8(r + 0xA8D, 1); prepare(v); check(!driving());
    m.store8(r + 0xA8D, 0); m.store8(r + 0x961, 1); prepare(v); check(!driving());
    m.store8(r + 0x961, 0); prepare(v); check(driving()); step(0,0); check(m.load16(c + 0x80) == before_l);
    prepare(v, false); check(!driving()); prepare(v); check(driving()); step(0,0);
    m.store32(c+0x70,stack);prepare(v);check(!driving());
    m.store32(c+0x70,table+96);prepare(v);step(0,0);check(driving());
    m.store32(0x08887614, 0); prepare(v); check(!driving());
    set_enabled(false); check(!driving());
    // Target discovery is independent of the optional HP overlay.
    m.store32(0x08887614, 0x86820080); set_enabled(true); prepare(v); step(0,0);
    constexpr auto manager = 0x09600000u, a = 0x09200000u, b = 0x09201000u;
    m.store32(0x09C122B0, manager);
    auto f = [&](unsigned address, float value) { m.store32(address, std::bit_cast<std::uint32_t>(value)); };
    f(hunter+0x200,0); f(hunter+0x204,0); f(hunter+0x208,0);
    auto monster = [&](unsigned slot, unsigned address, unsigned species, float x, float z, unsigned area = 98) {
        m.store32(manager+4640+slot*4,address); m.store32(address,0x08900000);
        m.store8(address+0x1E8,species); m.store16(address+0x29A,area);
        m.store16(address+0x2E4,1000); m.store16(address+0x41E,2000);
        f(address+0x200,x); f(address+0x204,0); f(address+0x208,z);
    };
    monster(0,a,75,1000,0); monster(1,b,11,0,-2000);
    monster(2,0x09202000,3,10,0); // Small monster never targeted.
    monster(3,0x09203000,1,20,0,97); // Other area never targeted.
    m.store16(c+0x80,1234); m.store16(c+0x82,1234);
    m.store16(c+0x84,0x100);step(0,0);m.store16(c+0x84,0);
    triggers(false,false); step(0,0); check(target().identity == a && m.load16(c+0x80) == 1234);
    check(triggers(true,false) == 3); step(0,0);
    check(m.load16(c+0x80) == 49152 && m.load16(c+0x82) == 49152);
    // Holding ZL, or monster motion, never tracks continuously.
    f(a+0x208,500); triggers(true,false); step(0,0); check(m.load16(c+0x80) == 49152);
    step(2,0); check(m.load16(c+0x80) != 49152); // Free camera remains available.
    const auto before_cycle = m.load16(c+0x80);
    triggers(false,false); triggers(false,true); step(0,0);
    check(target().identity == b && m.load16(c+0x80) == before_cycle);
    triggers(false,true); step(0,0); check(target().identity == b); // One cycle per press.
    triggers(false,false); triggers(true,false); step(0,0); check(m.load16(c+0x80) == 0);
    triggers(false,false); triggers(false,true); step(0,0); check(target().identity == a); // Wrap.
    m.store16(a+0x2E4,0); step(0,0); check(target().identity == b); // Dead target replaced.
    m.store16(b+0x29A,97); step(0,0); check(target().identity == 0);
    const auto no_target = m.load16(c+0x80);
    triggers(false,false); check(triggers(true,false) == 3); step(0,0); check(m.load16(c+0x80) == no_target);
    m.store16(b+0x29A,98); triggers(false,false); step(0,0); check(target().identity == b);
    // Reserved quest triggers cannot fall through to attacks while aiming.
    m.store8(r+0xA8D,1); prepare(v); check(!driving() && triggers(true,true) == 3);
    m.store8(r+0xA8D,0); prepare(v); step(0,0); check(m.load16(c+0x80) == no_target);
    triggers(false,false);
    triggers(true,false); triggers(false,false,false); step(0,0); check(m.load16(c+0x80) == no_target);
    triggers(true,false); step(0,0); check(m.load16(c+0x80) == no_target); // Held through capture is blocked.
    m.store8(state+46,5); prepare(v); check(target().identity == 0 && triggers(true,true) == 0);
    m.store8(state+46,2); prepare(v); step(0,0); check(m.load16(c+0x80) == no_target);
    set_enabled(false); check(target().identity == 0 && triggers(true,true) == 0);
    if(argc==2) {
        // Execute the real USA geometry-triggered yaw correction, then our
        // update. A native +2000 turn must not cancel a requested -182 turn.
        m.store16(c+0x80,12000);m.store16(c+0x82,12000);
        set_enabled(true);prepare(v);step(0,0);
        m.store32(hunter+0x1F4,28000u^32768u);m.store8(c+0x89,1);
        psprecomp::AllegrexContext native=ctx;native.pc=0x088875E0;native.gpr[2]=1;native.gpr[22]=0;
        unsigned steps=0;
        while(native.pc!=0x08887614 && ++steps<32)psprecomp::interpret_allegrex(runtime,native,1);
        check(native.pc==0x08887614 && m.load16(c+0x80)==14000);
        step(1,0);check(m.load16(c+0x80)>=11817 && m.load16(c+0x80)<=11819);
        // A wall can request correction every frame, including through a stick
        // reversal and after release. It must not accumulate drag or drift.
        for(unsigned frame=0;frame<180;++frame) {
            const auto previous=m.load16(c+0x80);
            m.store8(c+0x89,1);
            native=ctx;native.pc=0x088875E0;native.gpr[2]=1;native.gpr[22]=0;
            steps=0;
            while(native.pc!=0x08887614 && ++steps<32)psprecomp::interpret_allegrex(runtime,native,1);
            check(native.pc==0x08887614);
            const auto collision_flag=m.load8(c+0x89);
            const float input=frame<60?1.f:frame<120?-1.f:0.f;
            step(input,0);
            const auto change=static_cast<std::int16_t>(m.load16(c+0x80)-previous);
            check(input>0?(change>=-183 && change<=-181):
                  input<0?(change>=181 && change<=183):change==0);
            check(m.load8(c+0x89)==collision_flag);
        }
        std::printf("Actual native geometry correction reproduced (+2000); modern request retained (-182).\n");
        std::printf("Repeated native correction: 60 right, 60 left, 60 idle updates passed.\n");
        set_enabled(false);
    }
}
