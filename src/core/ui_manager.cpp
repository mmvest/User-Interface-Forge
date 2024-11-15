#include <Windows.h>
#include <stdexcept>
#include <string>

#include "core_utils.h"
#include "ui_manager.h"
#include "..\..\include\imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WNDPROC UiManager::original_wndproc_ = nullptr;

UiManager::UiManager(HWND target_window) : target_window_(target_window)
{
    InitializeImGui();
};

UiManager::~UiManager()
{
    CleanupUiManager();
}

void UiManager::InitializeImGui() 
/**
 * @brief Initializes the ImGui library for rendering UI elements.
 * 
 * This function sets up the necessary infrastructure to integrate ImGui into the existing graphics pipeline
 * 
 * @param none
 */
{
    original_wndproc_ = (WNDPROC)SetWindowLongPtrW(target_window_, GWLP_WNDPROC, (LONG_PTR)WndProc);
    if(!original_wndproc_) throw std::runtime_error("Failed to set new address for the window procedure. Error: " + std::to_string(GetLastError()));
    
    mod_context_ = ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    if(!ImGui_ImplWin32_Init(target_window_)) throw std::runtime_error("Unable to initialize ImGui Win32 Implementation.");
}

void UiManager::RenderUiElements(ForgeScriptManager& script_manager)
{
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // Execute all UI Mods
    // CreateTestWindow();
    script_manager.RunScripts();

    ImGui::Render();
}

void UiManager::CleanupUiManager()
{
    if(mod_context_)
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(mod_context_);
    }


    if(original_wndproc_)
    {
        if(!SetWindowLongPtrW(target_window_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc_)))
        {
            std::string err_msg = std::string("Failed to restore original window procedure. Error: ") + std::to_string(GetLastError());
            // TODO: Log error message
        }
    }
}

LRESULT WINAPI UiManager::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    return CallWindowProcW(original_wndproc_, hWnd, msg, wParam, lParam);
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