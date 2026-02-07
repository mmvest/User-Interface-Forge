/**
 * @file injector.cpp
 * @version 0.2.2
 * @brief "Entry point" for UiForge. Injects the core dll into a specified process.
 * 
 * @example	UiForge.exe --help
 * 			UiForge.exe -n pcsx2-qt.exe
 * 			UiForge.exe -p 1234
 * 
 * @note
 *   - The injector must be run with sufficient privileges to open the target process.
 *   - The injector and target process must be 64-bit.
 * 
 * @warning You use this module at your own risk. You are responsible for how you use this code.
 *          I highly recommend keeping this module very far away from any anti-cheats.
 * 			AV software might flag this as a virus/malware. This is because the code uses a
 * 			very classic and simple process injection technique, and is not being very sneaky
 * 			about it either. Go ahead and look over the code though -- its not malicious.
 * 
 * @author mmvest (wereox)
 * @date 2024-09-25
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <iomanip>
#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <cwctype>

#include "scl\SCL.hpp"
#include "plog\Log.h"
#include "plog\Initializers\RollingFileInitializer.h"
#include "plog\Formatters\TxtFormatter.h"
#include "plog\Appenders\RollingFileAppender.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

// Need to define this prior to including inject_tools.h so I can override their INJECT_LOG macro.
// This macro is just the inject_tools way of logging fatal errors.
#define INJECT_LOG(...) InjectLogToPlog(__VA_ARGS__)

inline void InjectLogToPlog(const wchar_t* fmt, ...)
{
    wchar_t buf[1024];

    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);

    PLOG_ERROR << buf;
}
#include "..\..\libs\InjectTools\inject_tools.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MIN_NUM_ARGS 2
#define BASE_10 10
#define DEFAULT_STACK_SIZE 0
#define CONFIG_FILE "config"

// Global flags and parameters
BOOL            is_using_pid            = FALSE;
BOOL            is_using_name           = FALSE;
BOOL            did_show_help           = FALSE;

unsigned long   target_process_pid      = 0;
wchar_t*        target_process_name     = NULL;

#include <windows.h>
#include <tlhelp32.h>

BOOL IsDllLoadedInProcess(DWORD pid, const wchar_t* target_dll_path);

namespace
{
    class UiLogBuffer final
    {
    public:
        void SetNotify(std::function<void()> notify)
        {
            std::scoped_lock lock(m_mutex);
            m_notify = std::move(notify);
        }

        void AppendLine(std::string line)
        {
            std::function<void()> notify;
            {
                std::scoped_lock lock(m_mutex);

                while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                {
                    line.pop_back();
                }
                if (line.empty())
                {
                    return;
                }

                m_lines.push_back(std::move(line));
                while (m_lines.size() > m_max_lines)
                {
                    m_lines.pop_front();
                }

                notify = m_notify;
            }

            if (notify)
            {
                notify();
            }
        }

        std::vector<std::string> Snapshot() const
        {
            std::scoped_lock lock(m_mutex);
            return std::vector<std::string>(m_lines.begin(), m_lines.end());
        }

    private:
        mutable std::mutex m_mutex;
        std::deque<std::string> m_lines;
        size_t m_max_lines = 800;
        std::function<void()> m_notify;
    };

    static std::string WideToUtf8(const std::wstring& w)
    {
        if (w.empty())
        {
            return {};
        }

        int required = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
        {
            std::string out;
            out.reserve(w.size());
            for (wchar_t c : w)
            {
                out.push_back((c >= 0 && c <= 0x7F) ? static_cast<char>(c) : '?');
            }
            return out;
        }

        std::string out(required, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), required, nullptr, nullptr);
        return out;
    }

    static std::string PlogNStringToUtf8(const plog::util::nstring& s)
    {
        if constexpr (std::is_same_v<plog::util::nstring, std::string>)
        {
            return s;
        }
        else
        {
            return WideToUtf8(s);
        }
    }

    class UiLogAppender final : public plog::IAppender
    {
    public:
        explicit UiLogAppender(UiLogBuffer& buffer) : m_buffer(buffer) {}

        void write(const plog::Record& record) override
        {
            const plog::util::nstring formatted = plog::TxtFormatter::format(record);
            std::string utf8 = PlogNStringToUtf8(formatted);

            size_t start = 0;
            while (start < utf8.size())
            {
                size_t end = utf8.find('\n', start);
                if (end == std::string::npos)
                {
                    m_buffer.AppendLine(utf8.substr(start));
                    break;
                }

                std::string line = utf8.substr(start, end - start);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                m_buffer.AppendLine(std::move(line));
                start = end + 1;
            }
        }

    private:
        UiLogBuffer& m_buffer;
    };
}

namespace
{
    static std::wstring ToLower(std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return s;
    }

    static std::wstring NormalizeProcName(std::wstring s)
    {
        s = ToLower(std::move(s));
        if (s.size() >= 4 && s.substr(s.size() - 4) == L".exe")
        {
            s.resize(s.size() - 4);
        }
        return s;
    }

    static bool EditDistanceWithin(const std::wstring& a, const std::wstring& b, size_t max_distance)
    {
        const size_t n = a.size();
        const size_t m = b.size();
        if (n == 0) return m <= max_distance;
        if (m == 0) return n <= max_distance;
        if (n > 128 || m > 128) return false; // keep this lightweight
        if (n > m ? (n - m) > max_distance : (m - n) > max_distance) return false;

        std::vector<size_t> prev(m + 1);
        std::vector<size_t> cur(m + 1);
        for (size_t j = 0; j <= m; ++j) prev[j] = j;

        for (size_t i = 1; i <= n; ++i)
        {
            cur[0] = i;
            size_t row_min = cur[0];
            for (size_t j = 1; j <= m; ++j)
            {
                const size_t cost = (a[i - 1] == b[j - 1]) ? 0u : 1u;
                cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
                row_min = std::min(row_min, cur[j]);
            }

            if (row_min > max_distance) return false;
            std::swap(prev, cur);
        }

        return prev[m] <= max_distance;
    }

    struct ProcessInfo
    {
        DWORD pid = 0;
        std::wstring exe_name;
        std::wstring path;
        int match_rank = 0; // 0 exact, 1 substring, 2 small edit-distance
    };

    static std::wstring TryGetProcessPath(DWORD pid)
    {
        std::wstring result;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return result;

        wchar_t buf[32768];
        DWORD size = static_cast<DWORD>(_countof(buf));
        if (QueryFullProcessImageNameW(process, 0, buf, &size))
        {
            result.assign(buf, size);
        }
        CloseHandle(process);
        return result;
    }

    static int ComputeMatchRank(const std::wstring& target_norm, const std::wstring& proc_norm)
    {
        if (target_norm == proc_norm) return 0;

        // Conservative "close match": only allow substring for reasonably-sized targets (avoid matching "a").
        if (target_norm.size() >= 3)
        {
            if (proc_norm.find(target_norm) != std::wstring::npos || target_norm.find(proc_norm) != std::wstring::npos)
            {
                return 1;
            }
        }

        // Lightweight fuzzy: bounded edit distance with a small threshold to catch common typos (missing/extra char).
        if (target_norm.size() >= 4 && EditDistanceWithin(target_norm, proc_norm, 2))
        {
            return 2;
        }

        return -1;
    }

    static std::vector<ProcessInfo> FindMatchingProcessesByName(const std::wstring& target_name)
    {
        std::vector<ProcessInfo> matches;

        const std::wstring target_norm = NormalizeProcName(target_name);
        if (target_norm.empty())
        {
            return matches;
        }

        PROCESSENTRY32W current_process_entry = { 0 };
        current_process_entry.dwSize = sizeof(PROCESSENTRY32W);

        HANDLE process_list_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (process_list_snapshot == INVALID_HANDLE_VALUE)
        {
            PLOG_ERROR << L"CreateToolhelp32Snapshot failed. Error: " << _wcserror(GetLastError()) << L" (0x" << std::setw(8) << std::setfill(L'0') << std::hex << GetLastError() << L")";
            return matches;
        }

        if (!Process32FirstW(process_list_snapshot, &current_process_entry))
        {
            PLOG_ERROR << L"Process32FirstW failed. Error: " << _wcserror(GetLastError()) << L" (0x" << std::setw(8) << std::setfill(L'0') << std::hex << GetLastError() << L")";
            CloseHandle(process_list_snapshot);
            return matches;
        }

        do
        {
            const std::wstring exe_name = current_process_entry.szExeFile;
            const std::wstring proc_norm = NormalizeProcName(exe_name);
            const int rank = ComputeMatchRank(target_norm, proc_norm);
            if (rank < 0)
            {
                continue;
            }

            ProcessInfo info;
            info.pid = current_process_entry.th32ProcessID;
            info.exe_name = exe_name;
            info.path = TryGetProcessPath(info.pid);
            info.match_rank = rank;
            matches.push_back(std::move(info));

        } while (Process32NextW(process_list_snapshot, &current_process_entry));

        CloseHandle(process_list_snapshot);

        std::sort(matches.begin(), matches.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
            if (a.match_rank != b.match_rank) return a.match_rank < b.match_rank;
            if (a.exe_name != b.exe_name) return a.exe_name < b.exe_name;
            return a.pid < b.pid;
        });

        return matches;
    }

    static bool InjectIntoProcessPid(DWORD pid, const std::wstring& core_dll_path, DWORD wait_time_ms)
    {
        const SIZE_T PAGE_SIZE_BYTES = 4096;

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Checking if core dll is already loaded...";
        if (IsDllLoadedInProcess(pid, core_dll_path.c_str()))
        {
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] Core is already loaded or an error occurred!";
            return false;
        }

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Opening process...";
        HANDLE target_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!target_process)
        {
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] Failed to access target process. Error: " << GetLastError();
            return false;
        }

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Process successfully opened. HANDLE: 0x" << target_process;
        PLOG_INFO << L"[PID " << std::dec << pid << L"] Injecting UiForge...";

        LPVOID payload_address = InjectDLL(target_process, const_cast<wchar_t*>(core_dll_path.c_str()));
        if (!payload_address)
        {
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] Failed to inject dll. Error: "
                << _wcserror(GetLastError())
                << L" (0x"
                << std::setw(8) << std::setfill(L'0') << std::hex
                << GetLastError()
                << L")";
            CloseHandle(target_process);
            return false;
        }

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Payload Address: 0x"
            << std::setw(16)
            << std::setfill(L'0')
            << std::hex
            << reinterpret_cast<std::uintptr_t>(payload_address);

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Checking string that was written to target process...";
        BYTE payload_snapshot[PAGE_SIZE_BYTES] = { 0 };
        BOOL read_proc_mem_result = ReadProcessMemory(target_process, payload_address, payload_snapshot, PAGE_SIZE_BYTES, NULL);
        if (!read_proc_mem_result)
        {
            DWORD read_err = GetLastError();
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] ReadProcessMemory failed. Error: "
                << _wcserror(read_err)
                << L" (0x"
                << std::setw(8)
                << std::setfill(L'0')
                << std::hex
                << read_err
                << L")";
            CloseHandle(target_process);
            return false;
        }
        payload_snapshot[PAGE_SIZE_BYTES - 2] = 0;
        payload_snapshot[PAGE_SIZE_BYTES - 1] = 0;

        const wchar_t* payload_snapshot_wstr = reinterpret_cast<const wchar_t*>(payload_snapshot);
        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] ReadProcessMemory String: " << payload_snapshot_wstr;
        if (core_dll_path != payload_snapshot_wstr)
        {
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] Injected payload mismatch. Expected: "
                << core_dll_path
                << L" Actual: "
                << payload_snapshot_wstr;
            CloseHandle(target_process);
            return false;
        }
        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Payload string was written correctly!";

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Running DLL Payload...";
        HANDLE injected_thread = RunPayloadDLL(target_process, payload_address);
        if (!injected_thread)
        {
            DWORD err = GetLastError();
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] Failed to run payload. Error: "
                << _wcserror(err)
                << L" (0x"
                << std::setw(8) << std::setfill(L'0') << std::hex
                << err
                << L")";

            if (!VirtualFreeEx(target_process, payload_address, 0, MEM_RELEASE))
            {
                DWORD free_err = GetLastError();
                PLOG_ERROR << L"[PID " << std::dec << pid << L"] VirtualFreeEx unable to free reserved memory. Error: "
                    << _wcserror(free_err)
                    << L" (0x"
                    << std::setw(8)
                    << std::setfill(L'0')
                    << std::hex
                    << free_err
                    << L")";
            }

            CloseHandle(target_process);
            return false;
        }

        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Injected Thread Handle: 0x" << injected_thread;
        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Waiting for injected thread (Timeout: " << std::dec << wait_time_ms << L"ms)...";

        DWORD wait_result = WaitForSingleObject(injected_thread, wait_time_ms);
        PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Result of waiting for injected thread: 0x"
            << std::setw(8)
            << std::setfill(L'0')
            << std::hex
            << wait_result;
        switch (wait_result)
        {
            case WAIT_OBJECT_0:
                PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Injected thread has exited!";
                break;
            case WAIT_TIMEOUT:
                PLOG_ERROR << L"[PID " << std::dec << pid << L"] WaitForSingleObject timed out. Note this does not necessarily mean UiForge failed to start.";
                break;
            case WAIT_FAILED:
                PLOG_ERROR << L"[PID " << std::dec << pid << L"] WaitForSingleObject failed with Error: "
                    << _wcserror(GetLastError())
                    << L" (0x"
                    << std::setw(8)
                    << std::setfill(L'0')
                    << std::hex
                    << GetLastError()
                    << L"). Note this does not necessarily mean UiForge failed to start.";
                break;
        }

        DWORD thread_exit_code = 0;
        if (!GetExitCodeThread(injected_thread, &thread_exit_code))
        {
            const DWORD exit_err = GetLastError();
            PLOG_ERROR << L"[PID " << std::dec << pid << L"] GetExitCodeThread failed with Error: "
                << _wcserror(exit_err)
                << L" (0x"
                << std::setw(8)
                << std::setfill(L'0')
                << std::hex
                << exit_err
                << L")";
        }
        else
        {
            PLOG_DEBUG << L"[PID " << std::dec << pid << L"] Injected thread exit code: "
                << std::dec
                << thread_exit_code
                << L" (0x"
                << std::setw(8)
                << std::setfill(L'0')
                << std::hex
                << thread_exit_code
                << L")";
        }

        CloseHandle(injected_thread);
        CloseHandle(target_process);

        PLOG_INFO << L"[PID " << std::dec << pid << L"] Success!";
        return true;
    }

    static std::string ShortenPathUtf8(const std::wstring& path, size_t max_len)
    {
        std::string utf8 = WideToUtf8(path);
        if (utf8.size() <= max_len) return utf8;
        if (max_len < 8) return utf8.substr(0, max_len);
        return "..." + utf8.substr(utf8.size() - (max_len - 3));
    }
}

/**
 * @brief Checks whether a specific DLL is loaded in a target process.
 *
 * Enumerates all modules loaded by the target process and performs a
 * case-sensitive comparison against the provided DLL path. Returns TRUE
 * if a matching module is found; otherwise, returns FALSE.
 *
 * @param pid The process ID of the target process to inspect.
 * @param target_dll_path Full path of the DLL to search for (e.g., "C:\my_dir\example.dll").
 *
 * @return TRUE if the specified DLL is loaded in the target process; FALSE otherwise.
 */


