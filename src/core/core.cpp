/**
 * @file uif_core.cpp
 * @version 0.3.0
 * @brief DLL for injecting into a target process to hook graphics API and display ImGUI windows.
 * 
 * This file defines a dynamic-link library (DLL) that is designed to be injected into a target 
 * process. Upon injection, the DLL will:
 * 
 * 1. Load all scripts located in the scripts directory.
 * 2. Utilize the Kiero library to hook the chosen graphics API (e.g., DirectX, Vulkan, etc.).
 * 3. Initialize the Dear ImGUI framework to create and manage graphical user interface (GUI) windows.
 * 4. Execute scripts, displaying custom imgui windows or performing other tasks.
 *
 *
 * @note    Ensure that the target process is compatible with the graphics API being hooked.
 * 
 * @warning You use this module at your own risk. You are responsible for how you use this code.
 *          Be careful about what lua scripts you throw into the scripts\modules directory. This code will
 *          load ANY scripts in there, including any that may contain malicious code. Only use
 *          modules from trusted sources, and only those that you KNOW are not malicious.
 * 
 * @author  mmvest (wereox)
 * @date    2024-12-20 (version 0.3.0)
 *          2024-12-13 (version 0.2.5)
 *          2024-12-11 (version 0.2.4)
 *          2024-11-15 (version 0.2.3)
 *          2024-11-12 (version 0.2.2)
 *          2024-10-28 (version 0.2.1)
 *          2024-10-02 (version 0.2.0)
 *          2024-10-02 (version 0.1.1)
 *          2024-09-25 (version 0.1.0)
 */



#include <Windows.h>
#include <stdexcept>
#include <filesystem>
#include <string>
#include <thread>
#include <atomic>
#include "kiero\kiero.h"
#include "core\graphics_api.h"
#include "core\util.h"
#include "core\forgescript_manager.h"
#include "scl\SCL.hpp"
#include "plog\Log.h"
#include "plog\Initializers\RollingFileInitializer.h"
#include "plog\Appenders\ConsoleAppender.h"

// *********************************
// * Function Forward Declarations *
// *********************************
BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved);
DWORD WINAPI CoreMain(LPVOID unused_param);
void OnGraphicsApiInvoke(void* params);
void CleanupUiForge();
void SetupLuaGlobals();

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

