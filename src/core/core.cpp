/**
 * @file uif_core.cpp
 * @version 0.3.2
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
 * @date    2025-01-02 (version 0.3.2)
 *          2025-01-01 (version 0.3.1)
 *          2024-12-20 (version 0.3.0)
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
#include "luajit\lua.hpp"
#include "imgui\sol_ImGui.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                             Forward Declarations                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved);
DWORD WINAPI CoreMain(LPVOID unused_param);
void OnGraphicsApiInvoke(void* params);
void LoadConfiguration();
void LogConfigValues();
void InitializeLua();
void InitializeUiForgeLuaBindings(sol::state_view lua);
void InitializeUiForgeLuaGlobalVariables(sol::table uiforge_table, sol::state_view lua);
void InitializeGraphicsApiLuaBindings(sol::table uiforge_table, sol::state_view lua);
void CleanupUiForge();

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                              Global Variables                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// For Kiero
static const unsigned D3D11_PRESENT_FUNCTION_INDEX  = 8;
kiero::Status::Enum kiero_is_initialized = kiero::Status::NotInitializedError;  // Default value so we know kiero is not initialized
kiero::Status::Enum kiero_is_bound = kiero::Status::UnknownError;               // Default value so we know kiero is not bound

IGraphicsApi* graphics_api;
UiManager* ui_manager;
ForgeScriptManager* script_manager;

// Logging
int max_log_size = 0;
int max_log_files = 0;
std::string log_file_name;
plog::Severity logging_level;

// For Lua
lua_State* uif_lua_state = nullptr;
std::string uiforge_root_dir;
std::string uiforge_scripts_dir;
std::string uiforge_modules_dir;
std::string uiforge_resources_dir;

// For cleanup
std::atomic<HMODULE> core_module_handle(NULL);  // I actually don't think this needs to be atomic?
std::atomic<bool> needs_cleanup(false);

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                               Main Functions                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

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
            HANDLE injected_main_thread = CreateThread(NULL, 0, CoreMain, NULL, 0, NULL);   
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
        LoadConfiguration();

        // Initialize logging
        static plog::RollingFileAppender<plog::TxtFormatter> file_appender(log_file_name.c_str(), max_log_size, max_log_files);
        plog::init(logging_level, &file_appender);
        PLOG_INFO << "Logging initialized";

        LogConfigValues();  // Logging config values here because I have to wait until logging is initialized before I can!
    }
    catch(const std::exception& err)
    {
        PLOG_FATAL << err.what();        
        CoreUtils::ErrorMessageBox(err.what());
        return EXIT_FAILURE;
    }
    
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
    graphics_api = new D3D11GraphicsApi(OnGraphicsApiInvoke);

    try
    {
        InitializeLua();
    }
    catch(const std::exception& err)
    {
        PLOG_FATAL << err.what();        
        CoreUtils::ErrorMessageBox(err.what());
        return EXIT_FAILURE;
    }

    // Hook the graphics API
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

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                             Helper Functions                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void LoadConfiguration()
{
    char path_to_dll[MAX_PATH];
    if(!GetModuleFileNameA(core_module_handle, path_to_dll, MAX_PATH))
    {
        throw std::runtime_error(std::string("Failed to get module file name. Error: " + GetLastError()));
    }
    uiforge_root_dir = std::filesystem::path(path_to_dll).parent_path().parent_path().string();

    uiforge_scripts_dir = std::string(uiforge_root_dir + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "FORGE_SCRIPT_DIR"));

    uiforge_modules_dir = std::string(uiforge_scripts_dir + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "FORGE_MODULES_DIR"));

    uiforge_resources_dir = std::string(uiforge_scripts_dir + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "FORGE_RESOURCES_DIR"));

    max_log_size = GET_CONFIG_VAL(uiforge_root_dir, int, "MAX_LOG_SIZE_BYTES");

    max_log_files = GET_CONFIG_VAL(uiforge_root_dir, int, "MAX_LOG_FILES");

    log_file_name = uiforge_root_dir + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "LOG_FILE_NAME");

    logging_level  = static_cast<plog::Severity>(GET_CONFIG_VAL(uiforge_root_dir, int, "LOGGING_LEVEL"));
}

void LogConfigValues()
{
    PLOG_DEBUG << "UiForge root directory: " << uiforge_root_dir;
    PLOG_DEBUG << "UiForge scripts directory: " << uiforge_scripts_dir;
    PLOG_DEBUG << "UiForge modules directory: " << uiforge_modules_dir;
    PLOG_DEBUG << "UiForge resources directory: " << uiforge_resources_dir;
    PLOG_DEBUG << "Max log size: " << max_log_size;
    PLOG_DEBUG << "Max log files: " << max_log_files;
    PLOG_DEBUG << "Log file name: " << log_file_name;
    PLOG_DEBUG << "Logging level: " << static_cast<int>(logging_level);
}

void InitializeLua()
{
    // Lua state
    uif_lua_state = lua_open();
    if(!uif_lua_state)
    {
        throw std::runtime_error("Failed to create a new lua state.");
    }

    luaL_openlibs(uif_lua_state);
    
    // Need the sol::state_view to initialize the sol bindings
    sol::state_view uif_sol_state_view(uif_lua_state);
    
    // Initialize UiForge specific Lua bindings
    InitializeUiForgeLuaBindings(uif_sol_state_view);

    // Initialize ImGui Lua bindings
    sol_ImGui::Init(uif_sol_state_view);

    script_manager = new ForgeScriptManager(uiforge_scripts_dir, uif_lua_state);
    PLOG_DEBUG << "ForgeScriptManager created";
}

void InitializeUiForgeLuaBindings(sol::state_view lua)
{
    PLOG_INFO << "[+] Initializing UiForge Lua Bindings";
    sol::table uiforge_table = lua.create_named_table("UiForge");

    InitializeUiForgeLuaGlobalVariables(uiforge_table, lua);
    
    // Extend the Lua package path to include the modules directory and its subdirectories (one level deep).
    // This allows Lua to find and load scripts/modules from the specified paths when using `require`.
    //
    // The new paths added are:
    // - modules_path\\?.lua: Looks for scripts directly in the modules directory.
    // - modules_path\\?\\?.lua: Allows for nested modules one level deep, e.g., modules\subdir\module.lua.
    //
    // Example:
    // Given modules_path = "scripts\modules":
    // - require("example") will search for:
    //   - scripts\modules\example.lua
    //   - scripts\modules\example\init.lua (if Lua supports init modules)
    //
    // - require("subdir.module") will search for:
    //   - scripts\modules\subdir\module.lua
    //   - scripts\modules\subdir\module\init.lua
    lua["package"]["path"] = lua["package"]["path"].get<std::string>() + ";" + uiforge_modules_dir + "\\?.lua;" + uiforge_modules_dir + "\\?\\?.lua";

    // Initialize Graphics API bindings
    InitializeGraphicsApiLuaBindings(uiforge_table, lua);
}

void InitializeUiForgeLuaGlobalVariables(sol::table uiforge_table, sol::state_view lua)
{
    PLOG_DEBUG << "[+] Setting up UiForge Lua Globals";
    uiforge_table["scripts_path"] = uiforge_scripts_dir;
    PLOG_DEBUG << "[+] scripts_path: " << uiforge_table["scripts_path"].get<std::string>();
    uiforge_table["modules_path"] = uiforge_modules_dir;
    PLOG_DEBUG << "[+] modules_path: " << uiforge_table["modules_path"].get<std::string>();
    uiforge_table["resources_path"] = uiforge_resources_dir;
    PLOG_DEBUG << "[+] resources_path: " << uiforge_table["resources_path"].get<std::string>();
}

void InitializeGraphicsApiLuaBindings(sol::table uiforge_table, sol::state_view lua)
{
    PLOG_DEBUG << "[+] Initializing graphics api Lua bindings";
    sol::usertype<IGraphicsApi> graphics_api_type = lua.new_usertype<IGraphicsApi>( "IGraphicsApi",
        sol::no_constructor,
        "CreateTextureFromFile", IGraphicsApi::CreateTextureFromFile
    );

    uiforge_table["IGraphicsApi"] = graphics_api_type;
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

    if(core_module_handle)
    {
        // std::thread([]()
        // {
            // PLOG_DEBUG << "Clean up thread created...";
            if(uif_lua_state)
            {
                PLOG_DEBUG << "Closing Lua State...";
                lua_close(uif_lua_state);

                PLOG_DEBUG << "Setting state to nullptr...";
                uif_lua_state = nullptr;
            }

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
                PLOG_INFO << "Cleaning up UI Manager...";
                delete ui_manager;
                ui_manager = nullptr;
            }

            if(graphics_api)
            {
                PLOG_INFO << "Cleaning up graphics api...";
                delete graphics_api;
                graphics_api = nullptr;
            }
            // PLOG_DEBUG << "Closing clean up thread...";
            
        // }).detach();

        // cleanup_thread.join();
        char* cleanup_msg = "UiForge Cleaned Up!";
        PLOG_DEBUG << cleanup_msg;
        CoreUtils::InfoMessageBox(cleanup_msg);
        FreeLibrary(core_module_handle);
    }


    return;
}