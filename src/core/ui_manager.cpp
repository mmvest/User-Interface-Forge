#include <cstddef>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "imgui\imgui_impl_win32.h"
#include "plog\Log.h"

#include "core\ui_manager.h"
#include "core\graphics_api.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern std::atomic<bool> needs_cleanup; // From core.cpp

// Since we are now hooking multiple windows, we use this to keep track of the original WndProcs
std::unordered_map<HWND, WNDPROC> UiManager::original_wndprocs;

// The mutex protects the map because hooks can be installed/removed while messages are arriving.
std::mutex UiManager::wndproc_mutex;

ImGuiContext* UiManager::imgui_context = nullptr;

UiManager::UiManager(HWND target_window, float settings_icon_size_x, float settings_icon_size_y) : target_window(target_window), root_window(nullptr), show_settings(false), settings_icon_size(settings_icon_size_x, settings_icon_size_y)
{
    InitializeImGui();
};

UiManager::~UiManager()
{
    CleanupUiManager();
}

void UiManager::InitializeImGui() 
{
    // In some apps, like those using Qt, the swapchain output window may be a child. Since keyboard focus
    // may go to the root window instead of the child, we want to hook both so we see keyboard messages
    // regardless of which HWND ends up receiving the keyboard .
    root_window = GetAncestor(target_window, GA_ROOT);
    if (!root_window)
        root_window = target_window;

    if (!HookWndProc(target_window))
        throw std::runtime_error("Failed to set new address for the window procedure. Error: " + std::to_string(GetLastError()));

    if (root_window != target_window)
    {
        if (!HookWndProc(root_window))
            throw std::runtime_error("Failed to set new address for the root window procedure. Error: " + std::to_string(GetLastError()));
    }

    // We want to hook any other windows owned by this process just in case any random window is eating
    // the keyboard inputs that we want for our UI. Kinda heavy-handed, but best that I got for now.
    // Technically this also hooks the root window, but based on MSDN, using enumwindows (which this
    // function does) is "more reliable" than using GetWindow in a loop, but I still hook the parent
    // window explicitly JUUUUUST in case something goes wrong here.
    HookAllProcessWindows();
    
    mod_context = ImGui::CreateContext();
    
    ImGui::SetCurrentContext(mod_context);
    imgui_context = mod_context;
    ImGuiIO& io = ImGui::GetIO();
    
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

    // Prefer the top-level/root HWND for proper IME/text handling.
    if(!ImGui_ImplWin32_Init(root_window)) throw std::runtime_error("Unable to initialize ImGui Win32 Implementation.");
}

bool UiManager::HookWndProc(HWND hwnd)
{
    if (!hwnd)
        return false;

    // Hooking can be triggered from initialization and rescanning/window enumeration callbacks.
    // Because of this and the fact multiple threads could try accessing original_wndprocs simultaneously,
    // we gotta start being safer and actually use mutexes to keep us from race conditions, double-hooking, etc.
    std::lock_guard<std::mutex> lock(wndproc_mutex);
    if (original_wndprocs.find(hwnd) != original_wndprocs.end())
        return true;

    // Replace the window procedure so we can route keyboard messages into ImGui and optionally block them
    // when ImGui wants to capture the keyboard. We save the previous WNDPROC so message handling remains
    // correct when we forward to the host app, and so we can restore it on shutdown.
    WNDPROC original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    if (!original)
        return false;

    original_wndprocs.emplace(hwnd, original);
    return true;
}

void UiManager::UnhookWndProc(HWND hwnd)
{
    if (!hwnd)
        return;

    std::lock_guard<std::mutex> lock(wndproc_mutex);
    auto it = original_wndprocs.find(hwnd);
    if (it == original_wndprocs.end())
        return;

    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(it->second));
    original_wndprocs.erase(it);
}

WNDPROC UiManager::GetOriginalWndProc(HWND hwnd)
{
    std::lock_guard<std::mutex> lock(wndproc_mutex);
    auto it = original_wndprocs.find(hwnd);
    if (it == original_wndprocs.end())
        return nullptr;
    return it->second;
}

