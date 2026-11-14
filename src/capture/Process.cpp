#include "Process.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include <SDL3/SDL.h>

#ifdef AE_OS_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>
#else
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace Atlas::Capture {

    uint32_t CurrentProcessId() {

#ifdef AE_OS_WINDOWS
        return uint32_t(::GetCurrentProcessId());
#else
        return uint32_t(getpid());
#endif

    }

    bool IsProcessRunning(uint32_t processId) {

#ifdef AE_OS_WINDOWS
        auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (!process)
            return false;
        DWORD code = 0;
        bool running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
        CloseHandle(process);
        return running;
#else
        return kill(pid_t(processId), 0) == 0;
#endif

    }

    fs::path GetExecutablePath() {

#ifdef AE_OS_WINDOWS
        std::wstring path(MAX_PATH, L'\0');
        auto length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
        path.resize(length);
        return fs::path(path);
#else
        std::error_code error;
        return fs::read_symlink("/proc/self/exe", error);
#endif

    }

#ifdef AE_OS_WINDOWS
    // Quotes an argument like the C runtime parses it
    static std::wstring QuoteArgument(const std::string& arg) {

        auto wide = fs::path(arg).wstring();
        if (!wide.empty() && wide.find_first_of(L" \t\"") == std::wstring::npos)
            return wide;

        std::wstring quoted = L"\"";
        size_t backslashes = 0;
        for (auto c : wide) {
            if (c == L'\\') {
                backslashes++;
                continue;
            }
            quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
            backslashes = 0;
            quoted.push_back(c);
        }
        quoted.append(backslashes * 2, L'\\');
        quoted.push_back(L'"');
        return quoted;

    }

    static HANDLE OpenLog(const fs::path& log) {

        SECURITY_ATTRIBUTES attributes = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        return CreateFileW(log.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);

    }

    static HANDLE Start(const std::vector<std::string>& command, const fs::path& workingDirectory, const fs::path& log, DWORD flags,
        uint32_t& processId) {

        std::wstring commandLine;
        for (const auto& arg : command)
            commandLine += (commandLine.empty() ? L"" : L" ") + QuoteArgument(arg);

        // Only the log is inherited: a child holding the output pipe of the caller would keep e.g. "| tail" waiting
        auto output = OpenLog(log);
        SECURITY_ATTRIBUTES attributes = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        auto input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes, OPEN_EXISTING, 0, nullptr);
        HANDLE inherited[] = { output, input };

        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        std::vector<uint8_t> attributeList(size);
        auto list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeList.data());
        InitializeProcThreadAttributeList(list, 1, 0, &size);
        UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr);

        STARTUPINFOEXW startup = {};
        startup.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = input;
        startup.StartupInfo.hStdOutput = output;
        startup.StartupInfo.hStdError = output;
        startup.lpAttributeList = list;

        PROCESS_INFORMATION process = {};
        auto directory = workingDirectory.wstring();
        auto create = [&](DWORD creationFlags) {

            return CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, creationFlags | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &process);

        };
        auto created = create(flags);
        // A job object of the calling shell may not allow processes to leave it
        if (!created && (flags & CREATE_BREAKAWAY_FROM_JOB))
            created = create(flags & ~CREATE_BREAKAWAY_FROM_JOB);
        DeleteProcThreadAttributeList(list);
        CloseHandle(output);
        CloseHandle(input);
        if (!created)
            return nullptr;

        CloseHandle(process.hThread);
        processId = uint32_t(process.dwProcessId);
        return process.hProcess;

    }
