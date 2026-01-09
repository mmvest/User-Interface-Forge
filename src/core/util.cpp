#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <thread>

#include "core\util.h"

extern std::atomic<bool> needs_cleanup;         // From uif_core.cpp

namespace CoreUtils
{
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
