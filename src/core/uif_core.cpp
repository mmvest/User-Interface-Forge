/**
 * @file uif_core.cpp
 * @version 0.1.1
 * @brief DLL for injecting into a target process to hook graphics API and display ImGUI windows.
 * 
 * This file defines a dynamic-link library (DLL) that is designed to be injected into a target 
 * process. Upon injection, the DLL will:
 * 
 * 1. Load all DLLs located in the "uif_mods" directory.
 * 2. Utilize the Kiero library to hook the chosen graphics API (e.g., DirectX, Vulkan, etc.).
 * 3. Initialize the Dear ImGUI framework to create and manage graphical user interface (GUI) windows.
 * 4. Execute the custom modules code, displaying custom imgui windows or performing other tasks.
 *
 *
 * @note Ensure that the target process is compatible with the graphics API being hooked.
 * 
 * @warning You use this module at your own risk. You are responsible for how you use this code.
 *          Be careful about what DLLs you throw into the uif_mods directory. This code will
 *          load ANY DLLs in there, including any that may contain malicious code. Only use
 *          modules from trusted sources, and only those that you KNOW are not malicious.
 * 
 * @author  mmvest (wereox)
 * @date    2024-10-02 (version 0.1.1)
 *          2024-09-25 (version 0.1.0)
 * 
 * @todo Clean up code -- some of this stuff is rough
 * @todo Comment functions
 * @todo BUG: disappears when fullscreen/screen-resize
 * @todo close/free handles and imported libraries and other general cleanup
 * @todo create management window for enabling/disabling modules and getting debug info such as time elapsed to run each module, memory usage, etc.
 * @todo add ability to detach core and all loaded modules from target without crashing or closing the application
 */



#include <Windows.h>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <d3d11.h>
#include <dxgi.h>
#include <stdexcept>
#include "..\..\include\kiero.h"
#include "..\..\include\imgui.h"
#include "..\..\include\imgui_impl_win32.h"
#include "..\..\include\imgui_impl_dx11.h"

// *********************************
// * Function Forward Declarations *
// *********************************

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);   // For Imgui to be able to call their window handler
typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);   // For storing and calling the original present function
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);                                     // For storing and calling the original WNDPROC function
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ExecuteUiMods();
bool LoadUiMods();
void CreateTestWindow();
void UnloadMods();
void CoreCleanup();
BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved);
DWORD WINAPI CoreMain(LPVOID unused_param);
HRESULT __stdcall hooked_d3d11_present_func(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
void InitializeGraphicsApi(IDXGISwapChain* swap_chain);
void InitializeUI(IDXGISwapChain* swap_chain);
void ProcessCustomInputs();
void RenderUiElements();
void ErrorMessageBox(const char* err_msg);
void InfoMessageBox(const char* info_msg);

// ********************
// * Global Variables *
// ********************

#pragma region globals

// For ImGUI and Kiero
static const unsigned D3D11_PRESENT_FUNCTION_INDEX  = 8;
HWND target_window                                  = NULL;
WNDPROC original_wndproc                            = NULL;
ID3D11Device* d3d11_device                          = NULL;
ID3D11DeviceContext* d3d11_context                  = NULL;
ID3D11RenderTargetView* main_render_target_view     = NULL;
static UINT resize_width                            = 0; 
static UINT resize_height                           = 0;
Present original_d3d11_present_func                 = NULL;
static bool is_ui_initialized                              = false;
ImGuiContext* mod_context                           = NULL;

ImFont* custom_font;
static const std::string fonts_path = "uif_mods\\resources\\fonts\\";

// For Module Management
static HMODULE core_module = NULL;
bool mod_exiting = false;

#pragma endregion

// ******************
// * Main Functions *
// ******************

#pragma region main_funcs

BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved)
/**
 * @brief Entry point for DLL, handling process attach/detach and creating a thread that will run the main logic.
 *
 * @param[in] h_module   Handle to the loaded module (DLL)
 * @param[in] ul_reason_for_call  Reason for calling DllMain (attach or detach)
 * @param[in] reserved Unused parameter (reserved for future use)
 *
 * @return TRUE if DLL initialization was successful, FALSE otherwise
 */
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(h_module);                    // No need for DLL thread attach/detach

            core_module = h_module;
            HANDLE injected_main_thread = CreateThread( NULL,       // Default thread attributes
                                                        0,          // Default stack size
                                                        CoreMain,   // Function for thread to run
                                                        NULL,       // Parameter to pass to the function
                                                        0,          // Creastion flags
                                                        NULL);      // Thread id
            if (!injected_main_thread)
            {
                ErrorMessageBox( std::string("UiForge failed to start properly. Error: " + std::to_string(GetLastError()) + "\n" ).c_str());
                break;
            }

            CloseHandle(injected_main_thread);
            injected_main_thread = NULL;
            break;
        }

        case DLL_PROCESS_DETACH:
            break;
    }
    
    return TRUE;
}

