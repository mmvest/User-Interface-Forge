/**
 * @file uif_injector.cpp
 * @version 0.2.1
 * @brief Entry point for UiForge. Injects the core dll into a specified process with the desired configuration.
 * 
 * This application allows the user to inject the core dll into a target process. Additionally, the user can specify
 * optional DLL files to inject.
 * 
 * @example	UiForge.exe --help
 * 			UiForge.exe pcsx2-qt.exe
 * 			UiForge.exe pcsx2-qt.exe my_module_01.dll my_module_02.dll
 * 
 * @note Ensure that the specified DLLs are compatible with the chosen bitness and graphics API.
 *       Currently only supports 64-bit d3d11 applications.
 *       If you are providing additional modules, be sure they are either in your path or
 *       somewhere the target application can find.
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
#include "..\..\include\injector_util.h"
#include "..\..\include\SimpleConfigLibrary\SCL.hpp"
#include "..\..\include\plog\Log.h"
#include "..\..\include\plog\Initializers\RollingFileInitializer.h"
#include "..\..\include\plog\Appenders\ConsoleAppender.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define MIN_NUM_ARGS		2
#define BASE_10				10
#define DEFAULT_STACK_SIZE	0
#define CONFIG_FILE "config"
#define USAGE \
L"Usage: %s <process_name> [ <dll_file1> <dll_file2> ... <dll_fileN> ]\n" \
L"\n" \
L"Parameters:\n" \
L"  <process_name>    Specifies the name of the target process to start UiForge in.\n" \
L"\n" \
L"  <dll_file1>, <dll_file2>, ..., <dll_fileN>\n" \
L"                    Optional: List of additional DLL files to load.\n" \
L"                    You can specify one or more module names that represent the names of the DLLs.\n" \
L"\n" \
L"Options:\n" \
L"  -h, -help, --help, /? \n" \
L"                    Displays this usage statement and exits.\n" \
L"\n" \
L"Example:\n" \
L"  %s --help\n" \
L"  %s 64 d3d11 my_module_01.dll my_module_02.dll\n" \
L"\n" \
L"Note: If you are providing additional modules, be sure they are either in your path or\n" \
L"      somewhere the target application can find.\n" 

UserOptions user_options;

bool ParseArgs(int argc, wchar_t** argv)
{
	if(argc < MIN_NUM_ARGS) return false;

	// Iterate over the arguments
	user_options.target_process_name = argv[1];

	for(unsigned arg_idx = 2; arg_idx < argc; arg_idx++)
	{
		if(!std::filesystem::exists(argv[arg_idx]))
		{
			PLOG_FATAL << L"[!] The module " << argv[arg_idx] << L" could not be found.";
			return false;
		}
		
		user_options.modules.emplace_back(argv[arg_idx]);
	}

	return true;
}

int wmain(int argc, wchar_t** argv)
{
	int return_code					= EXIT_FAILURE;
	HANDLE target_process			= NULL;
	void* inject_address			= NULL;
	size_t num_bytes_to_write		= 0;
	size_t num_bytes_written		= 0;
	const wchar_t* list_of_modules 	= NULL;
	bool did_write_memory			= FALSE;
	HMODULE k32dll					= NULL;
	FARPROC load_lib_addr			= NULL;
	HANDLE injected_thread			= NULL;

	// Read in data from config file
	scl::config_file uif_config(CONFIG_FILE, scl::config_file::READ);
	std::string core_dll = uif_config.get<std::string>("CORE_DLL");
	user_options.modules.emplace_back(std::filesystem::absolute(core_dll).wstring()); // Make sure the core_dll is the first module in the vector

	// Initialize Logging
	plog::Severity logging_level	= static_cast<plog::Severity>(uif_config.get<int>("LOGGING_LEVEL"));
	static plog::ConsoleAppender<plog::TxtFormatter> console_appender;
	plog::init(logging_level, &console_appender);

	if(!ParseArgs(argc, argv))
	{
		goto cleanup;
	}

	unsigned long target_pid = GetProcessIdByName(user_options.target_process_name.c_str());
	if (target_pid == 0)
	{
		goto cleanup;
	}

	target_process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, target_pid);
	if (!target_process)
	{
		PLOG_FATAL << L"[!] Failed to access target process. Error: " << GetLastError();
		goto cleanup;
	}

	PLOG_INFO << L"[+] Injecting modules... ";
	for(std::wstring module_name : user_options.modules) PLOG_INFO << L"---[+] " << module_name;

	num_bytes_to_write = GetTotalSizeBytesOfWStrings(user_options.modules);
	PLOG_INFO << L"[+] Total size of modules: " << num_bytes_to_write << L" bytes";
	inject_address = VirtualAllocEx(target_process, NULL, num_bytes_to_write, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!inject_address)
	{
		PLOG_FATAL << L"[!] Failed to allocate memory in target process. Error: " << GetLastError();
		goto cleanup;
	}

	// write dll name
	list_of_modules = ConcatenateWStrings(user_options.modules).c_str();
	PLOG_INFO << L"[+] Writing all DLLs to target process...";
	did_write_memory = WriteProcessMemory(target_process, inject_address, list_of_modules, num_bytes_to_write, &num_bytes_written);
	if (!did_write_memory || num_bytes_written != num_bytes_to_write)
	{
		PLOG_FATAL << L"[!] Failed to write payload to memory in target process. Error: " << GetLastError();
		goto cleanup;
	}
	PLOG_DEBUG << L"---[+] " << num_bytes_written << L" bytes written";
	PLOG_DEBUG << L"---[+] Bytes written: "; 
	for (unsigned idx=0; idx < num_bytes_to_write; idx++)
	{
		PLOG_DEBUG.printf("%02x ", ((char*)list_of_modules)[idx]);
	}

	k32dll = GetModuleHandleW(L"kernel32.dll");
	if (!k32dll)
	{
		PLOG_FATAL << L"[!] Failed to get module handle for kernel32. Error: " << GetLastError();
		goto cleanup;
	}
	
	load_lib_addr = GetProcAddress(k32dll, "LoadLibraryW");
	if (!load_lib_addr)
	{
		PLOG_FATAL << L"[!] Failed to get address of LoadLibrary. Error: " << GetLastError();
		goto cleanup;
	}

	// Run each module
	PLOG_INFO << L"[+] Starting injected DLLs...";
	for(std::wstring mod : user_options.modules)
	{
		PLOG_DEBUG.printf(L"[+] Starting %s (0x%llX)...", mod.c_str(), reinterpret_cast<std::uintptr_t>(inject_address));
		injected_thread = CreateRemoteThreadEx(target_process, NULL, DEFAULT_STACK_SIZE, (LPTHREAD_START_ROUTINE)load_lib_addr, inject_address, 0, NULL, NULL);
		if (!injected_thread)
		{
			PLOG_FATAL << L"[!] Failed to create thread in target process. Error: " << GetLastError();
			goto cleanup;
		}

		inject_address = (void*)(reinterpret_cast<std::uintptr_t>(inject_address) + GetSizeOfWStringBytes(mod));
	}
	
	PLOG_DEBUG << L"[+] Success!";

	return_code = EXIT_SUCCESS;

cleanup:
	if (return_code == EXIT_FAILURE)
	{
		std::wcout << L"[!] Be sure that the target application is running and that you have the user privileges to access the process at an administrator level\n";
		std::wprintf(USAGE, argv[0], argv[0], argv[0]);
		std::wcout << L"Press ENTER to exit...";
		std::getwchar();
	}
	if (inject_address && target_process && (!did_write_memory || num_bytes_written != num_bytes_to_write || injected_thread == NULL)) VirtualFreeEx(target_process, inject_address, 0, MEM_RELEASE);
	if (injected_thread) CloseHandle(injected_thread);
	if (target_process) CloseHandle(target_process);

	return return_code;
}