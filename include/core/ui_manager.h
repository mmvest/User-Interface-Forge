#pragma once
#include <Windows.h>

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
        UiManager(HWND target_window);

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
        
        static WNDPROC original_wndproc;    // Static so I can use it in WndProc
        HWND target_window;
        bool show_settings;

};
