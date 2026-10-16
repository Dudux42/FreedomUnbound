#include "updater.hpp"
#include "cheats.hpp"
#include "launcher_ini.hpp"
#include "release_notes.hpp"

#include "ui/layer.hpp"
#include "ui/widgets.hpp"
#include "ui/host_text.hpp"
#include "imgui.h"

#include <miniz.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef MHFU_VERSION
#define MHFU_VERSION "0.0.0-dev"
#endif

namespace portablekit::fu {
namespace {
namespace fs = std::filesystem;

constexpr const wchar_t *kHost = L"api.github.com";
constexpr const wchar_t *kReleases = L"/repos/Dudux42/FreedomUnbound/releases?per_page=10";

enum class State { Idle, Checking, UpToDate, Available, Offline, Working, Ready, Failed };

struct Shared {
    std::mutex lock;
    std::atomic<State> state{State::Idle};
    std::string latest, zip_url, sha_url, message;
    // A newer tag than this build, even when it has no Windows ZIP yet: the
    // banner points at its page (FU Controls).
    std::string newer_tag;
    // This build's own release notes, when GitHub has a release for it.
    std::string current_tag, current_notes;
} g;

constexpr const char *kReleasePage = "https://github.com/Dudux42/FreedomUnbound/releases/tag/";
constexpr const char *kDismissKey = "update_dismissed";
std::string g_dismissed_session;  // dismissed this run, before the file is read again

struct Version { int part[3]{}; int pre_rank = 2; int pre_num = 0; };

// "v0.1.1-alpha.2" -> {0,1,1, alpha=0, 2}. No prerelease ranks above any.
Version parse_version(std::string text) {
    Version v;
    if (!text.empty() && (text[0] == 'v' || text[0] == 'V')) text.erase(0, 1);
    if (!text.empty() && text[0] == '.') text.erase(0, 1);
    size_t i = 0;
    for (int k = 0; k < 3 && i < text.size(); ++k) {
        int n = 0;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) n = n * 10 + (text[i++] - '0');
        v.part[k] = n;
        if (i < text.size() && text[i] == '.') ++i; else break;
    }
    if (i < text.size() && text[i] == '-') {
        std::string pre = text.substr(i + 1);
        for (auto &c : pre) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        v.pre_rank = pre.rfind("alpha", 0) == 0 ? 0 : pre.rfind("beta", 0) == 0 ? 1 : 0;
        const size_t dot = pre.find('.');
        if (dot != std::string::npos) v.pre_num = std::atoi(pre.c_str() + dot + 1);
    }
    return v;
}

bool newer(const Version &a, const Version &b) {
    for (int k = 0; k < 3; ++k) if (a.part[k] != b.part[k]) return a.part[k] > b.part[k];
    if (a.pre_rank != b.pre_rank) return a.pre_rank > b.pre_rank;
    return a.pre_num > b.pre_num;
}

#ifdef _WIN32
std::wstring widen(const std::string &s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// HTTPS GET following redirects (release assets redirect to another host).
bool http_get(std::wstring host, std::wstring path, std::string &body, int depth = 0) {
    if (depth > 5) return false;
    bool ok = false;
    HINTERNET session = WinHttpOpen(L"FreedomUnbound-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return false;
    WinHttpSetTimeouts(session, 10000, 10000, 15000, 60000);
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (request) {
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
        const wchar_t *headers = L"Accept: application/vnd.github+json, application/octet-stream\r\n";
        if (WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr)) {
            DWORD status = 0, size = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
            if (status >= 300 && status < 400) {
                wchar_t location[4096]; DWORD len = sizeof(location);
                if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                        location, &len, WINHTTP_NO_HEADER_INDEX)) {
                    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
                    wchar_t h[512], p[3584];
                    parts.lpszHostName = h; parts.dwHostNameLength = 512;
                    parts.lpszUrlPath = p; parts.dwUrlPathLength = 3584;
                    wchar_t extra[2048]; parts.lpszExtraInfo = extra; parts.dwExtraInfoLength = 2048;
                    if (WinHttpCrackUrl(location, 0, 0, &parts)) {
                        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
                        return http_get(std::wstring(h, parts.dwHostNameLength),
                            std::wstring(p, parts.dwUrlPathLength) + std::wstring(extra, parts.dwExtraInfoLength),
                            body, depth + 1);
                    }
                }
            } else if (status == 200) {
                ok = true;
                for (;;) {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(request, &avail)) { ok = false; break; }
                    if (avail == 0) break;
                    const size_t at = body.size();
                    body.resize(at + avail);
                    DWORD read = 0;
                    if (!WinHttpReadData(request, body.data() + at, avail, &read)) { ok = false; break; }
                    body.resize(at + read);
                }
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

bool http_get_url(const std::string &url, std::string &body) {
    std::wstring w = widen(url);
    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
    wchar_t h[512], p[3584];
    parts.lpszHostName = h; parts.dwHostNameLength = 512;
    parts.lpszUrlPath = p; parts.dwUrlPathLength = 3584;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &parts)) return false;
    return http_get(std::wstring(h, parts.dwHostNameLength), std::wstring(p, parts.dwUrlPathLength), body);
}

std::string sha256_hex(const std::string &data) {
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32]{};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
    BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    BCryptHashData(hash, (PUCHAR)data.data(), (ULONG)data.size(), 0);
    BCryptFinishHash(hash, digest, 32, 0);
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
    static const char *hex = "0123456789abcdef";
    std::string out;
    for (unsigned char b : digest) { out += hex[b >> 4]; out += hex[b & 15]; }
    return out;
}

