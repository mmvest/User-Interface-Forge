#pragma once
#include <Windows.h>
#include <unordered_map>
#include <mutex>

#include "imgui\imgui.h"
#include "core\forgescript_manager.h"

/**
 * @brief Manages the user interface elements rendered using ImGui.
 */
class UiManager 
{
    public:
        /**
         * @brief Constructs the UI manager for the specified target window.
         *
         * @param target_window Handle to the target window.
         */
        UiManager(HWND target_window, float settings_icon_size_x, float settings_icon_size_y);

        /**
         * @brief Destroys the UI manager and cleans up resources.
         */
        ~UiManager();

        /**
         * @brief Initializes the ImGui library for rendering UI elements.
         */
        void InitializeImGui();

        /**
         * @brief Renders a transparent settings icon in the UI.
         *
         * Clicking the icon toggles the visibility of the settings window.
         */
        void RenderSettingsIcon(void* settings_icon);

        /**
         * @brief Renders the settings window with options managed by the script manager.
         *
         * This function displays a settings window if the icon toggle is active.
         *
         * @param script_manager Reference to the script manager.
         */
        void RenderSettingsWindow(ForgeScriptManager& script_manager);

        /**
         * @brief Renders all UI elements, including script-managed components and settings.
         *
         * @param script_manager Reference to the script manager.
         */
        void RenderUiElements(ForgeScriptManager& script_manager, void* settings_icon);

        /**
         * @brief Updates the target window handle if the swap chain output changes.
         *
         * @param new_target_window Handle to the new target window.
         * @return True if the window was updated.
         */
        bool UpdateTargetWindow(HWND new_target_window);

        
        /**
         * @brief Cleans up resources used by ImGui and restores the original window procedure.
         */
        void CleanupUiManager();

        /**
         * @brief Creates a test window with various sample UI components for demonstration.
         */
        void CreateTestWindow();

        ImGuiContext* mod_context;

    private:
        /**
         * @brief Static window procedure handler for processing ImGui input events.
         *  
         * @note This function is static so it can be used in `SetWindowLongPtrW`
         * 
         * @param hWnd Handle to the window.
         * @param msg The message sent to the window.
         * @param wParam Additional message information.
         * @param lParam Additional message information.
         * @return LRESULT result of the message processing.
         */
        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
         
        static std::mutex wndproc_mutex;
        static std::unordered_map<HWND, WNDPROC> original_wndprocs;
        static ImGuiContext* imgui_context;

        /**
         * @brief Replaces a window's procedure so keyboard input can be routed through ImGui.
         *
         * This keeps the original WNDPROC so we can forward messages and cleanly restore the host window
         * when the UI manager is destroyed.
         *
         * @param hwnd Handle to the window to hook.
         * @return True if the hook is installed (or already present).
         */
        static bool HookWndProc(HWND hwnd);

        /**
         * @brief Restores a previously-hooked window procedure.
         *
         * Restoring the original WNDPROC prevents leaving the host application in a broken state after
         * unloading or when the swap chain output window changes.
         *
         * @param hwnd Handle to the window to unhook.
         */
        static void UnhookWndProc(HWND hwnd);

        /**
         * @brief Restores all window procedures hooked by this UI manager.
         *
         * This is used during shutdown so every window we touched is returned to its original message
         * handling behavior.
         */
        static void UnhookAllWndProcs();

        /**
         * @brief Looks up the saved original window procedure for a hooked HWND.
         *
         * This allows our custom WndProc to forward messages to the host's original handler when we
         * are not explicitly capturing keyboard input.
         *
         * @param hwnd Handle to the window whose original WNDPROC is needed.
         * @return The original WNDPROC, or nullptr if the window is not hooked.
         */
        static WNDPROC GetOriginalWndProc(HWND hwnd);

        /**
         * @brief Hooks all windows owned by the current process.
         *
         * Some apps create multiple HWNDs (or re-parent / recreate children) and keyboard focus can land on
         * a different window than the swap chain output. Hooking all process windows makes the keyboard
         * capture behavior consistent across those cases.
         */
        static void HookAllProcessWindows();

        /**
         * @brief Enumeration callback for hooking child windows.
         *
         * This is used with window enumeration APIs so we can hook newly discovered child
         * HWNDs without needing the host app to explicitly expose them.
         *
         * @param hwnd Child window handle encountered during enumeration.
         * @param lparam User data passed through enumeration.
         * @return TRUE to continue enumeration.
         */
        static BOOL CALLBACK HookChildProc(HWND hwnd, LPARAM lparam);

        /**
         * @brief Enumeration callback for hooking top-level windows.
         *
         * Hooking top-level windows helps ensure we see keyboard messages even when focus is placed on a
         * different HWND than the one we originally targeted.
         *
         * @param hwnd Top-level window handle encountered during enumeration.
         * @param lparam User data passed through enumeration.
         * @return TRUE to continue enumeration.
         */
        static BOOL CALLBACK HookTopLevelProc(HWND hwnd, LPARAM lparam);
        HWND target_window;
        HWND root_window;
        bool show_settings;
        ImVec2 settings_icon_size;

};
