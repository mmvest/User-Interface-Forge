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

    void ProcessCustomInputs(HWND target_window)
    {
        // If we are not targeting the foreground window, we don't want to process the input
        if (target_window != GetForegroundWindow())
        {
            return; 
        }

        uint32_t is_pressed = 0x8000;
        bool is_custom_input_combo_active = (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                                            (GetAsyncKeyState(VK_MENU)    & 0x8000) &&  // Alt... why is it called MENU? idk...
                                            (GetAsyncKeyState(VK_SHIFT)   & 0x8000);
        if(is_custom_input_combo_active)
        {
            if(GetAsyncKeyState(VK_END) & is_pressed)
            {
                needs_cleanup = true;
            }
        }
    }
}