DWORD WINAPI CoreMain(LPVOID unused_param)
/**
 * @brief Initializes graphics API hooking functionality using kiero library.
 *         Binds D3D11 present function and sets up necessary hooks for further processing.
 *
 * @param unused_param [in] Unused parameter required by WINAPI convention (not used in this implementation)
 *
 * @return Exit status code, indicating success or failure
 */
{
    
    kiero::Status::Enum result = kiero::init(kiero::RenderType::D3D11);
    if (result != kiero::Status::Success)
    {
        ErrorMessageBox( std::string("Failed to initialize graphics api hooking functionality. Kiero status: " + std::to_string(result) + ". (See Kiero github or source for more info)\n").c_str());
        return EXIT_FAILURE;
    }

    result = kiero::bind(D3D11_PRESENT_FUNCTION_INDEX, (void**)&original_d3d11_present_func, (void*)hooked_d3d11_present_func);
    if (result != kiero::Status::Success)
    {
        ErrorMessageBox( std::string("Failed to hook graphics api \"present\" function. Kiero status: " + std::to_string(result) + ". (See Kiero github or source for more info)\n").c_str());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

HRESULT __stdcall hooked_d3d11_present_func(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    if (!is_ui_initialized)
    {
        try
        {
            InitializeUI(swap_chain);
        }
        catch (const std::exception& err)
        {
            ErrorMessageBox(err.what());
            CoreCleanup();
            return original_d3d11_present_func(swap_chain, sync_interval, flags);
        }

        is_ui_initialized = true;
    }

    RenderUiElements();

    ProcessCustomInputs();  // Put this here so it will return straight into calling the original D3D11 function
    return original_d3d11_present_func(swap_chain, sync_interval, flags);
}

#pragma endregion

// **********************************
// * CLEANUP AND EXCEPTION HANDLING *
// **********************************

#pragma region cleanup

void CoreCleanup()
{
    mod_exiting = true;

    // Shutdown ImGui
    if(is_ui_initialized)
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(mod_context);
    }

    // Restore original Windows Procedure
    if(original_wndproc)
    {
        if(!SetWindowLongPtrW(target_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc)))
        {
            std::string err_msg = std::string("Failed to restore original window procedure. Error: ") + std::to_string(GetLastError());
            ErrorMessageBox(err_msg.c_str());
        }
    }

    // Release directx resources
    if(d3d11_device != nullptr)
    {
        d3d11_device->Release();
        d3d11_device = NULL;
    }

    if(d3d11_context != nullptr)
    {
        d3d11_context->Release();
        d3d11_context = NULL;
    }

    is_ui_initialized = false;

    // Unhook graphics API
    kiero::shutdown();

    // Free the library
    FreeLibrary(core_module);
}

#pragma endregion

// ******************
// * USER INTERFACE *
// ******************

