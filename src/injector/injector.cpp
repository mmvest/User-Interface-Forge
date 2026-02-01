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

#include <Windows.h>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>

#include "scl\SCL.hpp"
#include "plog\Log.h"
#include "plog\Initializers\RollingFileInitializer.h"
#include "plog\Appenders\ConsoleAppender.h"

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

unsigned long   target_process_pid      = 0;
wchar_t*        target_process_name     = NULL;

#include <windows.h>
#include <tlhelp32.h>

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
	BOOL return_code = EXIT_FAILURE;
	HANDLE target_process = NULL;
    const SIZE_T PAGE_SIZE_BYTES = 4096;
    const DWORD WAIT_TIME_MILLISECONDS = 5000;

    if(!(std::filesystem::exists(CONFIG_FILE) && std::filesystem::is_regular_file(CONFIG_FILE)))
    {
        std::wcout << L"[!] Config file does not exist. Be sure a config file is defined in your current directory. See the github repo for an example config file.\n";
        return return_code;
    }

	// Read in data from config file, specifically the name of the dll to inject
	// and the logging level.
	scl::config_file uif_config(CONFIG_FILE, scl::config_file::READ);
    
    int max_log_size = uif_config.get<int>("MAX_LOG_SIZE_BYTES");
    int max_log_files =  uif_config.get<int>("MAX_LOG_FILES");
    std::string log_file_name =  uif_config.get<std::string>("INJECTOR_LOG_FILE_NAME");
	std::wstring core_dll_path = std::filesystem::absolute(uif_config.get<std::string>("CORE_DLL")).wstring();
	plog::Severity logging_level = static_cast<plog::Severity>(uif_config.get<int>("LOGGING_LEVEL"));

	// Initialize Logging
    static plog::RollingFileAppender<plog::TxtFormatter> file_appender(log_file_name.c_str(), max_log_size, max_log_files);
	static plog::ConsoleAppender<plog::TxtFormatter> console_appender;
	plog::init(logging_level, &console_appender).addAppender(&file_appender);

    PLOG_DEBUG << L"LOGGING LEVEL: " << logging_level;
    PLOG_DEBUG << L"CORE DLL PATH: " << core_dll_path;
    PLOG_DEBUG << L"LOG FILE NAME: " << log_file_name;
 	PLOG_DEBUG << L"MAX LOG FILES: " << max_log_files;
    PLOG_DEBUG << L"MAX LOG SIZE: " << max_log_size;
 
 	PLOG_DEBUG << L"Parsing arguments...";
    if (core_dll_path.empty())
    {
        PLOG_ERROR << L"Core DLL path is empty. Aborting.";
        goto cleanup;
    }
    if (!parse_args(argc, argv))
    {
        print_usage(argv[0]);
        goto cleanup;
    }
    if (target_process_pid == 0 && (target_process_name == NULL || target_process_name[0] == L'\0'))
    {
        PLOG_ERROR << L"Must specify a target process via -p pid or -n process_name.";
        print_usage(argv[0]);
        goto cleanup;
    }
 	PLOG_DEBUG << L"Arguments parsed successfully.";

    if (is_using_name)
    {
		PLOG_DEBUG << L"Process name: " << target_process_name;
        if (!GetPIDByName(target_process_name, &target_process_pid))
        {
            goto cleanup;
        }
    }

	PLOG_DEBUG << L"Process PID: " << target_process_pid;

    PLOG_DEBUG << L"Checking if core dll is already loaded in target process...";
    if(IsDllLoadedInProcess(target_process_pid, core_dll_path.c_str()))
    {
        PLOG_ERROR << L"Core is already loaded or an error occurred! Aborting.";
        goto cleanup;
    }
    PLOG_DEBUG << L"Core DLL is not loaded already. Good to keep going!";

	PLOG_DEBUG << L"Opening process...";
	target_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_process_pid);
	if (!target_process)
	{
		PLOG_ERROR << L"Failed to access target process. Error: " << GetLastError();
		goto cleanup;
	}
	PLOG_DEBUG << L"Process successfully opened. HANDLE: 0x" << target_process;

	PLOG_INFO << L"Injecting UiForge...";
	LPVOID payload_address = InjectDLL(target_process, const_cast<wchar_t*>(core_dll_path.c_str()));
    if (!payload_address)
    {
        PLOG_ERROR << L"Failed to inject dll. Error: "
           << _wcserror(GetLastError())
           << L" (0x"
           << std::setw(8) << std::setfill(L'0') << std::hex
           << GetLastError()
           << L")";
        goto cleanup;
    }

	PLOG_DEBUG << L"Payload Address: 0x"
           << std::setw(16)
           << std::setfill(L'0')
           << std::hex
           << reinterpret_cast<std::uintptr_t>(payload_address);

    // Lets do some sanity checking to make sure we did, in fact, inject the correct string
    PLOG_DEBUG << L"Checking string that was written to target process...";
    BYTE payload_snapshot[PAGE_SIZE_BYTES] = { 0 };
    BOOL read_proc_mem_result = ReadProcessMemory(target_process, payload_address, payload_snapshot, PAGE_SIZE_BYTES, NULL);
    if (!read_proc_mem_result)
    {
        DWORD read_err = GetLastError();
        PLOG_ERROR << L"ReadProcessMemory failed. Error: "
            << _wcserror(read_err)
            << L" (0x"
            << std::setw(8)
            << std::setfill(L'0')
            << std::hex
            << read_err
            << L")";
        goto cleanup;
    }
    // Is there a cleaner way to do this? Need to set the last two bytes to null delimiter to avoid
    // potentially reading over buffer later.
    payload_snapshot[PAGE_SIZE_BYTES - 2] = 0;
    payload_snapshot[PAGE_SIZE_BYTES - 1] = 0;
    
    const wchar_t* payload_snapshot_wstr = reinterpret_cast<const wchar_t*>(payload_snapshot);
    PLOG_DEBUG << L"ReadProcessMemory String: " << payload_snapshot_wstr;
    if (core_dll_path != payload_snapshot_wstr)
    {
        PLOG_ERROR << L"Injected payload mismatch. Expected: "
            << core_dll_path
            << L" Actual: "
            << payload_snapshot_wstr;
        goto cleanup;
    }
    PLOG_DEBUG << L"Payload string was written correctly!";
    
	PLOG_DEBUG << L"Running DLL Payload...";
	HANDLE injected_thread = RunPayloadDLL(target_process, payload_address);
	if (!injected_thread)
	{
		DWORD err = GetLastError();

		PLOG_ERROR << L"Failed to run payload. Error: "
				<< _wcserror(err)
				<< L" (0x"
				<< std::setw(8) << std::setfill(L'0') << std::hex
				<< err
				<< L")";

		if (!VirtualFreeEx(target_process, payload_address, 0, MEM_RELEASE))
		{
			DWORD freeErr = GetLastError();

			PLOG_ERROR << L"Oh my... a double error! VirtualFreeEx was unable to free "
						L"the reserved memory in target process. Error: "
					<< _wcserror(freeErr)
					<< L" (0x"
					<< std::setw(8)
                    << std::setfill(L'0')
                    << std::hex
					<< freeErr
					<< L")";
		}

		goto cleanup;
	}
	PLOG_DEBUG << L"Injected Thread Handle: 0x" << injected_thread; 

    // Lets check that the thread exited with a correct status code
    PLOG_DEBUG << L"Waiting for injected thread to load UiForge in target process (Timeout: " << WAIT_TIME_MILLISECONDS << "ms)...";
    DWORD wait_result = WaitForSingleObject(injected_thread, WAIT_TIME_MILLISECONDS);
    PLOG_DEBUG << L"Result of waiting for injected thread: 0x"
            << std::setw(8)
            << std::setfill(L'0')
            << std::hex
            << wait_result;
    switch(wait_result)
    {
        case WAIT_OBJECT_0:
            PLOG_DEBUG << L"The injected thread has exited!";
            break;
        case WAIT_TIMEOUT:
            PLOG_ERROR << L"WaitForSingleObject timed out. "
                << L"Note this does not necessarily mean UiForge failed to start";
            break;
        case WAIT_FAILED:
            PLOG_ERROR << L"WaitForSingleObject failed with Error: "
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
        PLOG_ERROR << L"GetExitCodeThread failed with Error: "
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
        PLOG_DEBUG << L"Injected thread exit code: "
            << std::dec
            << thread_exit_code
            << L" (0x"
            << std::setw(8)
            << std::setfill(L'0')
            << std::hex
            << thread_exit_code
            << L")";
    }

    PLOG_INFO << L"Success!";

	return_code = EXIT_SUCCESS;

cleanup:
	if (return_code == EXIT_FAILURE)
	{
		std::wcout << L"[!] Be sure that the target application is running and that you have the user privileges to access the process at an administrator level\n";
		print_usage(argv[0]);
		std::wcout << L"Press ENTER to exit...";
		std::getwchar();
	}
    if (target_process){CloseHandle(target_process);}
    if (injected_thread){CloseHandle(injected_thread);}
	return return_code;
}
