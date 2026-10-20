#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include "save_editor.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace ed=portablekit::fu::editor;
enum Id { OpenSave=101,GameFile,Hunter,Zenny,Points,UnusedCurrency,Search,Items,Quantity,Add,Backups,Save,Profile };
HWND window{},save_path{},game_path{},hunter{},zenny{},points{},search{},items{},quantity{},status{},free_slots{},add_button{},backup_button{},save_button{},target_label{},profile_picker{};
HFONT font{};
fs::path selected_save,selected_elf,selected_iso,last_backup;
std::unique_ptr<ed::Session> session;
std::vector<unsigned> hunter_indices;
std::vector<fs::path> profiles;
bool initializing{};
bool diagnostic_mode{};
unsigned displayed_hunter{};
std::array<std::wstring,3> draft_zenny,draft_points;
std::vector<ed::Change> pending_items;
std::wstring value(HWND control);
void remember_fields() {
    if(session && !hunter_indices.empty()) {
        draft_zenny[displayed_hunter]=value(zenny);
        draft_points[displayed_hunter]=value(points);
    }
}

std::ofstream startup_trace;
void trace(const char *step) { if(startup_trace) { startup_trace<<step<<'\n'; startup_trace.flush(); } }
std::wstring wide(const std::string &s) {
    if(s.empty()) return {};
    const int size=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);
    std::wstring out(size,0); MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),size); return out;
}
std::wstring value(HWND control) {
    std::wstring text(GetWindowTextLengthW(control)+1,0);
    GetWindowTextW(control,text.data(),static_cast<int>(text.size())); text.resize(text.size()-1); return text;
}
void message(const std::wstring &text) { SetWindowTextW(status,text.c_str()); }
void error(const std::exception &e) { message(wide(e.what())); }
HWND control(const wchar_t *type,const wchar_t *text,DWORD style,int x,int y,int w,int h,int id=0,DWORD extended=0) {
    auto handle=CreateWindowExW(extended,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(handle,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return handle;
}
void label(const wchar_t *text,int x,int y,int w=190) { control(L"STATIC",text,0,x,y,w,24); }
fs::path pick(const wchar_t *title,const wchar_t *filter,const fs::path &start={}) {
    std::wstring buffer(32768,0);
    if(!start.empty()) { const auto path=start.wstring(); if(path.size()<buffer.size()) std::copy(path.begin(),path.end(),buffer.begin()); }
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=window;
    dialog.lpstrTitle=title; dialog.lpstrFilter=filter; dialog.lpstrFile=buffer.data(); dialog.nMaxFile=static_cast<DWORD>(buffer.size());
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog)) return {};
    return fs::path(buffer.c_str());
}
unsigned chosen_hunter() {
    const auto index=SendMessageW(hunter,CB_GETCURSEL,0,0);
    if(index<0 || static_cast<std::size_t>(index)>=hunter_indices.size()) throw std::runtime_error("Choose a hunter.");
    return hunter_indices[index];
}
void update_hunter() {
    if(!session || hunter_indices.empty()) return;
    const auto &h=session->hunters()[chosen_hunter()];
    displayed_hunter=chosen_hunter();
    SetWindowTextW(zenny,draft_zenny[displayed_hunter].c_str());
    SetWindowTextW(points,draft_points[displayed_hunter].c_str());
    const auto profile=session->save_file().parent_path().parent_path().parent_path().parent_path().parent_path().filename().wstring();
    SetWindowTextW(target_label,(L"Save target: "+profile+L" / "+wide(h.summary.name)).c_str());
    std::size_t queued=0;
    for(const auto &c:pending_items) if(c.hunter==displayed_hunter) queued+=(c.quantity+98)/99;
    SetWindowTextW(free_slots,(L"Empty item-box slots after pending additions: "+std::to_wstring(h.free_slots-queued)+L" / "+std::to_wstring(h.capacity)).c_str());
}
void filter_items() {
    if(!session) return;
    auto term=value(search); if(!term.empty()) CharLowerBuffW(term.data(),static_cast<DWORD>(term.size()));
    SendMessageW(items,CB_RESETCONTENT,0,0);
    for(const auto &item:session->game_data().items) {
        auto name=wide(item.name); auto lower=name; CharLowerBuffW(lower.data(),static_cast<DWORD>(lower.size()));
        if(!term.empty() && lower.find(term)==std::wstring::npos && std::to_wstring(item.id).find(term)==std::wstring::npos) continue;
        name+=L"  ["+std::to_wstring(item.id)+L"]";
        const auto row=SendMessageW(items,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
        SendMessageW(items,CB_SETITEMDATA,row,item.id);
    }
    SendMessageW(items,CB_SETCURSEL,0,0);
}
void set_ready(bool ready) {
    for(auto handle:{hunter,zenny,points,search,items,quantity,save_button,add_button}) EnableWindow(handle,ready);
}
fs::path iso_from_settings(const fs::path &data) {
    std::ifstream in(data/L"settings.ini"); std::string line;
    while(std::getline(in,line)) if(line.rfind("disc_image=",0)==0) {
        auto text=line.substr(11); if(!text.empty() && text.back()=='\r') text.pop_back();
        fs::path iso(wide(text)); return iso.is_absolute()?iso:data/iso;
    }
    return data/L"disc.iso";
}
void open_session() {
    session.reset(); pending_items.clear(); hunter_indices.clear(); set_ready(false);
    SendMessageW(hunter,CB_RESETCONTENT,0,0);
    SetWindowTextW(save_path,selected_save.wstring().c_str());
    SetWindowTextW(game_path,selected_elf.wstring().c_str());
    SendMessageW(profile_picker,CB_SETCURSEL,static_cast<WPARAM>(-1),0);
    for(std::size_t i=0;i<profiles.size();++i)
        if(selected_save==profiles[i]/L"ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN") SendMessageW(profile_picker,CB_SETCURSEL,i,0);
    if(selected_save.empty() || selected_elf.empty()) { message(L"Choose a save and the EBOOT.ELF prepared by Freedom Unbound from your USA game image."); return; }
    try {
        if(selected_iso.empty() || !fs::is_regular_file(selected_iso)) selected_iso=iso_from_settings(selected_elf.parent_path());
        if(!fs::is_regular_file(selected_iso)) {
            selected_iso=pick(L"Choose your USA Freedom Unite ISO",L"PSP disc image (*.iso)\0*.iso\0\0");
            if(selected_iso.empty()) { message(L"An ISO is needed to read the item names. No save changes were made."); return; }
        }
        auto data=ed::read_game_data(selected_elf,selected_iso);
        session=std::make_unique<ed::Session>(selected_save,std::move(data));
        for(unsigned i=0;i<3;++i) {
            draft_zenny[i]=std::to_wstring(session->hunters()[i].summary.zenny);
            draft_points[i]=std::to_wstring(session->hunters()[i].points);
        }
        initializing=true;
        SendMessageW(hunter,CB_RESETCONTENT,0,0); hunter_indices.clear();
        for(unsigned i=0;i<3;++i) if(session->hunters()[i].summary.occupied) {
            const auto name=L"Slot "+std::to_wstring(i+1)+L" - "+wide(session->hunters()[i].summary.name);
            SendMessageW(hunter,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str())); hunter_indices.push_back(i);
        }
        SendMessageW(hunter,CB_SETCURSEL,0,0);
        initializing=false;
        set_ready(!hunter_indices.empty()); update_hunter(); filter_items();
        message(hunter_indices.empty()?L"This save has no hunters to edit.":L"Save opened. Add items, edit currency, then click Save to write your changes.");
    } catch(const std::exception &e) { initializing=false; error(e); }
}
bool game_running() {
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error("Could not check whether the game is running. No changes were written.");
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry); bool found=false;
    if(!Process32FirstW(snapshot,&entry)) { CloseHandle(snapshot); throw std::runtime_error("Could not check game processes."); }
    do { if(_wcsicmp(entry.szExeFile,L"FreedomUniteNative.exe")==0 || _wcsicmp(entry.szExeFile,L"FreedomUnbound.exe")==0) { found=true; break; } } while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot); return found;
}
std::uint32_t number(HWND edit,std::uint32_t max,std::uint32_t min=0) {
    const auto text=value(edit);
    if(text.empty() || text.size()>10) throw std::runtime_error("Enter a whole number within the shown limits.");
    std::uint64_t result=0;
    for(const wchar_t c:text) { if(c<L'0' || c>L'9') throw std::runtime_error("Enter a positive whole number (or zero for currency)."); result=result*10+c-L'0'; }
    if(result<min || result>max) throw std::runtime_error("The number is outside the shown limits.");
    return static_cast<std::uint32_t>(result);
}
bool dirty() {
    if(!session || hunter_indices.empty()) return false;
    remember_fields();
    if(!pending_items.empty()) return true;
    for(auto i:hunter_indices)
        if(draft_zenny[i]!=std::to_wstring(session->hunters()[i].summary.zenny) || draft_points[i]!=std::to_wstring(session->hunters()[i].points)) return true;
    return false;
}
bool allow_discard() {
    return !dirty() || MessageBoxW(window,L"Discard unsaved changes? Click Save first to keep them.",L"Unsaved changes",MB_YESNO|MB_ICONQUESTION)==IDYES;
}
void queue_item() {
    if(!session) return;
    try {
        const auto selected=SendMessageW(items,CB_GETCURSEL,0,0);
        if(selected<0) throw std::runtime_error("Choose an item from the list.");
        ed::Change c; c.hunter=chosen_hunter();
        c.item=static_cast<std::uint16_t>(SendMessageW(items,CB_GETITEMDATA,selected,0));
        c.quantity=number(quantity,99000,1);
        std::size_t used=(c.quantity+98)/99;
        for(const auto &p:pending_items) if(p.hunter==c.hunter) used+=(p.quantity+98)/99;
        if(used>session->hunters()[c.hunter].free_slots) throw std::runtime_error("Not enough empty item-box slots. No items were queued.");
        remember_fields(); pending_items.push_back(c); update_hunter();
        message(L"Item addition queued. Click Save to write all pending changes.");
    } catch(const std::exception &e) { error(e); }
}
bool save_changes() {
    if(!session) return false;
    try {
        if(game_running()) throw std::runtime_error("Save blocked: close Freedom Unbound completely, including its launcher, then click Save again. Your pending changes are still in the editor.");
        remember_fields();
        std::vector<ed::Change> changes=pending_items;
        for(auto i:hunter_indices) {
            ed::Change c; c.hunter=i;
            const auto parse=[](const std::wstring &text) {
                if(text.empty() || text.size()>7 || text.find_first_not_of(L"0123456789")!=std::wstring::npos)
                    throw std::runtime_error("Currency must be a whole number from 0 to 9,999,999.");
                return static_cast<std::uint32_t>(std::stoul(text));
            };
            c.zenny=parse(draft_zenny[i]); c.points=parse(draft_points[i]);
            changes.push_back(c);
        }
        if(!dirty()) { message(L"No changes to save."); return true; }
        last_backup=session->save(changes);
        pending_items.clear();
        for(auto i:hunter_indices) {
            draft_zenny[i]=std::to_wstring(session->hunters()[i].summary.zenny);
            draft_points[i]=std::to_wstring(session->hunters()[i].points);
        }
        update_hunter(); EnableWindow(backup_button,TRUE);
        message(L"Saved successfully to the file shown above. Backup created.\r\nCurrency and queued items were written and verified.");
        if(!diagnostic_mode) {
            std::wstring confirmation=L"Saved and verified:\r\n"+session->save_file().wstring()+L"\r\n\r\n";
            for(auto i:hunter_indices) {
                const auto &h=session->hunters()[i];
                confirmation+=wide(h.summary.name)+L": "+std::to_wstring(h.points)+L" Pokke Points, "+std::to_wstring(h.summary.zenny)+L" Zenny\r\n";
            }
            confirmation+=L"\r\nA backup was created. You can now reopen the game.";
            MessageBoxW(window,confirmation.c_str(),L"Save completed",MB_OK|MB_ICONINFORMATION);
        }
        return true;
    } catch(const std::exception &e) {
        error(e);
        if(!diagnostic_mode) MessageBoxW(window,wide(e.what()).c_str(),L"Save did not complete",MB_OK|MB_ICONERROR);
        return false;
    }
}
void capture_window(const fs::path &path) {
    RECT rect{}; GetWindowRect(window,&rect);
    const int width=rect.right-rect.left,height=rect.bottom-rect.top;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=24; info.bmiHeader.biCompression=BI_RGB;
    const auto stride=(width*3+3)&~3;
    void *pixels=nullptr;
    auto dc=CreateCompatibleDC(nullptr);
    auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    auto old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    const bool drawn=bitmap && pixels;
    BITMAPFILEHEADER header{}; header.bfType=0x4d42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+stride*height;
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(drawn) {
        out.write(reinterpret_cast<const char *>(&header),sizeof(header));
        out.write(reinterpret_cast<const char *>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
        out.write(reinterpret_cast<const char *>(pixels),stride*height);
    }
    out.flush();
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc);
    if(!drawn || !out) throw std::runtime_error("Could not capture the editor window.");
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE: {
        trace("Creating controls");
        window=hwnd;
        font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        label(L"Freedom Unbound Save Editor",22,16,680);
        label(L"Close the game before editing. Click Save to write changes and create a backup.",22,43,700);
        control(L"BUTTON",L"Open save...",BS_PUSHBUTTON|WS_TABSTOP,22,78,145,30,OpenSave);
        save_path=control(L"EDIT",L"",ES_READONLY|ES_AUTOHSCROLL,182,78,552,30,0,WS_EX_CLIENTEDGE);
        control(L"BUTTON",L"Game data...",BS_PUSHBUTTON|WS_TABSTOP,22,119,145,30,GameFile);
        game_path=control(L"EDIT",L"",ES_READONLY|ES_AUTOHSCROLL,182,119,552,30,0,WS_EX_CLIENTEDGE);
        label(L"Hunter",22,170);
        hunter=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,182,166,552,140,Hunter);
        label(L"Zenny (0 - 9,999,999)",22,212);
        zenny=control(L"EDIT",L"0",ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,238,208,190,30,Zenny,WS_EX_CLIENTEDGE);
        label(L"Pokke Points (0 - 9,999,999)",22,253,214);
        points=control(L"EDIT",L"0",ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,238,249,190,30,Points,WS_EX_CLIENTEDGE);
        target_label=control(L"STATIC",L"Save target: -",SS_LEFT,448,208,286,72);
        label(L"Search items",22,309);
        search=control(L"EDIT",L"",ES_AUTOHSCROLL|WS_TABSTOP,182,305,552,30,Search,WS_EX_CLIENTEDGE);
        label(L"Item",22,350);
        items=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,182,346,552,350,Items);
        SendMessageW(items,CB_SETMINVISIBLE,12,0);
        label(L"Quantity (1 - 99,000)",22,391);
        quantity=control(L"EDIT",L"1",ES_NUMBER|ES_AUTOHSCROLL|WS_TABSTOP,238,387,190,30,Quantity,WS_EX_CLIENTEDGE);
        add_button=control(L"BUTTON",L"Add to item box",BS_PUSHBUTTON|WS_TABSTOP,466,387,268,30,Add);
        free_slots=control(L"STATIC",L"Empty item-box slots: - / 1000",0,22,433,712,24);
        status=control(L"STATIC",L"Choose your save and game data to begin.",SS_LEFT,22,470,712,76);
        save_button=control(L"BUTTON",L"Save",BS_DEFPUSHBUTTON|WS_TABSTOP,466,556,268,30,Save);
        backup_button=control(L"BUTTON",L"Open last backup",BS_PUSHBUTTON|WS_TABSTOP,22,556,180,30,Backups);
        EnableWindow(backup_button,FALSE); set_ready(false);
        // Make room for an explicit profile choice without squeezing existing controls.
        for(HWND child=GetWindow(hwnd,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
            RECT r{}; GetWindowRect(child,&r); MapWindowPoints(nullptr,hwnd,reinterpret_cast<POINT *>(&r),2);
            if(r.top>=78) SetWindowPos(child,nullptr,r.left,r.top+40,0,0,SWP_NOSIZE|SWP_NOZORDER);
        }
        label(L"Save profile",22,82);
        profile_picker=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,182,78,552,140,Profile);
        wchar_t appdata[32768]{};
        if(GetEnvironmentVariableW(L"APPDATA",appdata,32768)) {
            for(const auto *name:{L"FreedomUnite",L"FreedomUnite-Test"}) {
                const auto data=fs::path(appdata)/L"FreedomUnite"/name;
                if(!fs::is_regular_file(data/L"ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN")) continue;
                const auto text=std::wstring(name)+(profiles.empty() && std::wstring(name)==L"FreedomUnite"?L" (current game)":L" (older test profile)");
                SendMessageW(profile_picker,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str())); profiles.push_back(data);
            }
        }
        trace("Controls ready"); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==Profile && HIWORD(w)==CBN_SELCHANGE) {
            const auto row=SendMessageW(profile_picker,CB_GETCURSEL,0,0);
            if(row<0 || static_cast<std::size_t>(row)>=profiles.size()) return 0;
            if(!allow_discard()) {
                for(std::size_t i=0;i<profiles.size();++i) if(selected_save==profiles[i]/L"ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN") SendMessageW(profile_picker,CB_SETCURSEL,i,0);
                return 0;
            }
            const auto data=profiles[row]; selected_save=data/L"ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN";
            if(fs::is_regular_file(data/L"EBOOT.ELF")) selected_elf=data/L"EBOOT.ELF";
            selected_iso.clear(); open_session();
        } else if(LOWORD(w)==OpenSave && HIWORD(w)==BN_CLICKED) {
            if(!allow_discard()) return 0;
            const auto path=pick(L"Open a Freedom Unite hunter save",L"Hunter save (MHP2NDG.BIN)\0MHP2NDG.BIN\0\0",selected_save);
            if(!path.empty()) { selected_save=path; if(fs::exists(path.parent_path().parent_path().parent_path().parent_path().parent_path()/L"EBOOT.ELF")) {
                selected_elf=path.parent_path().parent_path().parent_path().parent_path().parent_path()/L"EBOOT.ELF"; selected_iso.clear(); }
                open_session(); }
        } else if(LOWORD(w)==GameFile && HIWORD(w)==BN_CLICKED) {
            if(!allow_discard()) return 0;
            const auto path=pick(L"Choose the installed game's EBOOT.ELF",L"Installed game executable (EBOOT.ELF)\0EBOOT.ELF\0\0",selected_elf);
            if(!path.empty()) { selected_elf=path; selected_iso.clear(); open_session(); }
        } else if(LOWORD(w)==Hunter && HIWORD(w)==CBN_SELCHANGE && !initializing) { remember_fields(); update_hunter(); }
        else if(LOWORD(w)==Search && HIWORD(w)==EN_CHANGE) filter_items();
        else if(LOWORD(w)==Save && HIWORD(w)==BN_CLICKED) save_changes();
        else if(LOWORD(w)==Add && HIWORD(w)==BN_CLICKED) queue_item();
        else if(LOWORD(w)==Backups && HIWORD(w)==BN_CLICKED && !last_backup.empty())
            ShellExecuteW(hwnd,L"open",last_backup.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        return 0;
    case WM_CLOSE: if(allow_discard()) DestroyWindow(hwnd); return 0;
    case WM_DESTROY: if(font) DeleteObject(font); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    wchar_t trace_file[32768]{};
    if(GetEnvironmentVariableW(L"MHFU_SAVE_EDITOR_TRACE",trace_file,32768)) startup_trace.open(fs::path(trace_file));
    trace("Starting");
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\FreedomUnboundSaveEditor");
    if(!singleton || GetLastError()==ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,L"The save editor is already open, or its lock could not be created.",L"Freedom Unbound Save Editor",MB_OK|MB_ICONINFORMATION);
        if(singleton) CloseHandle(singleton); return 1;
    }
    // Fixed logical size, scaled by Windows on high-DPI displays.
    trace("Mutex ready");
    WNDCLASSW wc{}; wc.lpfnWndProc=proc; wc.hInstance=instance; wc.lpszClassName=L"FreedomUnboundSaveEditor";
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
    RegisterClassW(&wc);
    trace("Creating window");
    auto hwnd=CreateWindowW(wc.lpszClassName,L"Freedom Unbound Save Editor",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                           CW_USEDEFAULT,CW_USEDEFAULT,780,675,nullptr,nullptr,instance,nullptr);
    if(!hwnd) { CloseHandle(singleton); return 1; }
    trace("Window ready");
    int argc=0;
    auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    bool smoke=false;
    fs::path capture;
    for(int i=1;argv && i<argc;++i) {
        const std::wstring arg=argv[i];
        if(arg==L"-SmokeTest") smoke=true;
        else if(i+1<argc) {
            if(arg==L"-Save") selected_save=argv[++i];
            else if(arg==L"-Game") selected_elf=argv[++i];
            else if(arg==L"-ISO") selected_iso=argv[++i];
            else if(arg==L"-Capture") capture=argv[++i];
        }
    }
    if(argv) LocalFree(argv);
    diagnostic_mode=smoke || !capture.empty();
    trace("Arguments ready");
    // Resolve the normal native installation first; browsing remains available.
    wchar_t appdata[32768]{};
    if(smoke && (selected_save.empty() || selected_elf.empty() || selected_iso.empty())) { DestroyWindow(hwnd); CloseHandle(singleton); return 2; }
    if(!selected_save.empty() && !selected_elf.empty()) open_session();
    else if(GetEnvironmentVariableW(L"APPDATA",appdata,32768)) {
        auto data=fs::path(appdata)/L"FreedomUnite"/L"FreedomUnite";
        const auto testing=fs::path(appdata)/L"FreedomUnite"/L"FreedomUnite-Test";
        if(!fs::is_regular_file(data/L"ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN") && fs::is_regular_file(testing/L"ms0/PSP/SAVEDATA/ULUS10391/MHP2NDG.BIN") && fs::is_regular_file(testing/L"EBOOT.ELF")) data=testing;
        const auto save=data/L"ms0"/L"PSP"/L"SAVEDATA"/L"ULUS10391"/L"MHP2NDG.BIN";
        if(fs::exists(save) && fs::exists(data/L"EBOOT.ELF")) { selected_save=save; selected_elf=data/L"EBOOT.ELF"; open_session(); }
    }
    trace("Session ready");
    if(smoke || !capture.empty()) {
        int result=0;
        try {
            if(smoke) {
                if(!session || hunter_indices.empty()) throw std::runtime_error("Diagnostic save did not open.");
                if(hunter_indices.size()>1) { SendMessageW(hunter,CB_SETCURSEL,1,0); SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Hunter,CBN_SELCHANGE),0); }
                const auto edited_hunter=chosen_hunter();
                const auto original_hunters=session->hunters();
                const auto read_disk=[&]() { std::ifstream in(selected_save,std::ios::binary); return std::vector<char>(std::istreambuf_iterator<char>(in),{}); };
                const auto original_disk=read_disk();
                const auto before=session->hunters()[edited_hunter].free_slots;
                const auto old_zenny=session->hunters()[edited_hunter].summary.zenny;
                SetWindowTextW(zenny,L"100001"); SetWindowTextW(points,L"2222");
                SetWindowTextW(search,L"Mega Potion"); SetWindowTextW(quantity,L"17");
                SendMessageW(hwnd,WM_COMMAND,Add,0);
                if(session->hunters()[edited_hunter].summary.zenny!=old_zenny || session->hunters()[edited_hunter].free_slots!=before)
                    throw std::runtime_error("Changes were written before Save.");
                if(read_disk()!=original_disk) throw std::runtime_error("Save file changed before Save was pressed.");
                if(hunter_indices.size()>1) {
                    SendMessageW(hunter,CB_SETCURSEL,0,0); SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Hunter,CBN_SELCHANGE),0);
                    SendMessageW(hunter,CB_SETCURSEL,1,0); SendMessageW(hwnd,WM_COMMAND,MAKEWPARAM(Hunter,CBN_SELCHANGE),0);
                    if(value(zenny)!=L"100001" || value(points)!=L"2222") throw std::runtime_error("Switching hunters lost pending currency edits.");
                }
                SendMessageW(hwnd,WM_COMMAND,Save,0);
                const auto &h=session->hunters()[edited_hunter];
                if(h.summary.zenny!=100001 || h.points!=2222 || h.free_slots!=before-1 || last_backup.empty())
                    throw std::runtime_error("Diagnostic controls did not apply the requested edits.");
                ed::Session reopened(selected_save,session->game_data());
                if(reopened.hunters()[edited_hunter].summary.zenny!=100001 || reopened.hunters()[edited_hunter].points!=2222 || reopened.hunters()[edited_hunter].free_slots!=before-1)
                    throw std::runtime_error("Saved changes did not survive reopening the file.");
                for(auto i:hunter_indices) if(i!=edited_hunter) {
                    const auto &a=original_hunters[i], &b=reopened.hunters()[i];
                    if(a.summary.zenny!=b.summary.zenny || a.points!=b.points || a.free_slots!=b.free_slots)
                        throw std::runtime_error("Another hunter was changed.");
                }
            }
            ShowWindow(hwnd,SW_SHOWNOACTIVATE); UpdateWindow(hwnd);
            RedrawWindow(hwnd,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);
            MSG pending{};
            while(PeekMessageW(&pending,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&pending); DispatchMessageW(&pending); }
            if(!capture.empty()) capture_window(capture);
        } catch(const std::exception &e) { trace(e.what()); result=3; }
        DestroyWindow(hwnd); CloseHandle(singleton); return result;
    }
    ShowWindow(hwnd,show); UpdateWindow(hwnd);
    trace("Message loop");
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0) if(!IsDialogMessageW(hwnd,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CloseHandle(singleton); return static_cast<int>(msg.wParam);
}
