#include "weapon_tree.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace portablekit::fu::weapon_tree;
void check(bool b){if(!b)throw std::runtime_error("Weapon tree regression");}
int main(int argc,char **argv) {
 psprecomp::Runtime r;auto &m=r.memory();auto v=m.aot_fast_view();
 auto weapon=[&](unsigned id){const auto a=0x0895A0EC+id*24;m.store16(a,1);m.store16(a+10,100);};
 m.store8(0x089785A9,0);m.store32(0x0893ACDC,480);m.store32(0x0897935C,0x09000000);
 for(unsigned i=0;i<7;i++)m.store16(0x09000000+i*2,(i+1)*50);
 for(unsigned i=1;i<5;i++)weapon(i);
 auto edge=[&](unsigned parent,unsigned slot,unsigned child){m.store16(0x0894FAE4+parent*28+16+slot*2,child);};
 edge(1,0,2);edge(1,1,3);edge(2,0,4);edge(3,0,4);edge(1,2,2);edge(1,3,1501);
 auto c=build(v,5,2);check(c.nodes.size()==4 && c.edges.size()==4);
 auto w=read_weapon(v,5,1);check(w && w->power==480 && w->sharpness[0]==50 && w->sharpness[3]==0);
 check(!read_weapon(v,0,1) && !read_weapon(v,5,1501));
 edge(4,0,1);check(build(v,5,1).nodes.empty());edge(4,0,0);
 constexpr auto menu=0x09100000u,rows=0x09101000u;
 m.store32(0x09AA5770,0x27BDFDC0);m.store32(0x09AA49F8,0x27BDFFF0);
 m.store8(menu+32,2);m.store32(menu+16,rows);m.store32(menu+24,1);m.store8(rows+10,5);m.store16(rows+12,2);
 psprecomp::AllegrexContext ctx{};ctx.gpr[4]=menu;
 capture_menu(ctx,v);check(!available());set_enabled(true);capture_menu(ctx,v);check(available());
 m.store8(rows+10,0);capture_menu(ctx,v);check(!available());
 // Page two: index past the visible page's row count (menu+24) still selects.
 m.store8(rows+16+10,5);m.store16(rows+16+12,2);m.store32(menu+64,1);capture_menu(ctx,v);check(available());
 m.store32(menu+64,0);capture_menu(ctx,v);check(!available());
 m.store8(rows+10,5);capture_menu(ctx,v);check(available());m.store32(0x09AA5770,0);frame(v);check(!available());
 // Improve Weapon highlights owned equipment, not a 16-byte recipe row.
 m.store32(0x09AA5770,0x27BDFDC0);constexpr auto state=0x09200000u;
 m.store32(0x089CC558,state);m.store8(menu+33,6);m.store8(menu+37,1);
 m.store32(menu+64,103);m.store8(state+0x548+103*12+1,5);m.store16(state+0x548+103*12+2,3);
 ctx.gpr[17]=menu;ctx.gpr[31]=0x09A82140;
 capture_grid_text(ctx,v,0x08891474);check(available() && snapshot()->selected==3);
 m.store8(state+0x548+103*12+1,0);capture_grid_text(ctx,v,0x08891474);check(!available());
 m.store8(state+0x548+103*12+1,5);m.store8(menu+32,1);
 capture_grid_text(ctx,v,0x08891474);check(!available());m.store8(menu+32,2);
 ctx.gpr[31]=0x09A82141;capture_grid_text(ctx,v,0x08891474);check(!available());
 ctx.gpr[31]=0x09A82140;capture_grid_text(ctx,v,0x08891474);check(available());
 set_enabled(false);
 if(argc>1) {
  std::ifstream in(argv[1],std::ios::binary);std::vector<std::uint8_t>b(std::istreambuf_iterator<char>(in),{});
  const auto elf=psprecomp::Elf32Image::from_bytes(b);
  for(const auto &s:elf.segments())if(s.type==1 && s.file_size)m.copy_in(s.vaddr,std::span(b).subspan(s.offset,s.file_size));
  unsigned count=0,edges=0;std::array<unsigned,11> classes{};
  for(unsigned type:{5u,6u})for(unsigned id=1;id<1501;id++)if(auto a=read_weapon(v,type,id)) {
   classes[a->category]++;count++;edges+=unsigned(a->next.size());
   if(type==6 && a->category==10){check(a->charges>=2 && a->charges<=4);for(auto shot:a->shots)check(shot<15);}
   if(type==6 && a->category!=10)check(a->ammo_mask && a->ammo[0]>0 && a->deviation>0);
  }
  check(count>1300 && edges>800);for(auto n:classes)check(n>0);
  auto actual=build(v,5,3);check(actual.nodes.size()>5 && actual.edges.size()>5);
  std::cout<<"Original game data: "<<count<<" weapons, "<<edges<<" upgrade links, all 11 classes; selected component "<<actual.nodes.size()<<" nodes\n";
 }
 std::cout<<"Weapon tree checks passed\n";
}
