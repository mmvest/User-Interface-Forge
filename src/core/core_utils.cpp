#include <thread>
#include <windows.h>
#include <string>
#include <atomic>
#include <filesystem>
#include "core_utils.h"

extern std::atomic<bool> needs_cleanup;         // From uif_core.cpp
extern std::atomic<HMODULE> core_module_handle; // From uif_core.cpp

namespace CoreUtils
{
    std::string GetUiForgeRootDirectory()
    {
        static std::string uiforge_root_dir;

        if (uiforge_root_dir.empty())
        {
            char path_to_dll[MAX_PATH];
            if(!GetModuleFileNameA(core_module_handle, path_to_dll, MAX_PATH))
            {
                throw std::runtime_error(std::string("Failed to get module file name. Error: " + GetLastError()));
            }

            uiforge_root_dir = std::filesystem::path(path_to_dll).parent_path().parent_path().string();
        }
        
        return uiforge_root_dir;
    }

    void ErrorMessageBox(const char* err_msg)
    {
        std::thread([err_msg] { MessageBoxA(nullptr, err_msg, "UiForge Error",  MB_OK | MB_ICONERROR); }).detach();
    }

    void InfoMessageBox(const char* info_msg)
    {
        std::thread([info_msg] { MessageBoxA(nullptr, info_msg, "UiForge Message",  MB_OK); }).detach();
    }

    void ProcessCustomInputs()
    {
        uint32_t is_pressed = 0x01;
        if(GetAsyncKeyState(VK_END) & is_pressed)
        {
            needs_cleanup = true;
        }
    }
}