BOOL IsDllLoadedInProcess(DWORD pid, const wchar_t* target_dll_path)
{
    HANDLE dll_list_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32W current_dll = { 0 };
    current_dll.dwSize = sizeof(MODULEENTRY32W);

    BOOL found = FALSE;

    if (dll_list_snapshot == INVALID_HANDLE_VALUE)
    {
        PLOG_ERROR << L"CreateToolhelp32Snapshot failed to retrieve dll list snapshot. Error: "
           << _wcserror(GetLastError())
           << L" (0x"
           << std::setw(8) << std::setfill(L'0') << std::hex
           << GetLastError()
           << L")";
        found = TRUE;
        goto dll_check_cleanup;
    }

    if (!Module32FirstW(dll_list_snapshot, &current_dll))
    {
        PLOG_ERROR << L"ModuleEntry32W failed to retrieve first module in dll list. Error: "
           << _wcserror(GetLastError())
           << L" (0x"
           << std::setw(8) << std::setfill(L'0') << std::hex
           << GetLastError()
           << L")";
        found = TRUE;
        goto dll_check_cleanup;
    }

    do
    {
        if (wcscmp(current_dll.szExePath, target_dll_path) == 0)
        {
            found = TRUE;
            break;
        }
    }
    while (Module32NextW(dll_list_snapshot, &current_dll));

dll_check_cleanup:
    if (dll_list_snapshot) { CloseHandle(dll_list_snapshot); }
    return found;
}


