#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

static std::wstring dirname(std::wstring path)
{
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, slash);
}

static std::wstring quoteArg(const std::wstring &arg)
{
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = false;
    for (const auto ch : arg) {
        if (ch == L' ' || ch == L'\t' || ch == L'"') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return arg;
    }

    std::wstring out = L"\"";
    unsigned backslashes = 0;
    for (const auto ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
        } else if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(ch);
            backslashes = 0;
        } else {
            out.append(backslashes, L'\\');
            backslashes = 0;
            out.push_back(ch);
        }
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

static std::wstring getEnv(const wchar_t *name)
{
    DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) {
        return {};
    }

    std::wstring value(size, L'\0');
    DWORD written = GetEnvironmentVariableW(name, value.data(), size);
    value.resize(written);
    return value;
}

static void prependEnv(const wchar_t *name, const std::wstring &prefix)
{
    const auto current = getEnv(name);
    if (current.empty()) {
        SetEnvironmentVariableW(name, prefix.c_str());
    } else {
        SetEnvironmentVariableW(name, (prefix + L";" + current).c_str());
    }
}

static int showError(const std::wstring &message)
{
    MessageBoxW(nullptr, message.c_str(), L"KDevelop Launcher", MB_ICONERROR | MB_OK);
    return 1;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t launcherPath[MAX_PATH];
    const DWORD pathLen = GetModuleFileNameW(nullptr, launcherPath, MAX_PATH);
    if (pathLen == 0 || pathLen == MAX_PATH) {
        return showError(L"Unable to locate launcher path.");
    }

    const std::wstring root = dirname(launcherPath);
    const std::wstring binDir = root + L"\\bin";
    const std::wstring kdevelopExe = binDir + L"\\kdevelop.exe";

    if (GetFileAttributesW(kdevelopExe.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return showError(L"Cannot find " + kdevelopExe);
    }

    prependEnv(L"PATH", binDir);
    SetEnvironmentVariableW(L"KDEDIRS", root.c_str());
    SetEnvironmentVariableW(L"XDG_DATA_DIRS", (binDir + L"\\data;" + root + L"\\share").c_str());
    SetEnvironmentVariableW(L"XDG_CONFIG_DIRS", (root + L"\\etc\\xdg").c_str());
    SetEnvironmentVariableW(L"QT_PLUGIN_PATH", (binDir + L";" + root + L"\\lib\\plugins").c_str());
    SetEnvironmentVariableW(L"QML2_IMPORT_PATH", (binDir + L"\\qml;" + root + L"\\lib\\qml").c_str());
    SetEnvironmentVariableW(L"QT_QUICK_CONTROLS_STYLE_PATH",
                            (binDir + L"\\qml\\QtQuick\\Controls.2;" + root + L"\\lib\\qml\\QtQuick\\Controls.2").c_str());

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return showError(L"Unable to parse command line.");
    }

    std::wstring commandLine = quoteArg(kdevelopExe);
    for (int i = 1; i < argc; ++i) {
        commandLine += L" ";
        commandLine += quoteArg(argv[i]);
    }
    LocalFree(argv);

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(kdevelopExe.c_str(),
                        mutableCommand.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        root.c_str(),
                        &startupInfo,
                        &processInfo)) {
        return showError(L"Unable to start " + kdevelopExe);
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return 0;
}