#pragma region ui

void InitializeGraphicsAPI(IDXGISwapChain* swap_chain)
/**
 * @brief Initializes the DirectX 11 graphics API by performing a series of essential steps.

 * This function takes the IDXGISwapChain instance as input, obtains the context,
 * and uses the swap chain to obtain the description, back buffer, and create the render target view.
 *
 * @param swap_chain The IDXGISwapChain instance that contains the essential information about the DirectX 11 swap chain.
 */
{
        HRESULT result = swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11_device);
        if(FAILED(result))
        {
            throw std::runtime_error("Failed to get directx device. Error: " + std::to_string(result));
        }

        d3d11_device->GetImmediateContext(&d3d11_context);

        DXGI_SWAP_CHAIN_DESC swap_chain_description;
        result = swap_chain->GetDesc(&swap_chain_description);
        if(FAILED(result)) throw std::runtime_error("Failed to get the description of the DirectX 11 swapchain. Error: " + std::to_string(result));

        target_window = swap_chain_description.OutputWindow;

        ID3D11Texture2D* back_buffer = NULL;
        result = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
        if(FAILED(result)) throw std::runtime_error("Failed to get the DirectX backbuffer. Error: " + std::to_string(result));
       
        result = d3d11_device->CreateRenderTargetView(back_buffer, NULL, &main_render_target_view);
        if(FAILED(result)) throw std::runtime_error("Failed to create render target view. Error: " + std::to_string(result));

        back_buffer->Release();
}

void InitializeImGui()
/**
 * @brief Initializes the ImGui library for rendering UI elements.
 * 
 * This function sets up the necessary infrastructure to integrate ImGui into the existing graphics pipeline
 * 
 * @param none
 */
{
    original_wndproc = (WNDPROC)SetWindowLongPtrW(target_window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    if(!original_wndproc) throw std::runtime_error("Failed to set new address for the window procedure. Error: " + std::to_string(GetLastError()));
    
    mod_context = ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    if(!ImGui_ImplWin32_Init(target_window)) throw std::runtime_error("Unable to initialize ImGui Win32 Implementation.");

    if(!ImGui_ImplDX11_Init(d3d11_device, d3d11_context)) throw std::runtime_error("Unable to initialize ImGui DX11 Implementation.");
}

void InitializeUI(IDXGISwapChain* swap_chain)
/**
 * @brief Initializes the graphics API and UI library.
 *
 * This function retrieves the device from the swap chain, initializes the graphics pipeline,
 * and sets up ImGui for rendering UI elements.
 *
 * @param swap_chain The IDXGISwapChain instance to retrieve the device from.
 */
{
    InitializeGraphicsAPI(swap_chain);
    InitializeImGui();
}

void RenderUiElements()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Execute all UI Mods
    CreateTestWindow();

    ImGui::Render();
    d3d11_context->OMSetRenderTargets(1, &main_render_target_view, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    return CallWindowProcW(original_wndproc, hWnd, msg, wParam, lParam);
}

#pragma endregion

// *********************
// * UTILITY FUNCTIONS *
// *********************

#pragma region utility

void CreateTestWindow()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None; // Put your flags for the window style here

    ImGui::SetCurrentContext(mod_context);
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

void ErrorMessageBox(const char* err_msg)
{
    std::thread([err_msg] { MessageBoxA(nullptr, err_msg, "UiForge Error",  MB_OK | MB_ICONERROR); }).detach();
}

void InfoMessageBox(const char* info_msg)
{
    std::thread([info_msg] { MessageBoxA(nullptr, info_msg, "UiForge message",  MB_OK); }).detach();
}

void ProcessCustomInputs()
{
    uint32_t is_pressed = 0x01;
    if(GetAsyncKeyState(VK_END) & is_pressed)
    {
        InfoMessageBox("End Key Pressed");
        CoreCleanup();
    }
}

#pragma endregion