void UiManager::RenderSettingsIcon(void* settings_icon)
{
    static bool is_dragging_icon = false;

    unsigned window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("Settings Icon", nullptr, window_flags))
    {
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImGuiIO& io = ImGui::GetIO();
        const char* tooltip_text = "UiForge Settings";

        // Render the button -- this must happen before the click-n-drag logic
        // or else that logic won't work!
        if(ImGui::InvisibleButton("##UiForge Settings Icon", settings_icon_size))
        {
            if (!is_dragging_icon)
            {
                show_settings = !show_settings;
            }
        }

        const bool is_hovered = ImGui::IsItemHovered();

        // Click-n-drag logic!
        const bool is_dragging_now = ImGui::IsItemActive()
            && ImGui::IsMouseDown(ImGuiMouseButton_Left)
            && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f);

        if (is_hovered && !is_dragging_now)
        {
            ImGui::SetTooltip(tooltip_text);
        }

        if (is_dragging_now)
        {
            is_dragging_icon = true;
            
            // Get our drag offset for the icon
            ImVec2 icon_window_pos = ImGui::GetWindowPos();
            icon_window_pos.x += io.MouseDelta.x;
            icon_window_pos.y += io.MouseDelta.y;
            ImGui::SetWindowPos(icon_window_pos, ImGuiCond_Always);
        }
        else
        {
            is_dragging_icon = false;
        }

        ImVec4 icon_tint(1, 1, 1, (is_hovered || is_dragging_icon) ? 1.0f : 0.2f);
        ImGui::SetCursorPos(cursor_pos);
        ImGui::Image(settings_icon, settings_icon_size, ImVec2(0, 0), ImVec2(1, 1), icon_tint);
    }
    ImGui::End();
}

