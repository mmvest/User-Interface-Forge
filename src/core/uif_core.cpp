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
 * @date    2024-10-02 (version 0.2.0)
 *          2024-10-02 (version 0.1.1)
 *          2024-09-25 (version 0.1.0)
 * 
 * @todo Clean up code -- some of this stuff is rough
 * @todo Comment functions
 * @todo BUG: disappears when fullscreen/screen-resize
 * @todo create management window for enabling/disabling modules and getting debug info such as time elapsed to run each module, memory usage, etc.
 */



#include <Windows.h>
#include <string>
#include <thread>
#include <atomic>
#include "..\..\include\kiero.h"
#include "graphics_api.h"
#include "core_utils.h"

// *********************************
// * Function Forward Declarations *
// *********************************
BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved);
DWORD WINAPI CoreMain(LPVOID unused_param);
void CleanupUiForge();

// ********************
// * Global Variables *
// ********************

// For Kiero
static const unsigned D3D11_PRESENT_FUNCTION_INDEX  = 8;

// ImFont* custom_font;
// static const std::string mods_path = "uif_mods";
// static const std::string fonts_path = "uif_mods\\resources\\fonts\\";

// For cleanup
static std::atomic<bool> is_initialized(false);
HMODULE core_module_handle = NULL;
IGraphicsApi* graphics_api;

// ******************
// * Main Functions *
// ******************

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

            core_module_handle = h_module;
            HANDLE injected_main_thread = CreateThread( NULL,       // Default thread attributes
                                                        0,          // Default stack size
                                                        CoreMain,   // Function for thread to run
                                                        NULL,       // Parameter to pass to the function
                                                        0,          // Creation flags
                                                        NULL);      // Thread id
            if (!injected_main_thread)
            {
                CoreUtils::ErrorMessageBox( std::string("UiForge failed to start properly. Error: " + std::to_string(GetLastError()) + "\n" ).c_str());
                break;
            }

            CloseHandle(injected_main_thread);
            injected_main_thread = NULL;
            break;
        }

        case DLL_PROCESS_DETACH:
            CleanupUiForge();
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
        CoreUtils::ErrorMessageBox( std::string("Failed to initialize graphics api hooking functionality. Kiero status: " + std::to_string(result) + ". (See Kiero github or source for more info)\n").c_str());
        return EXIT_FAILURE;
    }

    // Based on configuration, choose the graphics api. For now, its just D3D11.
    // TODO: Maybe add a switch statement? Something here to choose which Graphics API
    graphics_api = new D3D11GraphicsApi();

    // TODO: Kiero won't work once we have multiple possible Graphics API as it assumes you are using a single API (if I remember correctly)
    result = kiero::bind(D3D11_PRESENT_FUNCTION_INDEX, (void**)&graphics_api->OriginalFunction, graphics_api->HookedFunction);
    if (result != kiero::Status::Success)
    {
        CoreUtils::ErrorMessageBox( std::string("Failed to hook graphics api \"present\" function. Kiero status: " + std::to_string(result) + ". (See Kiero github or source for more info)\n").c_str());
        return EXIT_FAILURE;
    }

    is_initialized.store(true);
    return EXIT_SUCCESS;
}

void CleanupUiForge()
{
    if(is_initialized.load())
    {
        is_initialized.store(false);
        kiero::shutdown();
        graphics_api->CleanupGraphicsApi(nullptr);
        FreeLibrary(core_module_handle);
    }
}