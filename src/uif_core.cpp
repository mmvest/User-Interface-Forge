/**
 * @file uif_core.cpp
 * @version 0.1.0
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
 * @author mmvest (wereox)
 * @date 2024-09-25
 * 
 * @todo Clean up code -- some of this stuff is rough
 * @todo Comment functions
 * @todo BUG: disappears when fullscreen/screen-resize
 * @todo close/free handles and imported libraries and other general cleanup
 * @todo create management window for enabling/disabling modules and getting debug info such as time elapsed to run each module, memory usage, etc.
 * @todo add ability to detach core and all loaded modules from target without crashing or closing the application
 */



#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <d3d11.h>
#include <dxgi.h>
#include "uif_core.h"
#include "..\libs\kiero\kiero.h"
#include "..\libs\imgui\imgui.h"
#include "..\libs\imgui\backends\imgui_impl_win32.h"
#include "..\libs\imgui\backends\imgui_impl_dx11.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

static HMODULE core_module = NULL;  //TODO: is this used??

// For debug console
#ifdef _DEBUG
#define DebugPrint(x) std::wcout << x << std::endl
#else
#define DebugPrint(x)
#endif

static FILE* debug_output           = NULL;
static FILE* debug_input            = NULL;
static bool debug_console_allocated = false;

// For ImGUI and Kiero
static const unsigned D3D11_PRESENT_FUNCTION_INDEX  = 8;
HWND window                                         = NULL;
WNDPROC original_wndproc                            = NULL;
ID3D11Device* d3d11_device                          = NULL;
ID3D11DeviceContext* d3d11_context                  = NULL;
ID3D11RenderTargetView* main_render_target_view     = NULL;
static UINT resize_width                            = 0; 
static UINT resize_height                           = 0;
Present original_d3d11_present_func;
static bool init = false;
ImGuiContext* mod_context;

// For Module Management
static const std::wstring MODS_PATH = L"uif_mods";
std::vector<std::wstring> mod_names;
std::vector<void*> mod_functions; // Vector to store the test functions

void CreateTestWindow()
{
       ImGuiWindowFlags window_flags = ImGuiWindowFlags_None; // Put your flags for the window style here

    ImGui::SetCurrentContext(mod_context);
    ImGui::Begin("Module Template", nullptr, window_flags);                        

    // ===> Put your ImGui code here between ImGui::Begin() and ImGui::End() <===

    //     // Text label
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

BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(h_module);
        core_module = h_module;
        HANDLE injected_main_thread = CreateThread(NULL, 0, CoreMain, NULL, 0, NULL);
        if (!injected_main_thread)
        {
            break;
        }
        CloseHandle(injected_main_thread);
        injected_main_thread = NULL;
    }
    case DLL_PROCESS_DETACH:
        //TODO: add imgui cleanup?
        kiero::shutdown();
        if (debug_input) fclose(debug_input);
        if (debug_output) fclose(debug_output);
        if (debug_console_allocated) FreeConsole();
        break;
    }
    return TRUE;
}

DWORD WINAPI CoreMain(LPVOID param)
{
    if (!AllocateDebugConsole()) return EXIT_FAILURE;

    DebugPrint("[+] Loading UI Mods...");
    if(!LoadUiMods()) return EXIT_FAILURE;

    DebugPrint(L"[+] Initializing Kiero...");
    
    kiero::Status::Enum result = kiero::init(kiero::RenderType::D3D11);
    DebugPrint("---[+] Status: " << result);
    if (result == kiero::Status::Success)
    {
        kiero::bind(D3D11_PRESENT_FUNCTION_INDEX, (void**)&original_d3d11_present_func, (void*)hooked_d3d11_present_func);
    }


    return EXIT_SUCCESS;
}

bool AllocateDebugConsole()
{
#ifdef _DEBUG
    if (!AllocConsole())
    {
        return false;
    }

    errno_t error = 0;
    error = freopen_s(&debug_input, "CONIN$", "r", stdin);
    if (error || !debug_input)
    {
        return false;
    }
    error = freopen_s(&debug_output, "CONOUT$", "w", stdout);
    if (error || !debug_output)
    {
        return false;
    }
    debug_console_allocated = true;
#endif
    return true;
}

