/**
 * @file uif_core.cpp
 * @version 0.2.3
 * @brief DLL for injecting into a target process to hook graphics API and display ImGUI windows.
 * 
 * This file defines a dynamic-link library (DLL) that is designed to be injected into a target 
 * process. Upon injection, the DLL will:
 * 
 * 1. Load all scripts located in the "uif_mods" directory.
 * 2. Utilize the Kiero library to hook the chosen graphics API (e.g., DirectX, Vulkan, etc.).
 * 3. Initialize the Dear ImGUI framework to create and manage graphical user interface (GUI) windows.
 * 4. Execute the custom modules code, displaying custom imgui windows or performing other tasks.
 *
 *
 * @note    Ensure that the target process is compatible with the graphics API being hooked.
 * 
 * @warning You use this module at your own risk. You are responsible for how you use this code.
 *          Be careful about what lua scripts you throw into the uif_mods directory. This code will
 *          load ANY scripts in there, including any that may contain malicious code. Only use
 *          modules from trusted sources, and only those that you KNOW are not malicious.
 * 
 * @author  mmvest (wereox)
 * @date    2024-11-15 (version 0.2.3)
 *          2024-11-12 (version 0.2.2)
 *          2024-10-28 (version 0.2.1)
 *          2024-10-02 (version 0.2.0)
 *          2024-10-02 (version 0.1.1)
 *          2024-09-25 (version 0.1.0)
 * 
 * @todo Clean up code -- some of this stuff is rough
 * @todo Comment functions
 * @todo BUG: disappears when fullscreen/screen-resize
 * @todo create management window for enabling/disabling modules and getting debug info such as time elapsed to run each module, memory usage, etc.
 */



#include <Windows.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include "..\..\include\kiero.h"
#include "..\..\include\graphics_api.h"
#include "..\..\include\core_utils.h"
#include "..\..\include\forgescript_manager.h"

// *********************************
// * Function Forward Declarations *
// *********************************
BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved);
DWORD WINAPI CoreMain(LPVOID unused_param);
void OnGraphicsApiInvoke(void* params);
void CleanupUiForge();

// ********************
// * Global Variables *
// ********************

// For Kiero
static const unsigned D3D11_PRESENT_FUNCTION_INDEX  = 8;
kiero::Status::Enum kiero_is_initialized = kiero::Status::NotInitializedError;  // Default value so we know kiero is not initialized
kiero::Status::Enum kiero_is_bound = kiero::Status::UnknownError;               // Default value so we know kiero is not bound

IGraphicsApi* graphics_api;
UiManager* ui_manager;
ForgeScriptManager* script_manager;

// For cleanup
std::atomic<HMODULE> core_module_handle(NULL);
std::atomic<bool> needs_cleanup(false);

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
    kiero_is_initialized = kiero::init(kiero::RenderType::D3D11);
    if (kiero_is_initialized != kiero::Status::Success)
    {
        CoreUtils::ErrorMessageBox( std::string("Failed to initialize graphics api hooking functionality. Kiero status: " + std::to_string(kiero_is_initialized) + ". (See Kiero github or source for more info)\n").c_str());
        return EXIT_FAILURE;
    }

    // Based on configuration, choose the graphics api. For now, its just D3D11.
    // TODO: Maybe add a switch statement? Something here to choose which Graphics API
    graphics_api = new D3D11GraphicsApi();
    graphics_api->OnGraphicsApiInvoke = OnGraphicsApiInvoke;    // We could probably move this to the constructor

    // Load all mods
    try
    {
        script_manager = new ForgeScriptManager("uif_mods");
    }
    catch(const std::exception& err)
    {
        CoreUtils::ErrorMessageBox(err.what());
        return EXIT_FAILURE;
    }

    // TODO: Kiero won't work once we have multiple possible Graphics API as it assumes you are using a single API (if I remember correctly)
    kiero_is_bound = kiero::bind(D3D11_PRESENT_FUNCTION_INDEX, (void**)&graphics_api->OriginalFunction, graphics_api->HookedFunction);
    if (kiero_is_bound != kiero::Status::Success)
    {
        CoreUtils::ErrorMessageBox( std::string("Failed to hook graphics api \"present\" function. Kiero status: " + std::to_string(kiero_is_bound) + ". (See Kiero github or source for more info)\n").c_str());
        return EXIT_FAILURE;
    }

    CoreUtils::InfoMessageBox("UiForge Started!");
    return EXIT_SUCCESS;
}

void OnGraphicsApiInvoke(void* params)
{
    if(needs_cleanup)
    {
        CleanupUiForge();
        return;
    }

    if (!graphics_api->initialized)
    {
        try
        {
            graphics_api->InitializeGraphicsApi(params);
            ui_manager = new UiManager(graphics_api->target_window);

            // Make context into lua global so scripts can access it
            lua_pushlightuserdata(script_manager->uif_lua_state, ui_manager->mod_context_);
            lua_setglobal(script_manager->uif_lua_state, "ModContext");

            if(!graphics_api->InitializeImGuiImpl())
            {
                throw std::runtime_error("Failed to initialize Graphics API ImGui Implementation.");
            }
        }
        catch (const std::exception& err)
        {
            CoreUtils::ErrorMessageBox(err.what());
            CleanupUiForge();
            return;
        }

        graphics_api->initialized = true;
    }

    graphics_api->NewFrame();
    ui_manager->RenderUiElements(*script_manager);
    graphics_api->Render();

    CoreUtils::ProcessCustomInputs();  // Put this here so it will return straight into calling the original Graphics API function
    return;
}

void CleanupUiForge()
{
    if(kiero_is_initialized == kiero::Status::Success)
    {
        kiero::shutdown();
        kiero_is_initialized = kiero::Status::NotInitializedError;
        kiero_is_bound = kiero::Status::UnknownError;
    }

    // Everything blows up if this is not in a separate thread -- but I'm not
    // entirely convinced this won't introduce some form of race condition
    if(core_module_handle)
    {
        std::thread([]()
        { 
            if(script_manager)
            {
                delete script_manager;
                script_manager = nullptr;
            }

            if(graphics_api) graphics_api->ShutdownImGuiImpl();

            if(ui_manager)
            {
                delete ui_manager;
                ui_manager = nullptr;
            }

            if(graphics_api)
            {
                delete graphics_api;
                graphics_api = nullptr;
            } 
            
        }).detach();
    }

    CoreUtils::InfoMessageBox("UiForge Cleaned Up!");
    FreeLibrary(core_module_handle);
    return;
}