fs::path exe_dir() {
    wchar_t buf[MAX_PATH * 4]; DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 4);
    return fs::path(std::wstring(buf, n)).parent_path();
}

fs::path work_dir() {
    wchar_t *local = nullptr;
    fs::path base = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local) == S_OK ? fs::path(local) : fs::temp_directory_path();
    if (local) CoTaskMemFree(local);
    return base / L"FreedomUnbound" / L"update";
}
#endif

// Small helpers for GitHub's JSON: the value of "key": "..." after `from`.
std::string json_string(const std::string &json, const std::string &key, size_t from, size_t *end = nullptr) {
    const std::string needle = "\"" + key + "\"";
    size_t k = json.find(needle, from);
    if (k == std::string::npos) return {};
    size_t q = json.find('"', json.find(':', k + needle.size()) + 1);
    if (q == std::string::npos) return {};
    std::string out;
    size_t i = q + 1;
    for (; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) ++i;
        out += json[i];
    }
    if (end) *end = i;
    return out;
}

void check() {
#ifdef _WIN32
    std::string body;
    if (!http_get(kHost, kReleases, body) || body.empty() || body[0] != '[') { g.state = State::Offline; return; }
    // FU Controls: the notes of the release matching this build, for "What's
    // new". Kept in memory and cached in <data>\whatsnew-cache.txt for offline
    // starts.
    {
        size_t p = 0;
        for (;;) {
            size_t e = 0;
            const std::string tag = json_string(body, "tag_name", p, &e);
            if (tag.empty()) break;
            const size_t next = body.find("\"tag_name\"", e);
            const std::string block = body.substr(p, (next == std::string::npos ? body.size() : next) - p);
            const Version a = parse_version(tag), b = parse_version(MHFU_VERSION);
            if (!newer(a, b) && !newer(b, a)) {
                const std::string notes = release_notes::json_text(block, "body");
                {
                    std::lock_guard l(g.lock);
                    g.current_tag = tag;
                    g.current_notes = notes;
                }
                if (!notes.empty()) release_notes::write_cache(tag, notes);
                break;
            }
            if (next == std::string::npos) break;
            p = next;
        }
    }
    // Releases come newest first; take the first that is not a draft.
    size_t at = 0;
    for (;;) {
        size_t tag_end = 0;
        std::string tag = json_string(body, "tag_name", at, &tag_end);
        if (tag.empty()) { g.state = State::UpToDate; return; }
        size_t next = body.find("\"tag_name\"", tag_end);
        std::string block = body.substr(at, (next == std::string::npos ? body.size() : next) - at);
        // Skip drafts.
        if (block.find("\"draft\":true") != std::string::npos || block.find("\"draft\": true") != std::string::npos) {
            if (next == std::string::npos) { g.state = State::UpToDate; return; }
            at = next; continue;
        }
        std::string zip, sha;
        size_t pos = 0;
        for (;;) {
            size_t e = 0;
            std::string url = json_string(block, "browser_download_url", pos, &e);
            if (url.empty()) break;
            if (url.size() > 4 && url.compare(url.size() - 4, 4, ".zip") == 0 && url.find("windows-x64") != std::string::npos) zip = url;
            if (url.size() > 11 && url.compare(url.size() - 11, 11, ".zip.sha256") == 0) sha = url;
            pos = e;
        }
        std::lock_guard l(g.lock);
        g.latest = tag;
        g.newer_tag = newer(parse_version(tag), parse_version(MHFU_VERSION)) ? tag : std::string();
        if (newer(parse_version(tag), parse_version(MHFU_VERSION)) && !zip.empty() && !sha.empty()) {
            g.zip_url = zip; g.sha_url = sha; g.state = State::Available;
        } else {
            g.state = State::UpToDate;
        }
        return;
    }
#else
    g.state = State::Offline;
#endif
}

