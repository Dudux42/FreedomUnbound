// GUI entry point for the packaged launcher. Gameplay remains in the native runtime.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include <vector>

namespace {
std::wstring quote(const std::wstring& value) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result += c;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    bool smoke = false;
    std::filesystem::path logRoot;
    auto fail = [&](const std::wstring& message) {
        if (!smoke) MessageBoxW(nullptr, message.c_str(), L"Freedom Unbound", MB_OK | MB_ICONERROR);
        return 1;
    };
    try {
        int count = 0;
        LPWSTR* raw = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!raw) return fail(L"Could not read launcher arguments.");
        std::vector<std::wstring> args(raw + 1, raw + count);
        LocalFree(raw);
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == L"-SmokeTest") smoke = true;
            else if ((args[i] == L"-DataDirectory" || args[i] == L"-LogDirectory") && i + 1 < args.size()) {
                if (args[i] == L"-LogDirectory") logRoot = args[i + 1];
                ++i;
            } else return fail(L"Unknown launcher argument.");
        }
        std::vector<wchar_t> buffer(32768);
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!length || length >= buffer.size()) return fail(L"Could not locate the launcher folder.");
        auto folder = std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
        auto script = folder / L"Start-Freedom-Unbound.ps1";
        if (!std::filesystem::is_regular_file(script) ||
            !std::filesystem::is_regular_file(folder / L"FreedomUniteNative.exe"))
            return fail(L"Extract the complete Freedom Unbound package before starting it. Keep the executable, scripts and DLLs together.");
        if (logRoot.empty()) {
            wchar_t local[MAX_PATH];
            if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local)))
                return fail(L"Could not locate the session log folder.");
            logRoot = std::filesystem::path(local) / L"FreedomUnbound" / L"logs";
        }
        logRoot = std::filesystem::absolute(logRoot);
        std::filesystem::create_directories(logRoot);
        auto startupLog = logRoot / L"launcher-startup.log";
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE output = CreateFileW(startupLog.c_str(), FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output == INVALID_HANDLE_VALUE) return fail(L"Could not create the launcher startup log.");
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (input == INVALID_HANDLE_VALUE) { CloseHandle(output); return fail(L"Could not initialize the launcher."); }
        wchar_t system[MAX_PATH];
        UINT systemLength = GetSystemDirectoryW(system, MAX_PATH);
        if (!systemLength || systemLength >= MAX_PATH) {
            CloseHandle(input); CloseHandle(output); return fail(L"Could not locate Windows PowerShell.");
        }
        auto powershell = std::filesystem::path(system) / L"WindowsPowerShell" / L"v1.0" / L"powershell.exe";
        std::wstring command = quote(powershell.wstring()) + L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " + quote(script.wstring());
        for (const auto& arg : args) command += L" " + quote(arg);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = output;
        startup.hStdError = output;
        PROCESS_INFORMATION process{};
        BOOL started = CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, folder.c_str(), &startup, &process);
        DWORD error = GetLastError();
        CloseHandle(input); CloseHandle(output);
        if (!started) return fail(L"Could not start Windows PowerShell (error " + std::to_wstring(error) + L").");
        AllowSetForegroundWindow(process.dwProcessId);
        CloseHandle(process.hThread);
        DWORD waited = WaitForSingleObject(process.hProcess, INFINITE);
        DWORD code = 1;
        BOOL gotCode = GetExitCodeProcess(process.hProcess, &code);
        CloseHandle(process.hProcess);
        if (waited != WAIT_OBJECT_0 || !gotCode || code != 0)
            return fail(L"Freedom Unbound could not complete this session. Logs are available in:\n" + logRoot.wstring());
        return 0;
    } catch (...) {
        return fail(L"Could not prepare the launcher. Check that the package is complete and its log folder is writable.");
    }
}
