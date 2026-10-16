#include "save_editor.hpp"
#include "save_data/auto_backup.hpp"
#include "save_data/param_sfo.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace ed=portablekit::fu::editor;
namespace sd=portablekit::savedata;
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
void check(bool ok,const char *what) { if(!ok) throw std::runtime_error(what); }
template<class F> void refused(F action) { bool caught=false; try { action(); } catch(const std::runtime_error &) { caught=true; } check(caught,"Expected edit refusal"); }
void put(Bytes &b,std::size_t at,std::uint32_t value,unsigned size=4) { for(unsigned i=0;i<size;++i) b[at+i]=static_cast<std::uint8_t>(value>>(8*i)); }
std::uint32_t get(const Bytes &b,std::size_t at) { return b[at]|(std::uint32_t(b[at+1])<<8)|(std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24); }
Bytes read(const fs::path &p) { std::ifstream in(p,std::ios::binary); return Bytes(std::istreambuf_iterator<char>(in),{}); }
void write(const fs::path &p,const Bytes &b) { std::ofstream out(p,std::ios::binary|std::ios::trunc); out.write(reinterpret_cast<const char *>(b.data()),b.size()); }
ed::GameData metadata() {
    ed::GameData d;
    // A synthetic permutation, not the game's substitution data.
    for(unsigned i=0;i<256;++i) d.table[i]=static_cast<std::uint8_t>(i*73+41);
    for(unsigned i=0;i<16;++i) d.key[i]=static_cast<std::uint8_t>(i+1);
    d.items={{7,"Synthetic potion"},{8,"Synthetic second item"}}; return d;
}
Bytes fixture() {
    Bytes b(0x16a0fc);
    for(unsigned i=0;i<3;++i) {
        const auto base=i*ed::slot_stride;
        b[base+0x1012]=i==1?2:1; b[base+0x1000]='A'+i; b[base+0x501c]=i;
        put(b,base+ed::zenny_offset,100+i); put(b,base+ed::points_offset,200+i);
        put(b,base+ed::box_offset+0*4,7,2); put(b,base+ed::box_offset+2,10,2);
        put(b,base+ed::box_offset+2*4,8,2); put(b,base+ed::box_offset+2*4+2,5,2);
        b[base+0x60000]=123; // Unrelated progression bytes must survive exactly.
    }
    return b;
}
int main() {
    try {
        const auto d=metadata();
        auto b=fixture();
        {
            auto encoded=*portablekit::fu::encode_save_data(b,d.table,1);
            const auto before=portablekit::fu::decode_save_preview(encoded,d.table);
            auto currency=b; put(currency,ed::zenny_offset,3456);
            const auto after_currency=portablekit::fu::decode_save_preview(*portablekit::fu::encode_save_data(currency,d.table,1),d.table);
            check(before.slots[0].appearance_key.size()==64 && before.slots[0].appearance_key==after_currency.slots[0].appearance_key,"Currency changes retain the equipped-character portrait");
            auto equipment=b; equipment[0x1022]^=1;
            const auto after_equipment=portablekit::fu::decode_save_preview(*portablekit::fu::encode_save_data(equipment,d.table,1),d.table);
            check(before.slots[0].appearance_key!=after_equipment.slots[0].appearance_key && before.slots[1].appearance_key==after_equipment.slots[1].appearance_key,"Equipment changes invalidate only the matching hunter portrait");
        }
        for(auto seed:{0u,0x12345678u,0xffff0000u,0x0000ffffu}) {
            const auto encoded=portablekit::fu::encode_save_data(b,d.table,seed);
            check(encoded.has_value(),"Encode synthetic save");
            const auto decoded=portablekit::fu::decode_save_data(*encoded,d.table);
            check(decoded.has_value(),"Decode newly encoded save");
            check(std::equal(b.begin(),b.begin()+0x16a0e8,decoded->begin()),"Unrelated bytes survive codec");
            check(*portablekit::fu::encode_save_data(*decoded,d.table,seed)==*encoded,"Exact unchanged inner round trip");
            auto corrupt=*encoded; corrupt[300]^=1;
            check(!portablekit::fu::decode_save_data(corrupt,d.table),"Corrupt checksum rejected");
        }
        for(unsigned hunter=0;hunter<3;++hunter) {
            auto edited=b;
            ed::Change c; c.hunter=hunter; c.zenny=9999999; c.points=0; c.item=7; c.quantity=120;
            const auto slots=ed::apply_change(edited,d,c);
            check(slots==std::vector<std::size_t>({1,3}),"First holes filled, no existing-stack merging");
            const auto base=hunter*ed::slot_stride;
            check(get(edited,base+ed::box_offset+4)==(99u<<16|7u),"First new stack is 99");
            check(get(edited,base+ed::box_offset+12)==(21u<<16|7u),"Remainder in next empty slot");
            for(std::size_t at=0;at<b.size();++at) {
                const bool allowed=(at>=base+ed::zenny_offset && at<base+ed::zenny_offset+4) ||
                    (at>=base+ed::points_offset && at<base+ed::points_offset+4) ||
                    (at>=base+ed::box_offset+4 && at<base+ed::box_offset+8) ||
                    (at>=base+ed::box_offset+12 && at<base+ed::box_offset+16);
                check(allowed || b[at]==edited[at],"No other hunter, progress or inventory bytes changed");
            }
        }
        for(int scenario=0;scenario<8;++scenario) {
            auto unchanged=b; ed::Change c; c.zenny=111; c.points=222; c.item=7; c.quantity=1;
            if(scenario==0) c.hunter=3;
            if(scenario==1) c.zenny=10000000;
            if(scenario==2) c.points=10000000;
            if(scenario==3) c.item=65535;
            if(scenario==4) c.quantity=0;
            if(scenario==5) c.quantity=99001;
            if(scenario==6) { unchanged[0x1012]=0; }
            if(scenario==7) put(unchanged,ed::box_offset+2,0,2);
            const auto original=unchanged;
            refused([&]{ed::apply_change(unchanged,d,c);}); check(unchanged==original,"Refused edit changes no bytes");
        }
        for(auto quantity:{100u,255u,65535u}) {
            auto oversized=b;
            put(oversized,ed::box_offset+2,quantity,2);
            const auto before=oversized;
            ed::Change c; c.zenny=123; c.item=7; c.quantity=100;
            check(ed::apply_change(oversized,d,c)==std::vector<std::size_t>({1,3}),"Oversized existing stacks do not prevent editing");
            check(get(oversized,ed::box_offset)==get(before,ed::box_offset),"Existing oversized stack preserved exactly");
            check(get(oversized,ed::box_offset+4)==(99u<<16|7u) && get(oversized,ed::box_offset+12)==(1u<<16|7u),"New stacks remain capped at 99");
            for(std::size_t at=0;at<before.size();++at) {
                const bool allowed=(at>=ed::zenny_offset && at<ed::zenny_offset+4) ||
                    (at>=ed::box_offset+4 && at<ed::box_offset+8) ||
                    (at>=ed::box_offset+12 && at<ed::box_offset+16);
                check(allowed || oversized[at]==before[at],"Oversized-stack edit preserves every unrelated byte");
            }
        }
        auto full=b;
        for(std::size_t i=0;i<1000;++i) { put(full,ed::box_offset+i*4,7,2); put(full,ed::box_offset+i*4+2,99,2); }
        const auto original_full=full;
        ed::Change add; add.item=7; add.quantity=1;
        refused([&]{ed::apply_change(full,d,add);}); check(full==original_full,"Full box remains intact");
        put(full,ed::box_offset+799*4,0);
        add.quantity=100; const auto almost_full=full;
        refused([&]{ed::apply_change(full,d,add);}); check(full==almost_full,"No partial addition when only one slot remains");
        add.quantity=99; check(ed::apply_change(full,d,add)==std::vector<std::size_t>({799}),"Last unlocked slot is usable");
        check(ed::box_capacity(b,0)==800,"Original eight pages respected");
        put(full,0x6b7b4+(829/32)*4,1u<<(829%32));
        check(ed::box_capacity(full,0)==900,"Organizer Guide unlocks ninth page");
        put(full,0x6b7b4+(829/32)*4,(1u<<(829%32))|(1u<<(830%32)));
        check(ed::box_capacity(full,0)==1000,"Both Guides unlock all pages");
        put(full,ed::box_offset+999*4,0);
        check(ed::apply_change(full,d,add)==std::vector<std::size_t>({999}),"Last expanded slot is usable");
        const auto root=fs::temp_directory_path()/("fu-save-editor-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root);
        try {
            for(bool encrypted:{false,true}) {
                const auto stick=root/(encrypted?"encrypted":"plain")/"ms0";
                sd::SaveFiles files{"ULUS10391","","MHP2NDG.BIN",encrypted?std::optional<sd::Block>(d.key):std::nullopt};
                sd::SaveContents contents; contents.data=*portablekit::fu::encode_save_data(b,d.table,0x12345678);
                contents.title="Synthetic title"; contents.savedata_title="Synthetic hunter save";
                std::string error; check(sd::write_save(stick,files,contents,error),"Create synthetic PSP save");
                const auto folder=sd::save_folder(stick,files), file=folder/"MHP2NDG.BIN";
                write(folder/"ICON0.PNG",Bytes{1,2,3,4});
                // Extra SFO fields survive editor transactions.
                auto sfo=*sd::ParamSfo::parse(read(folder/"PARAM.SFO"));
                sfo.set_string("EDITOR_TEST_EXTRA","preserve me",32);
                // The store checks the file hash and inner checksum; it does not
                // need console-specific PARAM.SFO hashes for this fixture.
                write(folder/"PARAM.SFO",sfo.serialize());
                const auto old_file=read(file), old_sfo=read(folder/"PARAM.SFO");
                ed::Session session(file,d);
                check(session.hunters()[2].points==202,"Points read for all three hunters");
                ed::Change c; c.hunter=1; c.points=123456; c.zenny=765432; c.item=8; c.quantity=101;
                const auto backup=session.save(c);
                check(read(backup/"ULUS10391/MHP2NDG.BIN")==old_file && read(backup/"ULUS10391/PARAM.SFO")==old_sfo,"Backup contains exact originals");
                check(read(folder/"ICON0.PNG")==Bytes({1,2,3,4}),"Artwork retained");
                check(sd::ParamSfo::parse(read(folder/"PARAM.SFO"))->string("EDITOR_TEST_EXTRA")==std::optional<std::string>("preserve me"),"Extra metadata retained");
                check(session.hunters()[1].points==123456 && session.hunters()[1].summary.zenny==765432,"Edited hunter reloads");
                check(session.hunters()[0].summary.zenny==100 && session.hunters()[2].summary.zenny==102,"Other hunters unchanged");
                check(portablekit::fu::read_save_preview(stick).status==portablekit::fu::PreviewStatus::Ready,"Preview refreshed after editor save");
                check(portablekit::fu::read_save_preview(stick).slots[1].appearance_key.size()==64,"Portrait identity survives the summary cache round trip");
                auto loaded=sd::load_save(stick,files);
                check(loaded.status==sd::LoadStatus::Ok,"PSP save hash and encryption revalidate");
                const auto decoded=*portablekit::fu::decode_save_data(loaded.contents.data,d.table);
                check(decoded[ed::slot_stride+0x60000]==123,"Progress bytes retained after disk transaction");
                ed::Change first; first.hunter=0; first.zenny=3456; first.item=7; first.quantity=1;
                ed::Change second; second.hunter=1; second.points=6543; second.item=8; second.quantity=100;
                auto invalid=second; invalid.item=65535;
                const auto before_batch=read(file);
                refused([&]{session.save(std::vector<ed::Change>{first,invalid});});
                check(read(file)==before_batch,"Invalid queued item prevents the whole batch from writing");
                const auto batch_backup=session.save(std::vector<ed::Change>{first,second});
                check(read(batch_backup/"ULUS10391/MHP2NDG.BIN")==before_batch,"Batch has one exact original backup");
                ed::Session persisted(file,d);
                check(persisted.hunters()[0].summary.zenny==3456 && persisted.hunters()[1].points==6543,"Both hunters' pending edits persist after reopening");
                check(persisted.hunters()[2].summary.zenny==102 && persisted.hunters()[2].points==202,"Unedited hunter retained after batch");
                // An external change refuses a stale session before backup/write.
                write(folder/"ICON0.PNG",Bytes{9,8,7});
                const auto after=read(file); refused([&]{session.save(c);}); check(read(file)==after,"External changes cannot be overwritten");
                ed::Session reopened(file,d);
                const auto backup_root=stick.parent_path()/"SaveEditorBackups";
                fs::remove_all(backup_root); write(backup_root,Bytes{0});
                c.zenny=1; refused([&]{reopened.save(c);}); check(read(file)==after,"Backup failure leaves original save intact");
                for(const auto &entry:fs::directory_iterator(folder.parent_path())) check(entry.path()==folder,"Failed staging cleaned up");
            }
            fs::remove_all(root);
        } catch(...) { fs::remove_all(root); throw; }
        std::puts("Save editor: codec, all hunters, exact edit scope, box holes/capacity, plain/encrypted transactions, backups, metadata, stale-save and failure checks passed.");
    } catch(const std::exception &e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
