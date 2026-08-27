#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "core\util.h"

extern std::atomic<bool> needs_cleanup;             // From uif_core.cpp
extern std::atomic<bool> toggle_settings_requested; // From uif_core.cpp

namespace
{
    // Held across frames so holding the combo toggles the settings window once per press.
    bool settings_hotkey_was_down = false;
}

namespace CoreUtils
{
    void ErrorMessageBox(std::string err_msg)
    {
        std::thread([msg = std::move(err_msg)] {
            MessageBoxA(nullptr, msg.c_str(), "UiForge Error", MB_OK | MB_ICONERROR);
        }).detach();
    }

    void InfoMessageBox(std::string info_msg)
    {
        std::thread([msg = std::move(info_msg)] {
            MessageBoxA(nullptr, msg.c_str(), "UiForge Message", MB_OK);
        }).detach();
    }

    void ProcessCustomInputs()
    {
        DWORD foreground_pid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
        if (foreground_pid != GetCurrentProcessId())
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

            // Only fire on the press itself, otherwise holding the combo toggles every frame.
            bool settings_hotkey_is_down = (GetAsyncKeyState('O') & is_pressed) != 0;
            if(settings_hotkey_is_down && !settings_hotkey_was_down)
            {
                toggle_settings_requested = true;
            }
            settings_hotkey_was_down = settings_hotkey_is_down;
        }
        else
        {
            settings_hotkey_was_down = false;
        }
    }
}
