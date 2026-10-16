#include "save_editor.hpp"
#include "fu_text_table.hpp"
#include "kernel/iso_image.hpp"
#include "save_data/auto_backup.hpp"
#include "save_data/param_sfo.hpp"
#include "save_data/savedata_crypto.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/sha256.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>

namespace portablekit::fu::editor {
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
[[noreturn]] void fail(const std::string &message) { throw std::runtime_error(message); }
std::uint32_t u32(std::span<const std::uint8_t> b,std::size_t at) {
    if(at>b.size() || b.size()-at<4) fail("Invalid save or game data extent.");
    return b[at]|(std::uint32_t(b[at+1])<<8)|(std::uint32_t(b[at+2])<<16)|(std::uint32_t(b[at+3])<<24);
}
std::uint16_t u16(std::span<const std::uint8_t> b,std::size_t at) {
    if(at>b.size() || b.size()-at<2) fail("Invalid item-box extent.");
    return b[at]|(std::uint16_t(b[at+1])<<8);
}
void put(Bytes &b,std::size_t at,std::uint32_t value,unsigned size) {
    if(at>b.size() || b.size()-at<size) fail("Invalid save extent.");
    for(unsigned i=0;i<size;++i) b[at+i]=static_cast<std::uint8_t>(value>>(i*8));
}
Bytes read(const fs::path &path,std::uintmax_t limit=16*1024*1024) {
    if(!fs::is_regular_file(path) || fs::is_symlink(path) || fs::file_size(path)>limit)
        fail("Missing, oversized or unsupported file: "+path.filename().string());
    std::ifstream in(path,std::ios::binary);
    Bytes bytes(static_cast<std::size_t>(fs::file_size(path)));
    if(!in.read(reinterpret_cast<char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) fail("Could not read "+path.filename().string());
    return bytes;
}
void write(const fs::path &path,const Bytes &bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size())); out.flush();
    if(!out) fail("Could not write the staged save.");
}
Bytes elf_bytes(const psprecomp::Elf32Image &elf,std::uint32_t address,std::uint32_t size) {
    for(const auto &s:elf.segments()) {
        if(s.type==1 && address>=s.vaddr && std::uint64_t(address)+size<=std::uint64_t(s.vaddr)+s.file_size) {
            const auto off=std::uint64_t(s.offset)+address-s.vaddr;
            if(off+size>elf.bytes().size()) break;
            return Bytes(elf.bytes().begin()+off,elf.bytes().begin()+off+size);
        }
    }
    fail("The installed executable is missing required game data.");
}
// Measured from the USA archive reader at 0884EA1C / 0884EC3C / 0884ECAC.
// The substitution bytes are read from the owner's ELF, never compiled in.
void decrypt_archive(Bytes &bytes,std::uint32_t seed,const Bytes &table) {
    if(bytes.size()%4 || table.size()!=256) fail("Invalid archive block.");
    std::uint32_t lo=seed&65535,hi=seed>>16;
    if(!lo) lo=0x7f8d;
    if(!hi) hi=0x2345;
    for(std::size_t at=0;at<bytes.size();at+=4) {
        lo=(lo*0x7f8d)%0xfff1; hi=(hi*0x2345)%0xffd9;
        const auto mask=lo|(hi<<16);
        for(unsigned i=0;i<4;++i) bytes[at+i]=table[bytes[at+i]]^static_cast<std::uint8_t>(mask>>(i*8));
    }
}
std::string fingerprint(const fs::path &folder) {
    if(!fs::is_directory(folder) || fs::is_symlink(folder)) fail("Invalid save folder.");
    std::map<std::string,std::string> hashes;
    for(const auto &entry:fs::recursive_directory_iterator(folder)) {
        if(entry.is_symlink()) fail("Linked files are not supported in a save folder.");
        if(entry.is_regular_file()) {
            if(entry.file_size()>16*1024*1024 || hashes.size()>32) fail("Unexpected save-folder contents.");
            hashes[fs::relative(entry.path(),folder).generic_string()]=psprecomp::sha256_bytes(read(entry.path()));
        }
    }
    std::string joined;
    for(const auto &[name,hash]:hashes) joined+=name+'\n'+hash+'\n';
    return psprecomp::sha256_bytes(std::span(reinterpret_cast<const std::uint8_t *>(joined.data()),joined.size()));
}
std::size_t validate_box(std::span<const std::uint8_t> plain,unsigned hunter) {
    std::size_t free=0;
    const auto capacity=box_capacity(plain,hunter);
    for(std::size_t i=0;i<box_slots;++i) {
        const auto at=hunter*slot_stride+box_offset+i*4;
        const auto id=u16(plain,at),quantity=u16(plain,at+2);
        if(!id && !quantity) { if(i<capacity) ++free; }
        // Existing saves can contain larger stacks (for example after cheats).
        // Preserve their full uint16 quantity; stack_limit only governs additions.
        else if(!id || id>1260 || !quantity) fail("This hunter has an invalid item-box entry. No changes were made.");
    }
    return free;
}
}
std::size_t box_capacity(std::span<const std::uint8_t> plain,unsigned hunter) {
    if(hunter>=3 || plain.size()!=0x16a0fc) fail("Invalid hunter or save size.");
    // 08850E58 / 08850EC4: 8 pages, plus one per owned Guide (829/830).
    // 08857C98 reads their persistent bitset at character + 6A7B4;
    // the serialized character starts at slot_stride * hunter + 1000.
    std::size_t pages=8;
    for(unsigned id:{829u,830u})
        if(u32(plain,hunter*slot_stride+0x6b7b4+(id/32)*4)&(1u<<(id%32))) ++pages;
    return pages*100;
}
GameData read_game_data(const fs::path &elf_path,const fs::path &iso_path) {
    const auto bytes=read(elf_path,32*1024*1024);
    if(psprecomp::sha256_bytes(bytes)!="3070954e2dc88fd0b0836c0c1dc526531aca4a213b0f18806e324963efb514f4")
        fail("Choose EBOOT.ELF from a supported USA ULUS10391 Freedom Unbound installation.");
    const auto elf=psprecomp::Elf32Image::from_bytes(bytes);
    GameData result;
    const auto table=elf_bytes(elf,0x089b2db4,256);
    std::copy(table.begin(),table.end(),result.table.begin());
    if(!valid_save_table(result.table)) fail("Invalid save substitution table.");
    // Recover the immediate byte stores that build the game's savedata key.
    // No key is embedded in the editor, persisted, logged or distributed.
    const auto code=elf_bytes(elf,0x088c175c,0x1800-0x175c);
    std::array<std::optional<std::uint32_t>,32> regs{}; regs[0]=0;
    std::array<bool,16> found{};
    for(std::size_t at=0;at<code.size();at+=4) {
        const auto op=u32(code,at),opcode=op>>26,rs=(op>>21)&31,rt=(op>>16)&31;
        const auto immediate=static_cast<std::int16_t>(op&65535);
        if(opcode==9 && regs[rs]) regs[rt]=*regs[rs]+static_cast<std::uint32_t>(immediate);
        else if(opcode==40 && rs==16 && immediate>=1524 && immediate<1540 && regs[rt]) {
            result.key[immediate-1524]=static_cast<std::uint8_t>(*regs[rt]); found[immediate-1524]=true;
        }
    }
    if(std::find(found.begin(),found.end(),false)!=found.end()) fail("Could not read the installed game's save key.");
    IsoImage iso(iso_path);
    const auto identity=iso.find("PSP_GAME/PARAM.SFO");
    if(!identity || identity->directory || identity->size>64*1024) fail("The selected ISO has no valid disc identity.");
    Bytes identity_bytes(identity->size);
    if(iso.read(std::uint64_t(identity->lba)*2048,identity_bytes)!=identity_bytes.size()) fail("Could not read the disc identity.");
    const auto disc=savedata::ParamSfo::parse(identity_bytes);
    if(!disc || disc->string("DISC_ID")!=std::optional<std::string>("ULUS10391")) fail("Choose the supported USA Freedom Unite ISO (ULUS10391).");
    const auto entry=iso.find("PSP_GAME/USRDIR/DATA.BIN");
    if(!entry || entry->directory || entry->size<0x6808) fail("The selected ISO has no supported DATA.BIN.");
    const auto substitution=elf_bytes(elf,0x0899a138,256);
    Bytes toc(0x6808);
    const auto archive_start=std::uint64_t(entry->lba)*2048;
    if(iso.read(archive_start,toc)!=toc.size()) fail("Could not read the item-name archive.");
    decrypt_archive(toc,0,substitution);
    const auto begin=u32(toc,19*4),end=u32(toc,20*4);
    if(!begin || end<=begin || std::uint64_t(end)*2048>entry->size || end-begin>512)
        fail("The selected ISO has an unsupported item-name table.");
    Bytes text_bytes((end-begin)*2048);
    if(iso.read(archive_start+std::uint64_t(begin)*2048,text_bytes)!=text_bytes.size()) fail("Incomplete item-name table.");
    decrypt_archive(text_bytes,begin,substitution);
    const auto names=text::read_table(text_bytes);
    if(!names || names->sections.size()!=23 || names->sections[1].size()!=1262)
        fail("The selected game image has an unsupported item catalogue.");
    for(unsigned id=1;id<=1260;++id) {
        const auto &name=names->sections[1][id];
        const auto flag=elf_bytes(elf,0x0899d2bb+id*24,1)[0];
        if(name && !name->empty() && name->size()<128 && flag!=255 && *name!="Dummy")
            result.items.push_back({static_cast<std::uint16_t>(id),*name});
    }
    if(result.items.empty()) fail("No usable items were found in the installed game.");
    return result;
}
std::vector<std::size_t> apply_change(Bytes &plain,const GameData &data,const Change &change) {
    if(plain.size()!=0x16a0fc || change.hunter>=3) fail("Invalid hunter or save size.");
    const auto base=change.hunter*slot_stride;
    if(plain[base+0x1012]<1 || plain[base+0x1012]>2) fail("Choose an occupied hunter slot.");
    if((change.zenny && *change.zenny>currency_limit) || (change.points && *change.points>currency_limit))
        fail("Zenny and Pokke Points must be between 0 and 9,999,999.");
    const auto free=validate_box(plain,change.hunter);
    std::vector<std::size_t> slots;
    if(change.item) {
        if(std::none_of(data.items.begin(),data.items.end(),[&](const Item &i){return i.id==*change.item;})) fail("Choose an item from the list.");
        if(!change.quantity || change.quantity>box_slots*stack_limit) fail("Quantity must be between 1 and 99,000.");
        const auto needed=(change.quantity+stack_limit-1)/stack_limit;
        if(needed>free) fail("Not enough empty item-box slots. No items were added.");
        for(std::size_t i=0;i<box_capacity(plain,change.hunter) && slots.size()<needed;++i)
            if(!u16(plain,base+box_offset+i*4)) slots.push_back(i);
    } else if(change.quantity) fail("Choose an item before entering a quantity.");
    if(change.zenny) put(plain,base+zenny_offset,*change.zenny,4);
    if(change.points) put(plain,base+points_offset,*change.points,4);
    auto remaining=change.quantity;
    for(const auto i:slots) {
        const auto amount=std::min(remaining,stack_limit);
        const auto at=base+box_offset+i*4;
        put(plain,at,*change.item,2); put(plain,at+2,amount,2); remaining-=amount;
    }
    return slots;
}
Session::Session(const fs::path &save,GameData data):data_(std::move(data)) {
    const auto path=fs::absolute(save).lexically_normal();
    if(path.filename()!="MHP2NDG.BIN" || path.parent_path().filename()!="ULUS10391" ||
       path.parent_path().parent_path().filename()!="SAVEDATA" || path.parent_path().parent_path().parent_path().filename()!="PSP")
        fail("Choose ULUS10391/MHP2NDG.BIN inside an ms0/PSP/SAVEDATA folder.");
    stick_=path.parent_path().parent_path().parent_path().parent_path();
    files_={"ULUS10391","","MHP2NDG.BIN",data_.key};
    refresh();
}
fs::path Session::save_file() const { return savedata::save_folder(stick_,files_)/files_.file_name; }
void Session::refresh() {
    const auto folder=savedata::save_folder(stick_,files_);
    const auto before=fingerprint(folder);
    const auto sfo=savedata::ParamSfo::parse(read(folder/"PARAM.SFO",64*1024));
    if(!sfo || sfo->string("SAVEDATA_DIRECTORY")!=std::optional<std::string>("ULUS10391")) fail("Not a supported USA hunter save.");
    const auto *params=sfo->binary("SAVEDATA_PARAMS");
    files_.key=(params && !params->empty() && (*params)[0]) ? std::optional<savedata::Block>(data_.key) : std::nullopt;
    auto loaded=savedata::load_save(stick_,files_);
    if(loaded.status!=savedata::LoadStatus::Ok) fail("Could not open this save: "+loaded.reason);
    auto plain=decode_save_data(loaded.contents.data,data_.table);
    const auto preview=decode_save_preview(loaded.contents.data,data_.table);
    if(!plain || preview.status!=PreviewStatus::Ready) fail("The save failed its game checksum or hunter validation.");
    std::array<Hunter,3> hunters{};
    for(unsigned i=0;i<3;++i) {
        hunters[i].summary=preview.slots[i];
        if(!preview.slots[i].occupied) continue;
        hunters[i].points=u32(*plain,i*slot_stride+points_offset);
        if(hunters[i].points>currency_limit || preview.slots[i].zenny>currency_limit) fail("Unsupported currency values in this save.");
        hunters[i].free_slots=validate_box(*plain,i);
        hunters[i].capacity=box_capacity(*plain,i);
    }
    if(before!=fingerprint(folder)) fail("The save changed while opening it. Close the game and open it again.");
    contents_=std::move(loaded.contents); plain_=std::move(*plain); hunters_=std::move(hunters); fingerprint_=before;
}
fs::path Session::save(const Change &change) {
    return save(std::vector<Change>{change});
}
fs::path Session::save(const std::vector<Change> &changes) {
    const auto folder=savedata::save_folder(stick_,files_);
    if(fingerprint(folder)!=fingerprint_) fail("The save changed since it was opened. Close the game and open the save again.");
    auto next=plain_;
    for(const auto &change:changes) apply_change(next,data_,change);
    if(next==plain_) fail("No changes to save.");
    const auto &old=contents_.data;
    std::uint32_t seed=0;
    for(unsigned i=0;i<4;++i) seed|=std::uint32_t(data_.table[data_.table[old[old.size()-4+i]]])<<(i*8);
    const auto encoded=encode_save_data(next,data_.table,seed);
    if(!encoded) fail("Could not encode the edited save.");
    const auto token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto stage=folder.parent_path()/(".fu-editor-stage-"+token);
    const auto rollback=folder.parent_path()/(".fu-editor-original-"+token);
    const auto staged_folder=savedata::save_folder(stage,files_);
    bool moved_original=false;
    fs::path backup_path;
    try {
        fs::create_directories(staged_folder);
        fs::copy(folder,staged_folder,fs::copy_options::recursive|fs::copy_options::overwrite_existing);
        auto edited=contents_; edited.data=*encoded;
        std::string error;
        if(!savedata::write_save(stage,files_,edited,error)) fail("Could not stage the edited save: "+error);
        // Keep any extra PARAM.SFO fields; only the data-file hash and crypto
        // parameters change. Re-sign the complete preserved SFO when encrypted.
        auto preserved=*savedata::ParamSfo::parse(read(folder/"PARAM.SFO",64*1024));
        const auto generated=*savedata::ParamSfo::parse(read(staged_folder/"PARAM.SFO",64*1024));
        for(const auto *key:{"SAVEDATA_FILE_LIST","SAVEDATA_PARAMS"}) {
            const auto *value=generated.binary(key);
            if(!value) fail("Missing staged save metadata.");
            preserved.set_binary(key,*value,static_cast<std::uint32_t>(value->size()));
        }
        auto sfo_bytes=preserved.serialize();
        if(files_.key) savedata::sign_param_sfo(sfo_bytes,*preserved.data_offset("SAVEDATA_PARAMS"),savedata::CryptMode::Mode5);
        write(staged_folder/"PARAM.SFO",sfo_bytes);
        const auto checked=savedata::load_save(stage,files_);
        if(checked.status!=savedata::LoadStatus::Ok || checked.contents.data!=*encoded || !decode_save_data(checked.contents.data,data_.table))
            fail("The staged edit failed integrity verification. The original save is unchanged.");
        savedata::auto_backup::Config config{folder,preview_file(stick_),stick_.parent_path()/"SaveEditorBackups",std::numeric_limits<std::size_t>::max()};
        const auto backup=savedata::auto_backup::back_up(config,std::chrono::system_clock::now());
        if(!backup.ok || backup.skipped || !savedata::auto_backup::same_contents(folder,backup.folder/"ULUS10391"))
            fail("Could not verify an automatic backup. No changes were written.");
        backup_path=backup.folder;
        if(fingerprint(folder)!=fingerprint_) fail("The save changed during editing. The edit was cancelled.");
        fs::rename(folder,rollback); moved_original=true;
        fs::rename(staged_folder,folder);
    } catch(...) {
        if(moved_original && !fs::exists(folder)) {
            std::error_code ec; fs::rename(rollback,folder,ec);
            if(ec) fail("Could not restore the original folder. Recover it from "+rollback.string()+" or "+backup_path.string());
        }
        std::error_code ec; fs::remove_all(stage,ec);
        throw;
    }
    std::error_code ec; fs::remove_all(rollback,ec); fs::remove_all(stage,ec);
    record_save_preview(data_.table,stick_,"ULUS10391","MHP2NDG.BIN",*encoded);
    refresh();
    return backup_path;
}
}