#endif

    uint32_t StartDetachedProcess(const fs::path& executable, const std::vector<std::string>& args, const fs::path& workingDirectory,
        const fs::path& log) {

        std::vector<std::string> command = { executable.string() };
        command.insert(command.end(), args.begin(), args.end());

#ifdef AE_OS_WINDOWS
        uint32_t processId = 0;
        auto process = Start(command, workingDirectory, log, DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB,
            processId);
        if (!process)
            return 0;
        CloseHandle(process);
        return processId;
#else
        auto child = fork();
        if (child != 0)
            return child > 0 ? uint32_t(child) : 0;

        setsid();
        auto output = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(output, 1);
        dup2(output, 2);
        if (!workingDirectory.empty() && chdir(workingDirectory.c_str()) != 0)
            _exit(127);
        std::vector<char*> argv;
        for (auto& arg : command)
            argv.push_back(arg.data());
        argv.push_back(nullptr);
        execv(argv[0], argv.data());
        _exit(127);
#endif

    }

    int32_t RunProcess(const std::vector<std::string>& command, const fs::path& workingDirectory, const fs::path& log,
        float timeoutSeconds) {

#ifdef AE_OS_WINDOWS
        uint32_t processId = 0;
        auto process = Start(command, workingDirectory, log, CREATE_NO_WINDOW, processId);
        if (!process)
            return -1;

        auto wait = WaitForSingleObject(process, timeoutSeconds > 0.0f ? DWORD(timeoutSeconds * 1000.0f) : INFINITE);
        if (wait == WAIT_TIMEOUT) {
            CloseHandle(process);
            KillProcessTree(processId);
            return -2;
        }

        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        CloseHandle(process);
        return int32_t(code);
#else
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, 1, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        posix_spawn_file_actions_adddup2(&actions, 1, 2);

        std::vector<std::string> arguments = command;
        std::vector<char*> argv;
        for (auto& arg : arguments)
            argv.push_back(arg.data());
        argv.push_back(nullptr);

        auto previous = fs::current_path();
        if (!workingDirectory.empty())
            fs::current_path(workingDirectory);
        pid_t child = 0;
        auto spawned = posix_spawnp(&child, argv[0], &actions, nullptr, argv.data(), environ);
        fs::current_path(previous);
        posix_spawn_file_actions_destroy(&actions);
        if (spawned != 0)
            return -1;

        auto start = std::chrono::steady_clock::now();
        int status = 0;
        while (waitpid(child, &status, WNOHANG) == 0) {
            if (timeoutSeconds > 0.0f && SecondsSince(start) > timeoutSeconds) {
                KillProcessTree(uint32_t(child));
                waitpid(child, &status, 0);
                return -2;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    }

    std::vector<uint32_t> KillProcessTree(uint32_t processId) {

        std::vector<uint32_t> killed;

#ifdef AE_OS_WINDOWS
        // Children first, a parent like a script would otherwise start new ones
        std::vector<std::pair<uint32_t, uint32_t>> processes;
        auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };
            for (auto valid = Process32FirstW(snapshot, &entry); valid; valid = Process32NextW(snapshot, &entry))
                processes.emplace_back(uint32_t(entry.th32ProcessID), uint32_t(entry.th32ParentProcessID));
            CloseHandle(snapshot);
        }

        std::vector<uint32_t> tree = { processId };
        for (size_t i = 0; i < tree.size(); i++)
            for (const auto& [child, parent] : processes)
                if (parent == tree[i] && child != processId && std::find(tree.begin(), tree.end(), child) == tree.end())
                    tree.push_back(child);

        for (auto it = tree.rbegin(); it != tree.rend(); ++it) {
            auto process = OpenProcess(PROCESS_TERMINATE, FALSE, *it);
            if (!process)
                continue;
            if (TerminateProcess(process, 1))
                killed.push_back(*it);
            CloseHandle(process);
        }
#else
        if (kill(pid_t(processId), SIGKILL) == 0)
            killed.push_back(processId);
#endif

        return killed;

    }

    void MoveWindowToBack(SDL_Window* window) {

#ifdef AE_OS_WINDOWS
        auto handle = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        // A window without one below it is at the bottom already
        if (handle && GetWindow(handle, GW_HWNDNEXT))
            SetWindowPos(handle, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif

    }

}
