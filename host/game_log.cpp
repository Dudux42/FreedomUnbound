#include "game_log.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <system_error>

#if defined(_WIN32) && !defined(MHFU_GAME_LOG_PURE)
#include "install/user_data.hpp"

#include <atomic>
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <mutex>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace portablekit::fu::game_log {

bool keep_line(std::string_view line) {
    std::size_t hex_run = 0;
    static constexpr std::string_view kWord = "savedata";
    for (std::size_t i = 0; i < line.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        hex_run = std::isxdigit(c) ? hex_run + 1u : 0u;
        if (hex_run >= 32u) return false;
        if (i + kWord.size() <= line.size()) {
            bool match = true;
            for (std::size_t k = 0; k < kWord.size() && match; ++k)
                match = std::tolower(static_cast<unsigned char>(line[i + k])) == kWord[k];
            if (match) return false;
        }
    }
    return true;
}

std::string file_name(const std::tm &t) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "FreedomUnbound-%04d%02d%02d-%02d%02d%02d.log", t.tm_year + 1900,
                  t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return buffer;
}

std::vector<std::string> log_files(const std::vector<std::string> &names) {
    std::vector<std::string> ours;
    for (const std::string &name : names) {
        if (name.size() > kPrefix.size() + kSuffix.size() && name.compare(0, kPrefix.size(), kPrefix) == 0 &&
            name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0)
            ours.push_back(name);
    }
    std::sort(ours.begin(), ours.end());
    return ours;
}

std::vector<std::string> files_to_delete(const std::vector<std::string> &names, std::size_t keep) {
    std::vector<std::string> ours = log_files(names);
    if (ours.size() <= keep) return {};
    ours.resize(ours.size() - keep);
    return ours;
}

std::size_t rotate(const std::filesystem::path &dir, std::size_t keep) {
    std::error_code ec;
    std::vector<std::string> names;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec)) names.push_back(it->path().filename().string());
    }
    std::size_t removed = 0;
    for (const std::string &name : files_to_delete(names, keep)) {
        if (std::filesystem::remove(dir / name, ec)) ++removed;
    }
    return removed;
}

#if defined(_WIN32) && !defined(MHFU_GAME_LOG_PURE)
namespace {

// Each of stdout/stderr is pointed at a pipe. A reader thread copies every byte
// unchanged to the original handle (so the console is as before) and splits the
// stream into lines for the file. The file opens on the first output after the
// data folder can be resolved; earlier lines wait in memory.
struct Tee {
    std::mutex mu;
    std::ofstream file;
    bool file_tried = false;
    std::string early;  // filtered lines before the file opened
};

Tee &tee() {
    static Tee *value = new Tee;  // never destroyed: threads run until exit
    return *value;
}

void write_file_line(Tee &t, const std::string &line) {
    if (!keep_line(line)) return;
    if (!t.file.is_open() && !t.file_tried) {
        try {
            const std::filesystem::path dir = portablekit::install::user_data_directory() / "logs";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            localtime_s(&local, &now);
            std::filesystem::path path = dir / file_name(local);
            for (int n = 2; std::filesystem::exists(path, ec) && n < 100; ++n) {
                std::string name = file_name(local);
                path = dir / (name.substr(0, name.size() - kSuffix.size()) + "-" + std::to_string(n) + std::string(kSuffix));
            }
            t.file.open(path, std::ios::binary | std::ios::app);
            t.file_tried = true;
            if (t.file.is_open()) {
                rotate(dir);  // the new file counts among the 10
                t.file << t.early;
                t.early.clear();
                t.early.shrink_to_fit();
            }
        } catch (...) {
            // The data folder is not known yet (too early); keep the line for later.
        }
    }
    if (t.file.is_open()) {
        t.file << line << '\n';
        t.file.flush();
    } else if (!t.file_tried && t.early.size() < (1u << 20)) {
        t.early += line;
        t.early += '\n';
    }
}

struct Stream {
    int fd;
    DWORD std_handle;
    FILE *stream;
    int original = -1;  // -1: there was no console or file behind it
    HANDLE read = nullptr;
    HANDLE done = nullptr;  // set when the reader saw the end of the pipe
};

void pump(Stream s) {
    Tee &t = tee();
    std::string partial;
    char buffer[4096];
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(s.read, buffer, sizeof(buffer), &got, nullptr) || got == 0) break;
        if (s.original >= 0) _write(s.original, buffer, got);  // unchanged bytes for the console
        std::lock_guard lock(t.mu);
        partial.append(buffer, got);
        std::size_t start = 0;
        for (std::size_t nl; (nl = partial.find('\n', start)) != std::string::npos; start = nl + 1u) {
            std::string line = partial.substr(start, nl - start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            write_file_line(t, line);
        }
        partial.erase(0, start);
    }
    {
        std::lock_guard lock(t.mu);
        if (!partial.empty()) write_file_line(t, partial);
    }
    SetEvent(s.done);
}