void fail(const char *why) {
    std::lock_guard l(g.lock);
    g.message = why;
    g.state = State::Failed;
}

void download_and_stage() {
#ifdef _WIN32
    std::string zip_url, sha_url;
    { std::lock_guard l(g.lock); zip_url = g.zip_url; sha_url = g.sha_url; }
    std::string zip, sha;
    if (!http_get_url(zip_url, zip) || zip.empty()) return fail(ui::host_text::cstr("launcher.update.download_failed"));
    if (!http_get_url(sha_url, sha) || sha.size() < 64) return fail(ui::host_text::cstr("launcher.update.checksum_download_failed"));
    std::string expected;
    for (char c : sha.substr(0, 64)) expected += (char)std::tolower((unsigned char)c);
    if (sha256_hex(zip) != expected) return fail(ui::host_text::cstr("launcher.update.update_failed_its_checksum"));

    std::error_code ec;
    const fs::path work = work_dir();
    const fs::path staging = work / L"staging";
    fs::remove_all(staging, ec);
    fs::create_directories(staging, ec);

    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, zip.data(), zip.size(), 0)) return fail(ui::host_text::cstr("launcher.update.update_zip_is_damaged"));
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    // Strip a single top-level folder if every entry shares it.
    std::string prefix;
    for (mz_uint i = 0; i < count; ++i) {
        char name[1024]; mz_zip_reader_get_filename(&archive, i, name, sizeof(name));
        std::string n(name);
        const size_t slash = n.find('/');
        std::string top = slash == std::string::npos ? std::string() : n.substr(0, slash + 1);
        if (i == 0) prefix = top; else if (top != prefix) { prefix.clear(); break; }
    }
    bool ok = true;
    for (mz_uint i = 0; i < count && ok; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&archive, i, &st)) { ok = false; break; }
        std::string n = st.m_filename;
        if (!prefix.empty()) n = n.substr(prefix.size());
        if (n.empty() || n.find("..") != std::string::npos || n[0] == '/' || n.find(':') != std::string::npos) continue;
        const fs::path out = staging / fs::path(widen(n));
        if (mz_zip_reader_is_file_a_directory(&archive, i)) { fs::create_directories(out, ec); continue; }
        fs::create_directories(out.parent_path(), ec);
        ok = mz_zip_reader_extract_to_file(&archive, i, out.string().c_str(), 0);
    }
    mz_zip_reader_end(&archive);
    if (!ok || !fs::exists(staging / L"FreedomUnbound.exe")) return fail(ui::host_text::cstr("launcher.update.update_zip_has_no_launcher"));

    // Helper: wait for the game and launcher to close, copy program files
    // over the install folder (never %APPDATA%), then start the new launcher.
    const fs::path install = exe_dir();
    const fs::path script = work / L"apply-update.cmd";
    std::ofstream cmd(script, std::ios::binary | std::ios::trunc);
    cmd << "@echo off\r\n"
           "set n=0\r\n"
           ":wait\r\n"
           "tasklist /FI \"IMAGENAME eq FreedomUniteNative.exe\" | find /I \"FreedomUniteNative.exe\" >nul && goto again\r\n"
           "tasklist /FI \"IMAGENAME eq FreedomUnbound.exe\" | find /I \"FreedomUnbound.exe\" >nul && goto again\r\n"
           "goto copy\r\n"
           ":again\r\n"
           "set /a n+=1\r\n"
           "if %n% GEQ 60 goto copy\r\n"
           "timeout /t 1 /nobreak >nul\r\n"
           "goto wait\r\n"
           ":copy\r\n"
           "robocopy \"" << staging.string() << "\" \"" << install.string() << "\" /E /R:5 /W:1 /NFL /NDL /NJH /NJS >\"" << (work / L"apply-update.log").string() << "\"\r\n"
           "start \"\" \"" << (install / L"FreedomUnbound.exe").string() << "\"\r\n";
    cmd.close();
    if (!cmd) return fail(ui::host_text::cstr("launcher.update.could_not_write_the_update"));

    std::wstring line = L"cmd.exe /c \"\"" + script.wstring() + L"\"\"";
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW | DETACHED_PROCESS | CREATE_BREAKAWAY_FROM_JOB, nullptr, work.c_str(), &si, &pi)) {
        if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, work.c_str(), &si, &pi))
            return fail(ui::host_text::cstr("launcher.update.could_not_start_the_update"));
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    g.state = State::Ready;
#else
    fail(ui::host_text::cstr("launcher.update.updates_need_windows"));
#endif
}

} // namespace