void UiManager::RenderSettingsWindow(ForgeScriptManager& script_manager)
{
    static ForgeScript* selected_script;    // Static so the selected script stays selected

    if (ImGui::Begin("UiForge Settings"))
    {
        ImGuiStyle& style = ImGui::GetStyle();
        float line_height = ImGui::GetTextLineHeightWithSpacing();
        float checkbox_height = ImGui::GetFrameHeight();
        ImVec2 parent_window_size = ImGui::GetContentRegionAvail();
        ImVec2 forgescript_list_window_size(parent_window_size.x * 0.3f - style.ItemSpacing.x * 0.5, parent_window_size.y * 0.75f);
        ImVec2 settings_child_window_size(parent_window_size.x * 0.7f - style.ItemSpacing.x * 0.5, parent_window_size.y * 0.75f);
        
        if(ImGui::BeginChild("ForgeScript List", forgescript_list_window_size, ImGuiChildFlags_Border))
        {
            for (unsigned idx = 0; idx < script_manager.GetScriptCount(); idx++)
            {
                ForgeScript* current_script = script_manager.GetScript(idx);
                std::filesystem::path script_path(current_script->GetFileName());
                bool is_current_script_selected = (selected_script == current_script);
                if(ImGui::Selectable((std::string("##selectable_") + script_path.filename().string()).c_str(), is_current_script_selected, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, line_height)))
                {
                    // If we click on a selected script and it is already selected, then deselect
                    if (is_current_script_selected)
                    {
                        selected_script = nullptr;
                    }
                    else
                    {
                        selected_script = current_script;
                    }
                }
                ImGui::SameLine();
                bool script_enabled = current_script->IsEnabled();
                if(ImGui::Checkbox((std::string("##toggle_") + script_path.filename().string()).c_str(), &script_enabled))
                {
                    if(script_enabled) current_script->Enable();
                    else current_script->Disable();
                }
                ImGui::SameLine();
                ImGui::TextUnformatted(script_path.filename().string().c_str()); // Manually placing name instead of using checkbox label so that clicking the name doesn't toggle the checkbox
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        if(ImGui::BeginChild("ForgeScript Settings", settings_child_window_size, ImGuiChildFlags_Border))
        {
            if(ImGui::BeginTabBar("ForgeScript Settings Tabs"))
            {
                if(ImGui::BeginTabItem("Settings"))
                {
                    if(selected_script)
                    {
                        try
                        {
                            selected_script->RunSettingsCallback();
                        }
                        catch(const std::exception& err)
                        {
                            selected_script->Disable();
                            PLOG_ERROR << err.what();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("Debug"))
                {
                    // If a script is selected, show stats about that script
                    if(selected_script)
                    {
                        size_t avg_time_loading     = (selected_script->stats.times_executed) ? selected_script->stats.total_time_loading_from_mem / selected_script->stats.times_executed : 0;
                        size_t avg_time_executing   = (selected_script->stats.times_executed) ? selected_script->stats.total_time_executing / selected_script->stats.times_executed : 0;
                        ImGui::Text("File Name                                  : %s", selected_script->GetFileName().c_str());
                        ImGui::Text("File Contents Size                         : %llu bytes", selected_script->stats.script_size);
                        ImGui::Text("Time to Read File                          : %llu microseconds", selected_script->stats.time_to_read_file_contents);
                        ImGui::Text("Time to Hash File                          : %llu microseconds", selected_script->stats.time_to_hash_file_contents);
                        ImGui::Text("Avg Time Loading Script From Memory        : %llu microseconds", avg_time_loading);
                        ImGui::Text("Avg Time Executing                         : %llu microseconds", avg_time_executing);
                        ImGui::Text("Number of Times Script Executed            : %llu", selected_script->stats.times_executed);
                    }

                    // If no script is selected, show total stats
                    if(!selected_script)
                    {
                        script_manager.UpdateDebugStats();
                        size_t avg_time_loading     = (script_manager.stats.times_executed) ? script_manager.stats.total_time_loading_from_mem / script_manager.stats.times_executed : 0;
                        size_t avg_time_executing   = (script_manager.stats.times_executed) ? script_manager.stats.total_time_executing / script_manager.stats.times_executed : 0;
                        ImGui::Text("Total File Contents Size                   : %llu bytes", script_manager.stats.script_size);
                        ImGui::Text("Time Reading Files                         : %llu microseconds", script_manager.stats.time_to_read_file_contents);
                        ImGui::Text("Time Hashing Files                         : %llu microseconds", script_manager.stats.time_to_hash_file_contents);
                        ImGui::Text("Avg Time Loading Scripts From Memory       : %llu microseconds", avg_time_loading);
                        ImGui::Text("Avg Time Executing Scripts                 : %llu microseconds", avg_time_executing);
                        ImGui::Text("Number of Times Scripts Executed           : %llu", script_manager.stats.times_executed);                        
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();

        const char* close_label = "Close##UiForgeSettingsClose";
        const ImVec2 close_text_size = ImGui::CalcTextSize(close_label);
        float close_button_width = (close_text_size.x + style.FramePadding.x * 2.0f);
        if (close_button_width < 80.0f) close_button_width = 80.0f;

        const char* eject_label = "Eject";
        const ImVec2 eject_text_size = ImGui::CalcTextSize(eject_label);
        float eject_button_width = (eject_text_size.x + style.FramePadding.x * 2.0f);
        if (eject_button_width < 80.0f) eject_button_width = 80.0f;
 
        const ImVec2 content_max = ImGui::GetWindowContentRegionMax();
        float buttons_x = content_max.x - (close_button_width + style.ItemSpacing.x + eject_button_width);
        float eject_y = content_max.y - ImGui::GetFrameHeight();
        if (buttons_x < style.WindowPadding.x) buttons_x = style.WindowPadding.x;
        if (eject_y < style.WindowPadding.y) eject_y = style.WindowPadding.y;
 
        ImGui::SetCursorPos(ImVec2(buttons_x, eject_y));
        if (ImGui::Button(close_label, ImVec2(close_button_width, 0.0f)))
        {
            show_settings = false;
        }
        ImGui::SameLine(0.0f, style.ItemSpacing.x);
        if (ImGui::Button(eject_label, ImVec2(eject_button_width, 0.0f)))
        {
            needs_cleanup = true;
        }
    }
    ImGui::End();
}

void UiManager::RenderUiElements(ForgeScriptManager& script_manager, void* settings_icon)
{
    ImGui::SetCurrentContext(mod_context);

    // We want to periodically hook new windows created after startup. For example, 
    // PCSX2 (the emulator) uses QT which can create/destroy child HWNDs dynamically.
    static ULONGLONG last_hook_scan_ms = 0;
    const ULONGLONG now_ms = GetTickCount64();
    if (now_ms - last_hook_scan_ms >= 2000)
    {
        last_hook_scan_ms = now_ms;
        HookAllProcessWindows();
    }
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // Execute all UI Mods
    // CreateTestWindow();
    RenderSettingsIcon(settings_icon);
    if(show_settings) RenderSettingsWindow(script_manager);
    script_manager.RunScripts();

    ImGui::Render();
}

bool UiManager::UpdateTargetWindow(HWND new_target_window)
{
    if (!new_target_window || new_target_window == target_window)
    {
        return false;
    }

    const HWND old_target = target_window;
    const HWND old_root = root_window;
    UnhookWndProc(old_target);
    if (old_root && old_root != old_target)
        UnhookWndProc(old_root);

    ImGui_ImplWin32_Shutdown();

    target_window = new_target_window;
    root_window = GetAncestor(target_window, GA_ROOT);
    if (!root_window)
        root_window = target_window;

    if (!HookWndProc(target_window))
    {
        PLOG_WARNING << "Failed to set new address for the window procedure after window change. Error: " << GetLastError();
    }

    if (root_window != target_window)
    {
        if (!HookWndProc(root_window))
            PLOG_WARNING << "Failed to set new address for the root window procedure after window change. Error: " << GetLastError();
    }

    if (!ImGui_ImplWin32_Init(root_window))
    {
        PLOG_WARNING << "Unable to initialize ImGui Win32 Implementation after window change.";
    }

    return true;
}

void UiManager::CleanupUiManager()
{
    if(mod_context)
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(mod_context);
    }

    imgui_context = nullptr;
    UnhookAllWndProcs();
}

void UiManager::UnhookAllWndProcs()
{
    std::lock_guard<std::mutex> lock(wndproc_mutex);
    for (const auto& [hwnd, original] : original_wndprocs)
    {
        if (hwnd && original)
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
    }
    original_wndprocs.clear();
}

BOOL CALLBACK UiManager::HookChildProc(HWND hwnd, LPARAM)
{
    HookWndProc(hwnd);
    return TRUE;
}

BOOL CALLBACK UiManager::HookTopLevelProc(HWND hwnd, LPARAM lparam)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != static_cast<DWORD>(lparam))
        return TRUE;

    HookWndProc(hwnd);
    EnumChildWindows(hwnd, HookChildProc, 0);
    return TRUE;
}

void UiManager::HookAllProcessWindows()
{
    const DWORD pid = GetCurrentProcessId();
    EnumWindows(HookTopLevelProc, static_cast<LPARAM>(pid));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          Manual Input Handling                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief Maps Win32 virtual-key (VK_*) codes to Dear ImGui key identifiers (ImGuiKey_*).
 *
 * This helper converts common navigation, editing, modifier, letter, digit, and function keys
 * into the corresponding ImGuiKey values for manual keyboard input handling. Yes... its a lot.
 *
 * @param vk The Win32 virtual-key code (WPARAM from WM_KEYDOWN/WM_KEYUP).
 * @return The corresponding ImGuiKey value, or ImGuiKey_None if unsupported.
 */
static ImGuiKey VkToImGuiKey(WPARAM vk)
{
    switch (vk)
    {
        case VK_TAB:      return ImGuiKey_Tab;
        case VK_LEFT:     return ImGuiKey_LeftArrow;
        case VK_RIGHT:    return ImGuiKey_RightArrow;
        case VK_UP:       return ImGuiKey_UpArrow;
        case VK_DOWN:     return ImGuiKey_DownArrow;
        case VK_PRIOR:    return ImGuiKey_PageUp;
        case VK_NEXT:     return ImGuiKey_PageDown;
        case VK_HOME:     return ImGuiKey_Home;
        case VK_END:      return ImGuiKey_End;
        case VK_INSERT:   return ImGuiKey_Insert;
        case VK_DELETE:   return ImGuiKey_Delete;
        case VK_BACK:     return ImGuiKey_Backspace;
        case VK_RETURN:   return ImGuiKey_Enter;
        case VK_ESCAPE:   return ImGuiKey_Escape;
        case VK_SPACE:    return ImGuiKey_Space;

        case VK_LSHIFT:   return ImGuiKey_LeftShift;
        case VK_RSHIFT:   return ImGuiKey_RightShift;
        case VK_LCONTROL: return ImGuiKey_LeftCtrl;
        case VK_RCONTROL: return ImGuiKey_RightCtrl;
        case VK_LMENU:    return ImGuiKey_LeftAlt;     // Alt
        case VK_RMENU:    return ImGuiKey_RightAlt;
        case VK_LWIN:     return ImGuiKey_LeftSuper;
        case VK_RWIN:     return ImGuiKey_RightSuper;

        case VK_OEM_1:     return ImGuiKey_Semicolon;   // ;:
        case VK_OEM_PLUS:  return ImGuiKey_Equal;       // =+
        case VK_OEM_COMMA: return ImGuiKey_Comma;       // ,<
        case VK_OEM_MINUS: return ImGuiKey_Minus;       // -_
        case VK_OEM_PERIOD:return ImGuiKey_Period;      // .>
        case VK_OEM_2:     return ImGuiKey_Slash;       // /?
        case VK_OEM_3:     return ImGuiKey_GraveAccent; // `~
        case VK_OEM_4:     return ImGuiKey_LeftBracket; // [{
        case VK_OEM_5:     return ImGuiKey_Backslash;   // \|
        case VK_OEM_6:     return ImGuiKey_RightBracket;// ]}
        case VK_OEM_7:     return ImGuiKey_Apostrophe;  // '"

        default:
            break;
    }

    // Letters
    if (vk >= 'A' && vk <= 'Z')
        return (ImGuiKey)(ImGuiKey_A + (int)(vk - 'A'));

    // Digits (top row)
    if (vk >= '0' && vk <= '9')
        return (ImGuiKey)(ImGuiKey_0 + (int)(vk - '0'));

    // Function keys
    if (vk >= VK_F1 && vk <= VK_F12)
        return (ImGuiKey)(ImGuiKey_F1 + (int)(vk - VK_F1));

    return ImGuiKey_None;
}

static bool HandleKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam)
{
    ImGuiIO& io = ImGui::GetIO();

    // Keep ImGui's modifier state (Ctrl/Shift/Alt/Windows Key) up to date
    auto update_mods = [&io]()
    {
        io.AddKeyEvent(ImGuiKey_ModCtrl, (GetKeyState(VK_CONTROL) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiKey_ModShift, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiKey_ModAlt, (GetKeyState(VK_MENU) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiKey_ModSuper, ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) != 0);
    };

    auto fixup_vk = [](WPARAM vk, LPARAM lp) -> WPARAM
    {
        // Win32 sometimes reports a generic VK_* (e.g., VK_SHIFT/VK_CONTROL/VK_MENU) and expects
        // the app to disambiguate left vs right using scan-code / extended-key bits in lParam.
        // ImGui has distinct left/right key identifiers (e.g., ImGuiKey_LeftCtrl vs RightCtrl),
        // so we normalize here.
        if (vk == VK_SHIFT)
        {
            // For shift, Win32 provides the scan-code in bits 16..23 of lParam.
            // MapVirtualKeyW converts that scan-code to the specific VK_LSHIFT/VK_RSHIFT.
            const UINT scancode = (lp >> 16) & 0xFF;
            return static_cast<WPARAM>(MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX));
        }
        if (vk == VK_CONTROL)
        {
            // For control/alt, the "extended" bit (bit 24) distinguishes right-side keys
            return (lp & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
        }
        if (vk == VK_MENU)
        {
            return (lp & 0x01000000) ? VK_RMENU : VK_LMENU;
        }
        return vk;
    };

    switch (msg)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            // Key down/up -- we forward these to ImGui as key events, not text characters.
            // Yes, there is a difference!
            const bool is_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            const WPARAM fixed_vk = fixup_vk(wParam, lParam);
            const ImGuiKey key = VkToImGuiKey(fixed_vk);

            // Not all VK codes are mapped in VkToImGuiKey(). For unsupported keys we will
            // still update modifiers, but we don't emit an ImGui key event.
            if (key != ImGuiKey_None)
                io.AddKeyEvent(key, is_down);

            update_mods();
            return true;
        }
    }
    return false;
}

LRESULT WINAPI UiManager::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT imgui_handled = 0;
    if (imgui_context)
    {
        ImGui::SetCurrentContext(imgui_context);
        imgui_handled = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    // When ImGui is actively interacting with UI, swallow keyboard input so the host app doesn't also act on it.
    if (imgui_context)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const bool is_keyboard_message =
            msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
            msg == WM_CHAR || msg == WM_SYSCHAR ||
            msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_COMPOSITION || msg == WM_IME_ENDCOMPOSITION;

        if (io.WantCaptureKeyboard && is_keyboard_message)
            return imgui_handled ? imgui_handled : 0;
    }

    if (WNDPROC original = GetOriginalWndProc(hWnd))
        return CallWindowProcW(original, hWnd, msg, wParam, lParam);

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void UiManager::CreateTestWindow()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    ImGui::Begin("Module Template", nullptr, window_flags);                        

    // Text label
    ImGui::Text("Hello, world!");

    // Button
    if (ImGui::Button("Click Me!")) {
        ImGui::Text("Button Clicked!");
    }

    // Checkbox
    static bool checkboxValue = false;
    ImGui::Checkbox("Checkbox", &checkboxValue);

    // Radio Buttons
    static int radioButtonValue = 0;
    ImGui::RadioButton("Radio A", &radioButtonValue, 0);
    ImGui::RadioButton("Radio B", &radioButtonValue, 1);
    ImGui::RadioButton("Radio C", &radioButtonValue, 2);

    // Slider
    static float sliderValue = 0.0f;
    ImGui::SliderFloat("Slider", &sliderValue, 0.0f, 1.0f);

    // Color Picker
    static float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    ImGui::ColorEdit4("Color Picker", color);

    // Input Text
    static char inputText[128] = "Enter text here...";
    ImGui::InputText("Input Text", inputText, IM_ARRAYSIZE(inputText));

    // Combo Box
    const char* items[] = { "Item 1", "Item 2", "Item 3", "Item 4" };
    static int itemIndex = 0;
    ImGui::Combo("Combo Box", &itemIndex, items, IM_ARRAYSIZE(items));

    // List Box
    static int listBoxItemIndex = 0;
    ImGui::ListBox("List Box", &listBoxItemIndex, items, IM_ARRAYSIZE(items), 4);

    // Tooltip
    ImGui::Text("Hover over me!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This is a tooltip");
    }

    // Tree View
    if (ImGui::TreeNode("Tree Node")) {
        ImGui::Text("Tree Item 1");
        ImGui::Text("Tree Item 2");
        ImGui::TreePop();
    }

    // Collapsible Header
    if (ImGui::CollapsingHeader("Collapsible Header")) {
        ImGui::Text("Collapsible content here.");
    }

    // End the ImGui window
    ImGui::End();  
}
