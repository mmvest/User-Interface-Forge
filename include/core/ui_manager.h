#pragma once
#include <Windows.h>
#include "imgui\imgui.h"
#include "core\forgescript_manager.h"

class UiManager 
{
    public:
        UiManager(HWND target_window);
        ~UiManager();

        void InitializeImGui();
        void RenderUiElements(ForgeScriptManager& script_manager);
        void CleanupUiManager();
        void CreateTestWindow();

        ImGuiContext* mod_context_;

    private:
        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);   // Static because otherwise I can't use in SetWindowLongPtrW
        static WNDPROC original_wndproc_;                                                   // Static so I can use it in WndProc
        HWND target_window_;

};