Stream g_streams[2];

bool redirect(Stream &s) {
    if (_fileno(s.stream) < 0) {
        // No console (started without one): give the stream a descriptor first.
        FILE *ignored = nullptr;
        if (freopen_s(&ignored, "NUL", "w", s.stream) != 0) return false;
    } else {
        s.original = _dup(s.fd);
    }
    int pipe_fds[2];
    if (_pipe(pipe_fds, 1u << 16, _O_BINARY | _O_NOINHERIT) != 0) return false;
    std::fflush(s.stream);
    if (_dup2(pipe_fds[1], _fileno(s.stream)) != 0) {
        _close(pipe_fds[0]);
        _close(pipe_fds[1]);
        return false;
    }
    _close(pipe_fds[1]);
    SetStdHandle(s.std_handle, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(s.stream))));
    std::setvbuf(s.stream, nullptr, _IONBF, 0);  // every write reaches the console at once, as before
    s.read = reinterpret_cast<HANDLE>(_get_osfhandle(pipe_fds[0]));
    s.done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread(pump, s).detach();
    return true;
}

// At exit: point stdout/stderr back where they went before. That closes the
// pipes' write ends, so each reader drains what is left, sees the end of the
// pipe and stops. Nothing touches the read ends here: a pipe handle is
// synchronous, so even PeekNamedPipe would wait behind the reader's ReadFile
// forever (that deadlocked the exit in the first build).
void drain() {
    std::fflush(stdout);
    std::fflush(stderr);
    HANDLE waits[2];
    DWORD count = 0;
    for (Stream &s : g_streams) {
        if (s.done == nullptr) continue;
        int target = s.original;
        if (target < 0) target = _open("NUL", _O_WRONLY);
        if (target >= 0 && _dup2(target, _fileno(s.stream)) == 0)
            SetStdHandle(s.std_handle, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(s.stream))));
        if (s.original < 0 && target >= 0) _close(target);
        waits[count++] = s.done;
    }
    // A child process that inherited a write end would keep a pipe open; do
    // not wait for it longer than a second.
    if (count != 0) WaitForMultipleObjects(count, waits, TRUE, 1000);
    std::lock_guard lock(tee().mu);
    if (tee().file.is_open()) tee().file.flush();
}

} // namespace

void start() {
    static const bool started = [] {
        if (const char *off = std::getenv("MHFU_GAME_LOG"); off != nullptr && off[0] == '0') return false;
        g_streams[0] = {1, STD_OUTPUT_HANDLE, stdout};
        g_streams[1] = {2, STD_ERROR_HANDLE, stderr};
        bool any = false;
        for (Stream &s : g_streams) any = redirect(s) || any;
        if (any) std::atexit(drain);
        return any;
    }();
    (void)started;
}

// Before main(), so the first lines are caught too.
namespace {
const bool kStarted = (start(), true);
}
#else
void start() {}
#endif

} // namespace portablekit::fu::game_log
