#pragma once
#include <Windows.h>
#include "graphics_api.h"
#include "..\..\include\imgui.h"

class UiManager 
{
    public:
        UiManager();
        ~UiManager();

        void InitializeImGui();
        void RenderUiElements();
        void CreateTestWindow();

    private:
        static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);   // Static because otherwise I can't use in SetWindowLongPtrW
        static WNDPROC original_wndproc_;                                                   // Static so I can use it in WndProc
        HWND target_window_;
        ImGuiContext* mod_context_;
};
