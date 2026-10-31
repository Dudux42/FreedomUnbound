#include "modern_camera.hpp"
#include "camera_aim.hpp"
#include "modern_camera_math.hpp"
#include "camera/camera_input.hpp"
#include "psprecomp/runtime.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace portablekit;
using namespace portablekit::fu::modern_camera;
namespace camera_aim = portablekit::fu::camera_aim;
void check(bool ok,const std::source_location at=std::source_location::current()) {
    if(!ok){std::fprintf(stderr,"camera regression line %u\n",at.line());throw std::runtime_error("camera regression");}
}
int main() {
    check(std::abs(math::curve(-.5f,2.f)+.25f)<1e-6f);
    const auto simulate=[](int fps){float velocity=0,angle=0;
        for(int i=0;i<fps;++i)angle+=math::travel(velocity,i<fps/2?90.f:0.f,1.f/fps,.15f);
        return angle;};
    check(std::abs(simulate(30)-simulate(60))<.001f);
    for(float pitch:{-20.f,0.f,30.f,70.f}) {
        const auto g=math::geometry(pitch,490,170);
        check(std::abs(std::hypot(g.height-170,g.distance)-490)<.001f);
    }
    check(math::pitch_step(68,10,-20,70)<70);
    check(math::pitch_step(-18,-10,-20,70)>-20);
    check(math::pitch_step(68,-1,-20,70)==67);
    check(math::framing(0,140,170)==math::Framing::Normal);
    check(math::framing(0,20,30)==math::Framing::Low);
    check(math::framing(3,20,30)==math::Framing::Low);
    check(math::framing(2,400,100)==math::Framing::Excluded);
    check(math::within_grace(250000001,1));check(!math::within_grace(250000002,1));
    psprecomp::Runtime runtime;auto &m=runtime.memory();auto v=m.aot_fast_view();
    constexpr auto r=0x09998E70u,c=r+0x150,hunter=0x090B3580u;
    constexpr auto state=0x09100000u,table=0x0945B000u,stack=0x09F00000u;
    m.store32(0x08887614,0x86820080);m.store32(0x08887834,0xC6600200);m.store32(0x09A6F490,0x27BDFFF0);
    m.store32(0x089CBD04,r);m.store32(0x089CC558,state);m.store8(state+46,2);
    m.store32(r+0xA84,hunter);m.store8(r+0xB0+0x91,1);m.store8(r+0x14,3);
    m.store32(c+0x6C,table);m.store32(c+0x70,table+96);m.store16(hunter+0x29A,98);
    const std::array<std::array<float,3>,5> params{{{400,200,195},{300,160,184},{270,490,170},{140,490,170},{60,450,210}}};
    for(unsigned i=0;i<5;++i)for(unsigned j=0;j<3;++j)
        m.store32(table+(i?i*32:160)+std::array<unsigned,3>{4,8,16}[j],std::bit_cast<std::uint32_t>(params[i][j]));
    psprecomp::AllegrexContext ctx{};ctx.gpr[20]=c;ctx.gpr[19]=hunter;ctx.gpr[29]=stack;
    const auto put=[&](unsigned at,float f){m.store32(at,std::bit_cast<unsigned>(f));};
    const auto height=[&]{return std::bit_cast<float>(m.load32(stack+340));};
    const auto distance=[&]{return std::bit_cast<float>(m.load32(stack+344));};
    auto step=[&](float x,float y){
        const auto preset=m.load8(r+0x14),mode=m.load8(hunter+0x29C);
        put(stack+340,mode==3?20:mode?400:params[preset][0]);
        put(stack+344,mode && mode!=3?400:params[preset][1]);
        put(c+0x54,mode==3?30:mode?100:params[preset][2]);
        camera::add_motion(camera::Source::Stick,x,y);camera::advance(1.f/30,90);
        yaw(ctx,v);geometry(ctx,v);
    };
    prepare(v);check(!driving());set_enabled(true);prepare(v);check(driving());
    m.store16(c+0x80,10000);m.store16(c+0x82,9900);step(0,0);
    check(height()==140);step(1,1);
    check(m.load16(c+0x80)==9818);check(m.load16(c+0x80)-m.load16(c+0x82)==100);
    check(std::abs(std::hypot(height()-170,distance())-490)<.001f);
    // Native wall nudge survives, with the same additive stick delta on both fields.
    const auto previous=m.load16(c+0x80);m.store16(c+0x80,previous+0x400);step(0,0);
    check(m.load16(c+0x80)==previous+0x400);
    step(1,0);check(m.load16(c+0x80)>=previous+0x400-183 && m.load16(c+0x80)<=previous+0x400-181);
    // Physical vertical controls get exactly vanilla scratch geometry.
    m.store16(c+0x84,0x10);m.store8(r+0x14,2);m.store32(c+0x70,table+64);step(9,9);
    check(height()==270 && distance()==490);m.store16(c+0x84,0x100);
    const auto before_l=m.load16(c+0x80);step(10,10);check(m.load16(c+0x80)==before_l && height()==270);
    m.store16(c+0x84,0);step(0,1);
    m.store8(hunter+0x29C,3);step(0,1);
    for(int i=0;i<8;++i)step(0,0);
    check(std::abs(std::hypot(height()-30,distance())-490)<.001f);
    m.store8(hunter+0x29C,2);step(0,0);check(height()!=400);
    for(int i=0;i<8;++i)step(2,2);
    check(height()==400 && distance()==400);
    m.store8(hunter+0x29C,0);step(0,0);
    for(int i=0;i<8;++i)step(0,0);
    // Quest gates must not restore the stick's D-pad mapping, even during aim.
    for(unsigned offset:{0xA8Du,0x961u}) {
        m.store8(r+offset,1);prepare(v);check(driving());
        auto yaw_before=m.load16(c+0x80);step(10,10);check(m.load16(c+0x80)==yaw_before);
        m.store8(r+offset,0);prepare(v);step(0,0);
    }
    for(unsigned phase:{0u,5u,6u,7u}){m.store8(state+46,phase);prepare(v);check(!driving());}
    m.store8(state+46,2);prepare(v);step(0,0);
    prepare(v,false);check(!driving());prepare(v);step(0,0);
    set_enabled(false);
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
    m.store8(r+0xA8D,1); prepare(v); check(driving() && triggers(true,true) == 3);
    m.store8(r+0xA8D,0); prepare(v); step(0,0); check(m.load16(c+0x80) == no_target);
    triggers(false,false);
    triggers(true,false); triggers(false,false,false); step(0,0); check(m.load16(c+0x80) == no_target);
    triggers(true,false); step(0,0); check(m.load16(c+0x80) == no_target); // Held through capture is blocked.
    m.store8(state+46,5); prepare(v); check(target().identity == 0 && triggers(true,true) == 0);
    m.store8(state+46,2); prepare(v); step(0,0); check(m.load16(c+0x80) == no_target);
    set_enabled(false); check(target().identity == 0 && triggers(true,true) == 0);
    // Provider integration: only the right stick, native invert and safe routes.
    set_enabled(true);m.store8(state+46,2);m.store8(r+0xA8D,1);prepare(v);
    Controls aim_options;aim_options.aim_smoothing=0;aim_options.aim_curve=1;
    set_controls(aim_options);configure_aim_input(90,.15f,false,false);
    camera::set_rate(camera::Source::Stick,1,0);
    m.store8(state+0x6ADE2,0);
    auto aim_step=game_camera_aim(hunter,camera_aim::AimRoute::bow,1.f/30);
    check(aim_step.yaw_units==546 && aim_step.pitch_units==0);
    m.store8(state+0x6ADE2,2);
    aim_step=game_camera_aim(hunter,camera_aim::AimRoute::bow,1.f/30);
    check(aim_step.yaw_units<0);
    check(game_camera_aim(hunter,camera_aim::AimRoute::special_c,1.f/30).yaw_units==0);
    check(game_camera_aim(hunter,camera_aim::AimRoute::cone_d,1.f/30).yaw_units==0);
    // B2 uses the separate native invert byte; no nub input is sampled.
    m.store32(hunter+0x614,0);m.store8(state+0x6ADDF,0);
    check(game_camera_aim(hunter,camera_aim::AimRoute::bowgun,1.f/30).yaw_units>0);
    camera::set_rate(camera::Source::Stick,0,-1);m.store8(hunter+0x67E,99);
    check(game_camera_aim(hunter,camera_aim::AimRoute::bowgun,1.f/30).pitch_units==1);
    m.store8(state+0x6ADDF,1);
    check(game_camera_aim(hunter,camera_aim::AimRoute::bowgun,1.f/30).pitch_units<0);
    camera::set_rate(camera::Source::Stick,0,0);
    aim_step=game_camera_aim(hunter,camera_aim::AimRoute::bowgun,1.f/30);
    check(!aim_step.yaw_units && !aim_step.pitch_units);
    aim_options.aim_enabled=false;set_controls(aim_options);camera::set_rate(camera::Source::Stick,1,1);
    aim_step=game_camera_aim(hunter,camera_aim::AimRoute::bow,1.f/30);
    check(!aim_step.yaw_units && !aim_step.pitch_units);
    aim_options.aim_enabled=true;set_controls(aim_options);
    // Actual hook arming, provider registration, and orbit exclusion.
    for(unsigned i=0;i<4;++i)m.store32(camera_aim::bow_site.first+i*4,camera_aim::bow_site.words[i]);
    for(unsigned i=0;i<3;++i)m.store32(camera_aim::pitch_helper+i*4,camera_aim::pitch_helper_words[i]);
    camera_aim::reset();camera_aim::set_provider(game_camera_aim);camera_aim::set_enabled(true);
    ctx.gpr[17]=hunter;camera_aim::bow(ctx,v);check(camera_aim::aiming());
    m.store8(r+0xA8D,0);prepare(v);const auto frozen_yaw=m.load16(c+0x80);
    camera::advance(1.f/30,90);yaw(ctx,v);check(m.load16(c+0x80)==frozen_yaw);
    camera_aim::set_enabled(false);camera_aim::set_provider(nullptr);
    camera::advance(1.f/30,90);yaw(ctx,v);check(m.load16(c+0x80)==frozen_yaw);
    set_enabled(false);check(!driving());
    check(game_camera_aim(hunter,camera_aim::AimRoute::bow,1.f/30).yaw_units==0);
    std::puts("Camera orbit, wall, state, timing, target and aim-provider regressions passed.");
}