/**
 * @brief Prints usage instructions to the console.
 *
 * Displays the program name, accepted command-line options, and their descriptions.
 *
 * @param prog_name The name or path of the executable (typically argv[0]).
 */
void print_usage(const wchar_t* prog_name)
{
    wprintf(L"[+] Usage: %s [-p pid | -n process_name] [-h | -help | --help]\n", prog_name);
    wprintf(L"[+] Options:\n");
    wprintf(L"[+]   -p pid             Target process by PID\n");
    wprintf(L"[+]   -n name            Target process by executable name\n");
    wprintf(L"[+]   -h, -help, --help  Show this usage information\n");
}

/**
 * @brief Parses and validates command-line arguments.
 *
 * Sets global flags indicating how to identify the target process and
 * which payload to inject. Validates that exactly one target selector
 * (PID or name) and one payload selector (DLL or shellcode) have been provided.
 * Prints errors or usage information if validation fails.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of wide-character argument strings.
 * @return TRUE if arguments are valid and parsing succeeded; FALSE otherwise.
 */

BOOL parse_args(int argc, wchar_t* argv[])
{
    for (int idx = 1; idx < argc; ++idx)
    {
        if (wcscmp(argv[idx], L"-p") == 0)
        {
            is_using_pid = TRUE;
            is_using_name = FALSE;
            ++idx < argc ? (target_process_pid = wcstoul(argv[idx], NULL, 10)) : (void)0;
        }
        else if (wcscmp(argv[idx], L"-n") == 0)
        {
            is_using_name = TRUE;
            is_using_pid = FALSE;
            ++idx < argc ? (target_process_name = argv[idx]) : (void)0;
        }

        else if (wcscmp(argv[idx], L"-h") == 0 || wcscmp(argv[idx], L"-help") == 0 || wcscmp(argv[idx], L"--help") == 0)
        {
            print_usage(argv[0]);
            did_show_help = TRUE;
            return FALSE;
        }
    }
	PLOG_DEBUG << L"Arguments:";
	PLOG_DEBUG << L"is_using_pid: " << is_using_pid;
	if(is_using_pid) PLOG_DEBUG << L"PID: " << target_process_pid;
	PLOG_DEBUG << L"is_using_name: " << is_using_name;
	if(is_using_name) PLOG_DEBUG << L"Process Name: " << target_process_name;

    // Validate parsed arguments
    if (is_using_pid && target_process_pid == 0)
    {
        PLOG_ERROR << L"PID must be non-zero.";
        return FALSE;
    }

    if (is_using_name && target_process_name == NULL)
    {
        PLOG_ERROR << L"Process name must be specified.";
        return FALSE;
    }

    return TRUE;
}