bool draw_update_button(float width) {
    using namespace ui;
    State s = g.state.load();
    if (s == State::Idle) {
        g.state = State::Checking;
        std::thread(check).detach();
        s = State::Checking;
    }
    std::string label, latest, message;
    { std::lock_guard l(g.lock); latest = g.latest; message = g.message; }
    bool active = false;
    switch (s) {
    case State::Idle:
    case State::Checking: label = ui::host_text::cstr("launcher.update.checking_for_updates"); break;
    case State::UpToDate: label = ui::host_text::format("launcher.update.up_to_date", {MHFU_VERSION}); break;
    case State::Offline: label = ui::host_text::format("launcher.update.offline", {MHFU_VERSION}); break;
    case State::Available: label = ui::host_text::format("launcher.update.update_to", {latest}); active = true; break;
    case State::Working: label = ui::host_text::format("launcher.update.downloading", {latest}); break;
    case State::Ready: label = ui::host_text::cstr("launcher.update.restarting"); break;
    case State::Failed: label = ui::host_text::format("launcher.update.failed_retry", {message}); active = true; break;
    }
    // Keep the label inside the button: fall back to the bare version.
    const float room = width - ImGui::GetFontSize() * 2.0f;
    if (ImGui::CalcTextSize(label.c_str()).x > room && !active && s != State::Working)
        label = std::string("v") + MHFU_VERSION;
    if (ImGui::CalcTextSize(label.c_str()).x > room && s == State::Failed)
        label = ui::host_text::cstr("launcher.update.update_failed_retry");
    // Cheat choices persist in cheats.ini; say so on the launcher home screen.
    if (cheats() != 0u) paragraph(ui::host_text::cstr("launcher.update.cheats_are_on_see_cheats"), IM_COL32(240, 190, 80, 255));
    ImGui::PushID("##update");
    // Up to date / offline: the same button checks again (FU Controls).
    const bool recheck = s == State::UpToDate || s == State::Offline;
    // Only an actionable button (Update, retry) is reachable with the pad or
    // the arrow keys, so Down past Quit doesn't land on a status line. The
    // mouse can still click Up to date / Offline to check again.
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, !active);
    const bool pressed = big_button(label.c_str(), width, active, !active && !recheck);
    ImGui::PopItemFlag();
    if (recheck && (ImGui::IsItemHovered() || ImGui::IsItemFocused()))
        Layer::get().set_description(ui::host_text::cstr("launcher.update.check_for_updates_again"));
    ImGui::PopID();
    if (pressed && recheck) {
        g.state = State::Checking;
        std::thread(check).detach();
    } else if (pressed && s == State::Failed && g.zip_url.empty()) {
        g.state = State::Checking;  // a failed check, not a failed download
        std::thread(check).detach();
    } else if (pressed && active) {
        g.state = State::Working;
        std::thread(download_and_stage).detach();
    }
    return g.state.load() == State::Ready;
}

void ensure_update_check() {
    State idle = State::Idle;
    if (g.state.compare_exchange_strong(idle, State::Checking)) std::thread(check).detach();
}

bool current_release_notes(std::string &tag, std::string &notes) {
    std::lock_guard l(g.lock);
    if (g.current_notes.empty()) return false;
    tag = g.current_tag;
    notes = g.current_notes;
    return true;
}

static void open_release_page(const std::string &tag) {
    const std::string url = std::string(kReleasePage) + tag;
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)url;
#endif
}

void draw_update_banner(float x, float y, float width) {
    using namespace ui;
    std::string tag;
    { std::lock_guard l(g.lock); tag = g.newer_tag; }
    if (tag.empty() || tag == g_dismissed_session) return;
    if (const auto dismissed = launcher_ini::get(kDismissKey); dismissed && *dismissed == tag) return;
    const float font = ImGui::GetFontSize();
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({width, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, colors::kPanel);
    ImGui::PushStyleColor(ImGuiCol_Border, colors::kAccent);
    ImGui::Begin("##update_banner", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing);
    paragraph(ui::host_text::format("launcher.update.banner", {tag, MHFU_VERSION}), colors::kAccentBright);
    ImGui::PushFont(nullptr, font * 0.8f);
    paragraph(ui::host_text::cstr("launcher.update.nothing_is_downloaded_until_you"), colors::kTextDim);
    ImGui::PopFont();
    const float half = (ImGui::GetContentRegionAvail().x - font * 0.5f) * 0.5f;
    if (big_button(ui::host_text::cstr("launcher.update.open_release_page"), half, true)) open_release_page(tag);
    ImGui::SameLine();
    if (big_button(ui::host_text::cstr("launcher.update.dismiss_for_this_version"), half)) {
        g_dismissed_session = tag;
        (void)launcher_ini::set(kDismissKey, tag);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

} // namespace portablekit::fu