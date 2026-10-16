#include "save_editor.hpp"
#include "equipment_style.hpp"
#include <cstdio>
#include <iostream>
#include "save_data/param_sfo.hpp"
#include <fstream>
#include <algorithm>
#include <set>
#include <tuple>
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
int main(int argc,char **argv) {
    try {
        if(argc<5) return 2;
        auto game=portablekit::fu::editor::read_game_data(argv[2],argv[3]);
        std::cout<<"items="<<game.items.size()<<'\n';
        if(std::string(argv[1])=="inspect-box" || std::string(argv[1])=="inspect-points") {
            namespace ed=portablekit::fu::editor;
            namespace sd=portablekit::savedata;
            const std::filesystem::path file=argv[4], folder=file.parent_path();
            std::ifstream in(folder/"PARAM.SFO",std::ios::binary);
            std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in),{});
            const auto sfo=sd::ParamSfo::parse(bytes);
            const auto *params=sfo->binary("SAVEDATA_PARAMS");
            sd::SaveFiles files{"ULUS10391","","MHP2NDG.BIN",params && !params->empty() && (*params)[0]?std::optional<sd::Block>(game.key):std::nullopt};
            const auto loaded=sd::load_save(folder.parent_path().parent_path().parent_path(),files);
            if(loaded.status!=sd::LoadStatus::Ok) throw std::runtime_error(loaded.reason);
            const auto plain=portablekit::fu::decode_save_data(loaded.contents.data,game.table);
            if(!plain) throw std::runtime_error("Checksum failed");
            if(argc>5) {
                std::ofstream out(argv[5],std::ios::binary);
                out.write(reinterpret_cast<const char *>(plain->data()),plain->size());
                if(!out) throw std::runtime_error("Could not write diagnostic copy");
            }
            const auto get=[&](std::size_t at){return (*plain)[at]|(unsigned((*plain)[at+1])<<8);};
            for(unsigned h=0;h<3;++h) {
                if((*plain)[h*ed::slot_stride+0x1012]<1 || (*plain)[h*ed::slot_stride+0x1012]>2) continue;
                const auto capacity=ed::box_capacity(*plain,h);
                std::cout<<"hunter="<<h+1<<" capacity="<<capacity<<'\n';
                if(std::string(argv[1])=="inspect-points") {
                    for(unsigned off=0x6a230;off<=0x6a260;off+=4) {
                        const auto at=h*ed::slot_stride+off;
                        std::cout<<std::hex<<off<<std::dec<<'='<<(get(at)|(get(at+2)<<16))<<' ';
                    }
                    std::cout<<'\n'; continue;
                }
                for(unsigned i=0;i<1000;++i) {
                    const auto at=h*ed::slot_stride+ed::box_offset+i*4;
                    const auto id=get(at),qty=get(at+2);
                    if((id || qty) && (!id || id>1260 || !qty || qty>99))
                        std::cout<<"index="<<i<<" id="<<id<<" quantity="<<qty<<" unlocked="<<(i<capacity)<<'\n';
                }
            }
            return 0;
        }
        portablekit::fu::editor::Session session(argv[4],std::move(game));
        if(std::string(argv[1])=="verify-equipment") {
            using namespace portablekit::fu;
            std::ifstream in(argv[2],std::ios::binary);
            std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in),{});
            const auto elf=psprecomp::Elf32Image::from_bytes(bytes);
            const auto load=[&](psprecomp::Runtime &runtime) {
                for(const auto &s:elf.segments()) if(s.type==1 && s.file_size)
                    runtime.memory().copy_in(s.vaddr,std::span(bytes).subspan(s.offset,s.file_size));
            };
            psprecomp::Runtime reference;load(reference);
            std::vector<EquippedItem> samples;
            for(const auto &h:session.hunters()) if(h.summary.occupied)
                for(const auto &item:h.summary.equipment) samples.push_back(item);
            std::set<std::tuple<unsigned,unsigned,unsigned>> symbols;
            std::set<unsigned> rarities;
            for(unsigned type=5;type<=6;++type) for(unsigned id=1;id<1501;++id) {
                const EquippedItem item{true,type,id};
                if(auto style=equipment::read_style(reference.memory(),item)) {
                    const auto rarity=reference.memory().load8((type==5?0x0895a0ec+24*id:0x08960cbc+28*id)+2);
                    const bool new_symbol=symbols.insert({style->page,style->x,style->y}).second;
                    const bool new_rarity=rarities.insert(rarity).second;
                    if(new_symbol || new_rarity) samples.push_back(item);
                }
            }
            unsigned checked=0;
            for(const auto &item:samples) {
                const auto style=equipment::read_style(reference.memory(),item);
                if(!style) throw std::runtime_error("Missing style");
                const auto original=[&](unsigned pc,unsigned a1,unsigned a2,unsigned a3,bool record) {
                    psprecomp::Runtime runtime;load(runtime);
                    constexpr unsigned exit=0x08804000,gear=0x09000000;
                    runtime.memory().store8(gear,1);runtime.memory().store8(gear+1,item.type);runtime.memory().store16(gear+2,item.id);
                    runtime.register_function(exit,[](psprecomp::CorpusRuntime &r,psprecomp::AllegrexContext &){static_cast<psprecomp::Runtime &>(r).stop("returned");},"equipment-test-return");
                    runtime.cpu().gpr[5]=record?gear:a1;runtime.cpu().gpr[6]=a2;runtime.cpu().gpr[7]=a3;
                    runtime.cpu().gpr[29]=0x09fff000;runtime.cpu().gpr[31]=exit;
                    runtime.run(pc,1000);
                    if(runtime.stop_reason()!="returned") throw std::runtime_error("Original equipment routine did not return");
                    return runtime.cpu().gpr[2];
                };
                const auto icon=original(0x0883029c,0,0,0,true);
                const auto rect=0x089392c8+icon*12;
                const auto &m=reference.memory();
                if(style->page!=(m.load16(rect+8)&4095) || style->x!=m.load16(rect) || style->y!=m.load16(rect+2) ||
                   style->width!=m.load16(rect+4) || style->height!=m.load16(rect+6)) throw std::runtime_error("Original icon mismatch");
                const auto rarity=original(0x0885c83c,item.type,item.id,0,false);
                const auto color=original(0x08830354,rarity,255,0,false);
                const auto q=[](unsigned v){return ((v&255)>>3)*255/31;};
                const auto tint=0xff000000u|q(color>>16)|(q(color>>8)<<8)|(q(color)<<16);
                if(style->tint!=tint) throw std::runtime_error("Original rarity color mismatch");
                ++checked;
            }
            std::cout<<"Original game icon/color routines: "<<checked<<" equipment samples, "<<symbols.size()<<" weapon classes, "<<rarities.size()<<" rarities PASS\n";
            return 0;
        }
        if(std::string(argv[1])=="verify-points") {
            std::ifstream in(argv[2],std::ios::binary);
            std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in),{});
            const auto elf=psprecomp::Elf32Image::from_bytes(bytes);
            constexpr unsigned entry=0x0884fc0c,exit=0x08804000,state=0x09000000;
            for(unsigned h=0;h<3;++h) if(session.hunters()[h].summary.occupied) {
                for(int delta:{10,-10}) {
                    psprecomp::Runtime runtime;
                    bool copied=false;
                    for(const auto &s:elf.segments()) if(s.type==1 && entry>=s.vaddr && entry+100<=s.vaddr+s.file_size) {
                        runtime.memory().copy_in(entry,std::span(bytes).subspan(s.offset+entry-s.vaddr,100)); copied=true;
                    }
                    if(!copied) throw std::runtime_error("Missing original point-award routine");
                    runtime.register_function(exit,[](psprecomp::CorpusRuntime &r,psprecomp::AllegrexContext &){static_cast<psprecomp::Runtime &>(r).stop("returned");},"point-test-return");
                    const auto before=session.hunters()[h].points;
                    runtime.memory().store32(state+0x696e8,before);
                    runtime.cpu().gpr[4]=state; runtime.cpu().gpr[5]=static_cast<unsigned>(delta); runtime.cpu().gpr[31]=exit;
                    runtime.run(entry,8);
                    const auto after=runtime.memory().load32(state+0x696e8);
                    const auto expected=static_cast<unsigned>(std::clamp<long long>(static_cast<long long>(before)+delta,0,9999999));
                    if(runtime.stop_reason()!="returned" || after!=expected) throw std::runtime_error("Original game's point update failed");
                    std::cout<<"hunter="<<h+1<<" before="<<before<<" delta="<<delta<<" after="<<after<<" original-game-routine=PASS\n";
                }
            }
        }
        if(std::string(argv[1])=="edit-copy") {
            portablekit::fu::editor::Change c; c.hunter=0; c.zenny=543210; c.points=12345; c.item=7; c.quantity=120;
            std::cout<<"backup="<<session.save(c).string()<<'\n';
        }
        for(unsigned i=0;i<3;++i) {
            const auto &h=session.hunters()[i];
            if(h.summary.occupied) std::cout<<"slot="<<i+1<<" zenny="<<h.summary.zenny<<" points="<<h.points<<" free="<<h.free_slots<<'\n';
        }
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