bool LoadUiMods()
{
    // Check if MODS_PATH exists
    if(!std::filesystem::exists(MODS_PATH))
    {
        std::wcout << "[!] Warning: could not find module folder " << MODS_PATH << ". uif_core will not be able to load any custom modules. Continuing..." << std::endl;
        return true;
    }

    // Iterate through the directory using std::filesystem
    for (const auto& entry : std::filesystem::directory_iterator(MODS_PATH))
    {
        // Ignore non-dll files
        if (!entry.is_regular_file() || entry.path().extension() != ".dll")
        {
            continue;
        }

        std::wstring mod_path = entry.path().wstring();
        mod_names.emplace_back(mod_path);
        DebugPrint(L"---[+] " << mod_path);
        HMODULE mod_handle = LoadLibraryW(mod_path.c_str());
        if (!mod_handle)
        {
            DebugPrint(L"[!] Failed to get module handle for " << mod_path << L" (Error " << GetLastError() << L"). Continuing...");
            continue;
        }

        // Get the function address
        DebugPrint(L"------[+] Getting address of mod function...");
        void* ui_mod_func = reinterpret_cast<void*>(GetProcAddress(mod_handle, "ShowUiMod"));
        DebugPrint(L"------[+] Address: " << ui_mod_func);
        if (!ui_mod_func)
        {
            DebugPrint(L"Failed to get function \"ShowUiMod\" from " << mod_path << L" (Error " << GetLastError() << L"). Continuing...");
            continue;
        }

        // Store the dll function for calling later
        DebugPrint(L"------[+] Storing mod function address...");
        mod_functions.push_back(ui_mod_func);
    }

    return true;
}

HRESULT __stdcall hooked_d3d11_present_func(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    if (!init)
    {
        DebugPrint(L"[+] Initializing ImGUI...");

        /**
         * Since we are hooking something that already has a d3d11 device, we need to get
         * that device instead of creating it -- so we need to call GetDevice() instead
         * of D3D11CreateDeviceAndSwapChain()
         */ 

        if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11_device)))
        {
            d3d11_device->GetImmediateContext(&d3d11_context);
            DXGI_SWAP_CHAIN_DESC swap_chain_description;
            swap_chain->GetDesc(&swap_chain_description);
            window = swap_chain_description.OutputWindow;

            ID3D11Texture2D* back_buffer = NULL;
            swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
            if(!back_buffer) return original_d3d11_present_func(swap_chain, sync_interval, flags);
            d3d11_device->CreateRenderTargetView(back_buffer, NULL, &main_render_target_view);
            back_buffer->Release();

            original_wndproc = (WNDPROC)SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)WndProc);

            mod_context = ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(d3d11_device, d3d11_context);
            init = true;
        }

        else
        {
            return original_d3d11_present_func(swap_chain, sync_interval, flags);
        }
            
    }
    DebugPrint(L"[+] Rendering ui elements...");

    DebugPrint(L"---[+] New Frames...");
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Execute all UI Mods
    DebugPrint(L"---[+] Executing Mods...");
    ExecuteUiMods();
    //CreateTestWindow();

    DebugPrint(L"---[+] Rendering...");
    ImGui::Render();
    DebugPrint(L"------[+] Set Render Target...");
    d3d11_context->OMSetRenderTargets(1, &main_render_target_view, nullptr);
    DebugPrint(L"------[+] RenderDrawData...");
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    DebugPrint(L"---[+] Running Original Func...");
    return original_d3d11_present_func(swap_chain, sync_interval, flags);
}

void ExecuteUiMods()
{
    unsigned idx = 0;
    for (void* mod_func : mod_functions)
    {
        DebugPrint(L"------[+] Executing function from " << mod_names[idx].c_str());
        (reinterpret_cast<void (*)(ImGuiContext*)>(mod_func))(mod_context);
        DebugPrint(L"---------[+] Done");
        idx++;
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    return CallWindowProcW(original_wndproc, hWnd, msg, wParam, lParam);
}