std::string uiforge_root_dir;

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
                std::string err_msg("UiForge failed to start properly. Error: " + std::to_string(GetLastError()));
                CoreUtils::ErrorMessageBox(err_msg.c_str());
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

    try
    {
        uiforge_root_dir = CoreUtils::GetUiForgeRootDirectory();

        // Initialize logging
        int max_log_size 			= GET_CONFIG_VAL(uiforge_root_dir, int, "MAX_LOG_SIZE_BYTES");
        int max_log_files 			= GET_CONFIG_VAL(uiforge_root_dir, int, "MAX_LOG_FILES");
        std::string log_file_name 	= uiforge_root_dir + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "LOG_FILE_NAME");
        plog::Severity logging_level= static_cast<plog::Severity>(GET_CONFIG_VAL(uiforge_root_dir, int, "LOGGING_LEVEL"));
        static plog::RollingFileAppender<plog::TxtFormatter> file_appender(log_file_name.c_str(), max_log_size, max_log_files);
        plog::init(logging_level, &file_appender);
    }
    catch(const std::exception& err)
    {
        PLOG_FATAL << err.what();        
        CoreUtils::ErrorMessageBox(err.what());
        return EXIT_FAILURE;
    }
    

    PLOG_INFO << "Logging initialized";
    
    PLOG_INFO << "Initializing Core...";
    PLOG_DEBUG << "Initializing kiero...";
    kiero_is_initialized = kiero::init(kiero::RenderType::D3D11);
    if (kiero_is_initialized != kiero::Status::Success)
    {
        std::string err_msg("Failed to initialize graphics api hooking functionality. Kiero status: " + std::to_string(kiero_is_initialized) + ". (See Kiero github or source for more info)");
        PLOG_FATAL << err_msg;
        CoreUtils::ErrorMessageBox(err_msg.c_str());
        return EXIT_FAILURE;
    }

    // Based on configuration, choose the graphics api. For now, its just D3D11.
    // TODO: Maybe add a switch statement? Something here to choose which Graphics API
    PLOG_DEBUG << "Creating GraphicsAPI object";
    graphics_api = new D3D11GraphicsApi();
    graphics_api->OnGraphicsApiInvoke = OnGraphicsApiInvoke;    // We could probably move this to the constructor

    // Load all mods
    try
    {
        // Set the script directory that the script manager will use
        std::string uiforge_script_dir = std::string(uiforge_root_dir + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "FORGE_SCRIPT_DIR"));
        PLOG_DEBUG << "UiForge script directory: " << uiforge_script_dir;
        
        script_manager = new ForgeScriptManager(uiforge_script_dir);
        PLOG_DEBUG << "ForgeScriptManager created";
    }
    catch(const std::exception& err)
    {
        PLOG_FATAL << err.what();        
        CoreUtils::ErrorMessageBox(err.what());
        return EXIT_FAILURE;
    }

    kiero_is_bound = kiero::bind(D3D11_PRESENT_FUNCTION_INDEX, (void**)&graphics_api->OriginalFunction, graphics_api->HookedFunction);
    if (kiero_is_bound != kiero::Status::Success)
    {
        std::string err_msg("Failed to hook graphics api \"present\" function. Kiero status: " + std::to_string(kiero_is_bound) + ". (See Kiero github or source for more info)\n");
        PLOG_FATAL << err_msg;
        CoreUtils::ErrorMessageBox(err_msg.c_str());
        return EXIT_FAILURE;
    }

    PLOG_INFO << "Core initialized!";
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
            PLOG_DEBUG << "Initializing GraphicsAPI object";
            graphics_api->InitializeGraphicsApi(params);

            PLOG_DEBUG << "Creating UiManager";
            ui_manager = new UiManager(graphics_api->target_window);
            if(!ui_manager)
            {
                throw std::runtime_error("Failed to initialize Graphics API ImGui Implementation.");
            }

            PLOG_DEBUG << "Initializing ImGuiImpl";
            if(!graphics_api->InitializeImGuiImpl())
            {
                throw std::runtime_error("Failed to initialize Graphics API ImGui Implementation.");
            }
             PLOG_DEBUG << "Done with graphics initialization";
        }
        catch (const std::exception& err)
        {
            PLOG_FATAL << err.what();
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
    PLOG_INFO << "Cleaning up UiForge...";
    if(kiero_is_initialized == kiero::Status::Success)
    {
        PLOG_INFO << "Shutting down Kiero...";
        kiero::shutdown();
        kiero_is_initialized = kiero::Status::NotInitializedError;
        kiero_is_bound = kiero::Status::UnknownError;
    }

    // Everything blows up if this is not in a separate thread -- but I'm not
    // entirely convinced this won't introduce some form of race condition
    if(core_module_handle)
    {
        // std::thread([]()
        // {
            PLOG_DEBUG << "Clean up thread created...";

            if(script_manager)
            {
                PLOG_INFO << "Cleaning up script manager...";
                delete script_manager;
                script_manager = nullptr;
            }

            if(graphics_api)
            {
                PLOG_INFO << "Shutting down imgui graphics api implementation...";
                graphics_api->ShutdownImGuiImpl();
            }

            if(ui_manager)
            {
                PLOG_INFO << "Cleaning up UI Manager";
                delete ui_manager;
                ui_manager = nullptr;
            }

            if(graphics_api)
            {
                PLOG_INFO << "Cleaning up graphics api...";
                delete graphics_api;
                graphics_api = nullptr;
            }
            PLOG_DEBUG << "Closing clean up thread...";
            
        // }).detach();

        // cleanup_thread.join();
        char* cleanup_msg = "UiForge Cleaned Up!";
        PLOG_DEBUG << cleanup_msg;
        CoreUtils::InfoMessageBox(cleanup_msg);
        FreeLibrary(core_module_handle);
    }


    return;
}