int wmain(int argc, wchar_t** argv)
{
    int return_code = EXIT_FAILURE;
    const DWORD WAIT_TIME_MILLISECONDS = 5000;

    if (!(std::filesystem::exists(CONFIG_FILE) && std::filesystem::is_regular_file(CONFIG_FILE)))
    {
        std::wcout << L"[!] Config file does not exist. Be sure a config file is defined in your current directory. See the github repo for an example config file.\n";
        return EXIT_FAILURE;
    }

    scl::config_file uif_config(CONFIG_FILE, scl::config_file::READ);

    int max_log_size = uif_config.get<int>("MAX_LOG_SIZE_BYTES");
    int max_log_files = uif_config.get<int>("MAX_LOG_FILES");
    std::string log_file_name = uif_config.get<std::string>("INJECTOR_LOG_FILE_NAME");
    std::wstring core_dll_path = std::filesystem::absolute(uif_config.get<std::string>("CORE_DLL")).wstring();
    plog::Severity logging_level = static_cast<plog::Severity>(uif_config.get<int>("LOGGING_LEVEL"));

    // Initialize logging:
    // - Rolling file via PLOG
    // - Mirrored UI log buffer via an additional appender (stdout/stderr writes while FTXUI is running will cause big problems)
    static plog::RollingFileAppender<plog::TxtFormatter> file_appender(log_file_name.c_str(), max_log_size, max_log_files);
    static UiLogBuffer ui_log_buffer;
    static UiLogAppender ui_appender(ui_log_buffer);
    plog::init(logging_level, &ui_appender).addAppender(&file_appender);

    PLOG_DEBUG << L"LOGGING LEVEL: " << logging_level;
    PLOG_DEBUG << L"CORE DLL PATH: " << core_dll_path;
    PLOG_DEBUG << L"LOG FILE NAME: " << log_file_name;
    PLOG_DEBUG << L"MAX LOG FILES: " << max_log_files;
    PLOG_DEBUG << L"MAX LOG SIZE: " << max_log_size;

    PLOG_DEBUG << L"Parsing arguments...";
    if (!parse_args(argc, argv))
    {
        if (did_show_help) return EXIT_SUCCESS;
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (target_process_pid == 0 && (target_process_name == NULL || target_process_name[0] == L'\0'))
    {
        PLOG_ERROR << L"Must specify a target process via -p pid or -n process_name.";
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (core_dll_path.empty())
    {
        PLOG_ERROR << L"Core DLL path is empty. Aborting.";
        return EXIT_FAILURE;
    }
    PLOG_DEBUG << L"Arguments parsed successfully.";

    std::vector<ProcessInfo> candidates;
    if (is_using_pid)
    {
        ProcessInfo info;
        info.pid = static_cast<DWORD>(target_process_pid);
        info.exe_name = L"(pid specified)";
        info.path = TryGetProcessPath(info.pid);
        candidates.push_back(std::move(info));
    }
    else
    {
        PLOG_INFO << L"Scanning for processes matching: " << (target_process_name ? target_process_name : L"(null)");
        candidates = FindMatchingProcessesByName(target_process_name ? target_process_name : L"");
    }

    enum class UiMode
    {
        Select,
        Injecting,
        Done,
    };

    std::atomic<UiMode> ui_mode = UiMode::Select;
    std::atomic<bool> injection_all_ok = false;
    std::string done_message;
    std::mutex done_mutex;

    std::vector<bool> selected(candidates.size(), false);
    int menu_index = 0;
    std::vector<std::string> menu_entries;
    menu_entries.reserve(candidates.size());

    auto rebuild_menu_entries = [&]()
    {
        menu_entries.clear();
        menu_entries.reserve(candidates.size());
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            const auto& p = candidates[i];
            std::string row = std::to_string(p.pid) + "  " + WideToUtf8(p.exe_name);
            if (!p.path.empty())
            {
                row += "  " + ShortenPathUtf8(p.path, 60);
            }
            row += selected[i] ? "   [x]" : "   [ ]";
            menu_entries.push_back(std::move(row));
        }
    };
    rebuild_menu_entries();

    ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
    ui_log_buffer.SetNotify([&]() { screen.PostEvent(ftxui::Event::Custom); });

    int log_scroll_lines = 0;
    std::thread worker;

    auto start_injection = [&]()
    {
        if (worker.joinable())
        {
            return;
        }

        std::vector<DWORD> pids;
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            if (selected[i]) pids.push_back(candidates[i].pid);
        }

        if (pids.empty())
        {
            {
                std::scoped_lock lock(done_mutex);
                done_message = "No processes selected. Exiting.";
            }
            ui_mode.store(UiMode::Done);
            screen.PostEvent(ftxui::Event::Custom);
            return;
        }

        ui_mode.store(UiMode::Injecting);
        screen.PostEvent(ftxui::Event::Custom);

        worker = std::thread([&, pids = std::move(pids)]() mutable {
            int ok = 0;
            int fail = 0;
            for (DWORD pid : pids)
            {
                PLOG_INFO << L"--- Injecting into PID " << std::dec << pid << L" ---";
                if (InjectIntoProcessPid(pid, core_dll_path, WAIT_TIME_MILLISECONDS))
                {
                    ++ok;
                }
                else
                {
                    ++fail;
                }
            }

            injection_all_ok.store(fail == 0);
            {
                std::scoped_lock lock(done_mutex);
                done_message = "Done. Success: " + std::to_string(ok) + "  Failed: " + std::to_string(fail);
            }
            ui_mode.store(UiMode::Done);
            screen.PostEvent(ftxui::Event::Custom);
        });
    };

    ftxui::MenuOption menu_opt = ftxui::MenuOption::Vertical();
    auto menu = ftxui::Menu(&menu_entries, &menu_index, menu_opt);
    menu = ftxui::CatchEvent(menu, [&](ftxui::Event event) {
        if (event == ftxui::Event::Character(' '))
        {
            if (!selected.empty() && menu_index >= 0 && static_cast<size_t>(menu_index) < selected.size())
            {
                selected[static_cast<size_t>(menu_index)] = !selected[static_cast<size_t>(menu_index)];
                rebuild_menu_entries();
            }
            return true;
        }
        return false;
    });

    auto inject_button = ftxui::Button("[ Inject ]", start_injection, ftxui::ButtonOption::Animated());
    auto exit_button = ftxui::Button("[ Exit ]", [&]() { screen.Exit(); }, ftxui::ButtonOption::Animated());

    auto root = ftxui::Container::Vertical({
        ftxui::Maybe(menu, [&]() { return ui_mode.load() == UiMode::Select && candidates.size() >= 2; }),
        ftxui::Maybe(inject_button, [&]() { return ui_mode.load() == UiMode::Select && candidates.size() >= 2; }),
        ftxui::Maybe(exit_button, [&]() { return ui_mode.load() == UiMode::Done; }),
    });

    root = ftxui::CatchEvent(root, [&](ftxui::Event event) {
        if (event == ftxui::Event::Escape)
        {
            if (ui_mode.load() == UiMode::Select)
            {
                {
                    std::scoped_lock lock(done_mutex);
                    done_message = "Canceled by user. Exiting.";
                }
                ui_mode.store(UiMode::Done);
                screen.PostEvent(ftxui::Event::Custom);
                return true;
            }
            if (ui_mode.load() == UiMode::Done)
            {
                screen.Exit();
                return true;
            }
        }

        if (event == ftxui::Event::PageUp)
        {
            log_scroll_lines = std::min(log_scroll_lines + 10, 500);
            return true;
        }
        if (event == ftxui::Event::PageDown)
        {
            log_scroll_lines = std::max(log_scroll_lines - 10, 0);
            return true;
        }

        return false;
    });

    auto ui = ftxui::Renderer(root, [&] {
        using namespace ftxui;

        const UiMode mode = ui_mode.load();
        const bool show_select = mode == UiMode::Select;
        const bool show_injecting = mode == UiMode::Injecting;
        const bool show_done = mode == UiMode::Done;

        std::string step_text = "Scan → Select → Inject";
        if (show_injecting) step_text = "Scan → Select → Injecting";
        if (show_done) step_text = "Scan → Select → Done";

        auto header = vbox({
            text("UiForge Injector") | bold | center,
            text(step_text) | dim | center,
        });

        Element main_panel;
        if (candidates.empty())
        {
            std::string target = target_process_name ? WideToUtf8(target_process_name) : "(pid)";
            main_panel = vbox({
                text("Target: " + target) | center,
                separator(),
                text("No matching processes found.") | bold | center,
                text("Press Esc to exit.") | dim | center,
            });
        }
        else if (candidates.size() == 1 && show_injecting)
        {
            main_panel = vbox({
                text("Auto-injecting into PID " + std::to_string(candidates[0].pid) + "...") | bold | center,
                text("Logs are updating below.") | dim | center,
            });
        }
        else if (candidates.size() == 1 && show_done)
        {
            std::string msg;
            {
                std::scoped_lock lock(done_mutex);
                msg = done_message.empty() ? "Done." : done_message;
            }
            main_panel = vbox({
                text(msg) | bold | center,
                separator(),
                exit_button->Render() | center,
            });
        }
        else if (show_select && candidates.size() >= 2)
        {
            std::string target = target_process_name ? WideToUtf8(target_process_name) : "(pid)";
            auto instructions = paragraph("↑/↓: move   Space: toggle   Tab: switch focus   Enter: activate   Esc: cancel") | dim | center;

            main_panel = vbox({
                text("Target: " + target) | center,
                separator(),
                window(text("Matches (PID first)"),
                       menu->Render() | frame | vscroll_indicator | size(HEIGHT, LESS_THAN, 12)),
                separator(),
                inject_button->Render() | center,
                instructions,
            });
        }
        else if (show_injecting)
        {
            main_panel = vbox({
                text("Injecting...") | bold | center,
                text("Please wait. Logs are updating below.") | dim | center,
            });
        }
        else
        {
            std::string msg;
            {
                std::scoped_lock lock(done_mutex);
                msg = done_message.empty() ? "Done." : done_message;
            }
            main_panel = vbox({
                text(msg) | bold | center,
                separator(),
                exit_button->Render() | center,
            });
        }

        auto lines = ui_log_buffer.Snapshot();
        Elements log_elems;
        log_elems.reserve(lines.size());
        for (const auto& line : lines)
        {
            log_elems.push_back(text(line));
        }

        const int total_lines = static_cast<int>(log_elems.size());
        int focus_line = 0;
        if (total_lines > 0)
        {
            focus_line = std::max(0, (total_lines - 1) - log_scroll_lines);
        }
        const float rel = (total_lines <= 1) ? 1.0f : (static_cast<float>(focus_line) / static_cast<float>(total_lines - 1));

        auto logs_panel = window(
            text("Logs (PageUp/PageDown scroll)"),
            vbox(std::move(log_elems)) | focusPositionRelative(0.f, rel) | frame | vscroll_indicator | size(HEIGHT, LESS_THAN, 14));

        return vbox({
                   header,
                   separator(),
                   main_panel | flex,
                   separator(),
                   logs_panel,
               }) |
               border;
    });

    // Auto behavior:
    // - 0 matches: message + exit
    // - 1 match: auto-inject (no selector UI)
    // - 2+ matches: selector UI
    if (candidates.empty())
    {
        {
            std::scoped_lock lock(done_mutex);
            done_message = "No matching processes found.";
        }
        ui_mode.store(UiMode::Done);
    }
    else if (candidates.size() == 1)
    {
        selected[0] = true;
        rebuild_menu_entries();
        ui_mode.store(UiMode::Injecting);
        screen.PostEvent(ftxui::Event::Custom);
        start_injection();
    }
    else if (is_using_pid)
    {
        selected[0] = true;
        rebuild_menu_entries();
        ui_mode.store(UiMode::Injecting);
        screen.PostEvent(ftxui::Event::Custom);
        start_injection();
    }
    else
    {
        ui_mode.store(UiMode::Select);
    }

    screen.Loop(ui);

    if (worker.joinable())
    {
        worker.join();
    }

    if (candidates.empty())
    {
        return EXIT_FAILURE;
    }

    return_code = injection_all_ok.load() ? EXIT_SUCCESS : EXIT_FAILURE;
    return return_code;
}
