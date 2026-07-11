/**
 * @file core.cpp
 * @version 1.0.0
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
 * @date    2026-07-09 (version 1.0.0)
 *
 *          2025-01-13 (version 0.3.3)
 * 
 *          2025-01-02 (version 0.3.2)
 *  
 *          2025-01-01 (version 0.3.1)
 *  
 *          2024-12-20 (version 0.3.0) 
 * 
 *          2024-12-13 (version 0.2.5) 
 * 
 *          2024-12-11 (version 0.2.4) 
 * 
 *          2024-11-15 (version 0.2.3) 
 * 
 *          2024-11-12 (version 0.2.2) 
 * 
 *          2024-10-28 (version 0.2.1) 
 * 
 *          2024-10-02 (version 0.2.0) 
 * 
 *          2024-10-02 (version 0.1.1) 
 * 
 *          2024-09-25 (version 0.1.0)
 */


#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <unknwn.h>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <sol_ImGui.h>
#include <kiero.h>
#include <lua.hpp>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Log.h>
#include <SCL/SCL.hpp>
#include <sol/sol.hpp>

#include "core\util.h"
#include "core\audio_manager.h"
#include "core\graphics_api.h"
#include "core\forgescript_manager.h"
#include "core\serpent.h"
#include "core\ui_manager.h"

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
void InitializeUiForgeLuaGlobalVariables(sol::table uiforge_table);
void InitializeGraphicsApiLuaBindings(sol::table uiforge_table, sol::state_view lua);
void CleanupUiForge();

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                              Global Variables                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define CONFIG_FILE "config"
#define GET_CONFIG_VAL(root_dir, val_type, val_name) scl::config_file(std::string(root_dir + "\\" + CONFIG_FILE), scl::config_file::READ).get<val_type>(val_name)

// For Kiero
static const unsigned D3D11_PRESENT_FUNCTION_INDEX                = 8;
static const unsigned D3D12_PRESENT_FUNCTION_INDEX                = 140;
static const unsigned D3D12_EXECUTE_COMMAND_LISTS_FUNCTION_INDEX  = 54;
kiero::Status::Enum kiero_is_initialized = kiero::Status::NotInitializedError;  // Default value so we know kiero is not initialized
kiero::Status::Enum kiero_is_bound = kiero::Status::UnknownError;               // Default value so we know kiero is not bound

IGraphicsApi* graphics_api;
UiManager* ui_manager;
ForgeScriptManager* script_manager;
bool imgui_impl_initialized = false;    // True only once the graphics-API ImGui backend has been initialized

// Settings
std::string config_parent_dir;
void* settings_icon = nullptr;
std::string settings_icon_file;
float settings_icon_size_x = 32.0f;
float settings_icon_size_y = 32.0f;

// Logging
int max_log_size = 0;
int max_log_files = 0;
std::string log_file_name;
plog::Severity logging_level;

// Graphics API selection ("auto", "d3d11", or "d3d12")
std::string graphics_api_name = "auto";

// Script reloading
int reload_on_save = 0;
int reload_on_save_poll_ms = 2500;

// For Lua
lua_State* uif_lua_state = nullptr;
std::string uiforge_root_dir;
std::string uiforge_scripts_dir;
std::string uiforge_modules_dir;
std::string uiforge_resources_dir;
std::string uiforge_profiles_dir;

// Fonts loaded through UiForge.LoadFont, keyed by "<resolved path>|<size>" so repeat
// loads (multiple mods, per-frame settings callbacks) reuse the same ImFont.
static std::unordered_map<std::string, ImFont*> loaded_fonts;

// For cleanup
std::atomic<HMODULE> core_module_handle(NULL);  // I actually don't think this needs to be atomic?
std::atomic<bool> needs_cleanup(false);

/**
 * @brief IM_ASSERT handler installed through compat\uiforge_imconfig.h.
 *
 * Logs the failed assertion and throws so the failure unwinds to the per-script
 * or per-frame catch instead of aborting the host process.
 */
[[noreturn]] void UiForgeImGuiAssertFail(const char* expression, const char* file, int line)
{
    std::string message = std::string("ImGui assertion failed: (") + expression + ") at " + file + ":" + std::to_string(line);
    PLOG_ERROR << message;
    throw std::runtime_error(message);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                               Main Functions                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief Entry point for DLL, handling process attach/detach and creating a thread that will run the main logic.
 *
 * @param[in] h_module   Handle to the loaded module (DLL)
 * @param[in] ul_reason_for_call  Reason for calling DllMain (attach or detach)
 * @param[in] reserved Unused parameter (reserved for future use)
 *
 * @return TRUE
 */
BOOL APIENTRY DllMain( HMODULE h_module, DWORD  ul_reason_for_call, LPVOID reserved)
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
                CoreUtils::ErrorMessageBox(std::move(err_msg));
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

/**
 * @brief Initializes graphics API hooking functionality using kiero library.
 *         Binds D3D11 present function and sets up necessary hooks for further processing.
 *
 * @param unused_param [in] Unused parameter required by WINAPI convention (not used in this implementation)
 *
 * @return Exit status code, indicating success or failure
 */
DWORD WINAPI CoreMain(LPVOID unused_param)
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
        CoreUtils::ErrorMessageBox(err.what());
        return EXIT_FAILURE;
    }
    
    PLOG_INFO << "Initializing Core...";

    // Resolve which graphics API to hook. If the config says "auto" (or the key is missing) we detect it
    // by checking which runtime DLLs the target process has loaded. D3D12 is checked before D3D11 because
    // D3D12 applications very commonly also load d3d11.dll (e.g. via D3D11On12 or media components), while
    // pure D3D11 applications rarely load d3d12.dll.
    kiero::RenderType::Enum render_type = kiero::RenderType::None;
    if (graphics_api_name == "auto")
    {
        if (GetModuleHandleA("d3d12.dll"))
        {
            render_type = kiero::RenderType::D3D12;
        }
        else if (GetModuleHandleA("d3d11.dll"))
        {
            render_type = kiero::RenderType::D3D11;
        }
        else
        {
            std::string err_msg("Failed to auto-detect the target process graphics API (no d3d11.dll or d3d12.dll loaded). Set GRAPHICS_API in the config to choose one explicitly.");
            PLOG_FATAL << err_msg;
            CoreUtils::ErrorMessageBox(std::move(err_msg));
            return EXIT_FAILURE;
        }
        PLOG_INFO << "Auto-detected graphics API: " << (render_type == kiero::RenderType::D3D12 ? "d3d12" : "d3d11");
    }
    else if (graphics_api_name == "d3d11")
    {
        render_type = kiero::RenderType::D3D11;
    }
    else if (graphics_api_name == "d3d12")
    {
        render_type = kiero::RenderType::D3D12;
    }
    else
    {
        std::string err_msg("Unsupported GRAPHICS_API value \"" + graphics_api_name + "\". Must be one of: auto, d3d11, d3d12.");
        PLOG_FATAL << err_msg;
        CoreUtils::ErrorMessageBox(std::move(err_msg));
        return EXIT_FAILURE;
    }

    PLOG_DEBUG << "Initializing kiero...";
    kiero_is_initialized = kiero::init(render_type);
    if (kiero_is_initialized != kiero::Status::Success)
    {
        std::string err_msg("Failed to initialize graphics api hooking functionality. Kiero status: " + std::to_string(kiero_is_initialized) + ". (See Kiero github or source for more info)");
        PLOG_FATAL << err_msg;
        CoreUtils::ErrorMessageBox(std::move(err_msg));
        return EXIT_FAILURE;
    }

    PLOG_DEBUG << "Creating GraphicsAPI object";
    if (render_type == kiero::RenderType::D3D12)
    {
        graphics_api = new D3D12GraphicsApi(OnGraphicsApiInvoke);
    }
    else
    {
        graphics_api = new D3D11GraphicsApi(OnGraphicsApiInvoke);
    }

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

    // Hook the graphics API. D3D12 needs an extra hook on ExecuteCommandLists so we can capture the
    // application's command queue (the swap chain alone cannot provide it), and it must be bound before
    // Present so the queue is available by the time our Present hook first runs.
    if (render_type == kiero::RenderType::D3D12)
    {
        kiero_is_bound = kiero::bind(D3D12_EXECUTE_COMMAND_LISTS_FUNCTION_INDEX, (void**)&D3D12GraphicsApi::OriginalExecuteCommandLists, (void*)D3D12GraphicsApi::HookedExecuteCommandLists);
        if (kiero_is_bound != kiero::Status::Success)
        {
            std::string err_msg("Failed to hook graphics api \"ExecuteCommandLists\" function. Kiero status: " + std::to_string(kiero_is_bound) + ". (See Kiero github or source for more info)\n");
            PLOG_FATAL << err_msg;
            CoreUtils::ErrorMessageBox(std::move(err_msg));
            return EXIT_FAILURE;
        }
    }

    unsigned present_index = (render_type == kiero::RenderType::D3D12) ? D3D12_PRESENT_FUNCTION_INDEX : D3D11_PRESENT_FUNCTION_INDEX;
    kiero_is_bound = kiero::bind(present_index, (void**)&graphics_api->OriginalFunction, graphics_api->HookedFunction);
    if (kiero_is_bound != kiero::Status::Success)
    {
        std::string err_msg("Failed to hook graphics api \"present\" function. Kiero status: " + std::to_string(kiero_is_bound) + ". (See Kiero github or source for more info)\n");
        PLOG_FATAL << err_msg;
        CoreUtils::ErrorMessageBox(std::move(err_msg));
        return EXIT_FAILURE;
    }

    PLOG_INFO << "Core initialized!";
    return EXIT_SUCCESS;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                               UI Functions                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief This is the replacement function for whichever graphics API function is hooked.
 *
 * This function is called whenever the graphics API's primary function (e.g., `Present` for D3D11) is invoked. It
 * ensures that the graphics API is initialized, sets up ImGui and the UI manager, and handles rendering the UI
 * elements. If cleanup is required, it terminates UiForge and cleans up resources.
 *
 * @param params A pointer to the parameters required by the graphics API. For example, in D3D11, this would typically
 * be a pointer to an `IDXGISwapChain`.
 *
 * @throws std::runtime_error If any step in the initialization process fails (e.g., UI manager creation or ImGui
 * initialization).
 */
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
            ui_manager = new UiManager(graphics_api->target_window, settings_icon_size_x, settings_icon_size_y);
            if(!ui_manager)
            {
                throw std::runtime_error("Failed to initialize Graphics API ImGui Implementation.");
            }

            PLOG_DEBUG << "Initializing ImGuiImpl";
            if(!graphics_api->InitializeImGuiImpl())
            {
                throw std::runtime_error("Failed to initialize Graphics API ImGui Implementation.");
            }
            imgui_impl_initialized = true;

            if (!settings_icon_file.empty())
            {
                PLOG_DEBUG << "Loading Settings Icon Texture";
                const std::filesystem::path icon_path = std::filesystem::path(uiforge_resources_dir) / settings_icon_file;
                settings_icon = graphics_api->CreateTextureFromFile(icon_path.wstring());
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

    // Handle the case where the screen changes from window to fullscreen and vice versa
    // and any other render target changes.
    graphics_api->UpdateRenderTarget(params);
    ui_manager->UpdateTargetWindow(graphics_api->target_window);

    // A thrown IM_ASSERT (see UiForgeImGuiAssertFail) can abort the frame anywhere.
    // Catch it here, close out the ImGui frame, and skip rendering rather than
    // letting the exception escape into the host's Present call.
    try
    {
        graphics_api->NewFrame();
        ui_manager->RenderUiElements(*script_manager, settings_icon);
        graphics_api->Render();
    }
    catch (const std::exception& err)
    {
        PLOG_ERROR << "Frame aborted: " << err.what();
        try
        {
            if (ImGui::GetCurrentContext() && ImGui::GetFrameCount() > 0)
            {
                ImGui::EndFrame();
            }
        }
        catch (...) {}
    }

    CoreUtils::ProcessCustomInputs(graphics_api->target_window);  // Put this here so it will return straight into calling the original Graphics API function
    return;
}



// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                             Helper Functions                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief Loads the configuration settings for the application.
 *
 * This function initializes critical directories, logging parameters, and 
 * other settings required for the application to function properly.
 *
 * @throws std::runtime_error If the module file name cannot be retrieved or no config file exists.
 */
void LoadConfiguration()
{
    char path_to_dll[MAX_PATH];
    if(!GetModuleFileNameA(core_module_handle, path_to_dll, MAX_PATH))
    {
        throw std::runtime_error("Failed to get module file name. Error: " + std::to_string(GetLastError()));
    }
    uiforge_root_dir = std::filesystem::path(path_to_dll).parent_path().parent_path().string();

    // We need to account for the fact that UiForge may be bundled with other code. If there is a bundled
    // config, we will use that. We determine this by going up one directory level from UiForge and checking
    // if it exists there. If it does not, we default to the UiForge normal config. If there is no
    // config there, then we must error out and cleanup.
    std::string bundled_root_dir = std::filesystem::path(uiforge_root_dir).parent_path().string();
    if(std::filesystem::exists(bundled_root_dir + "\\" + CONFIG_FILE) && 
            std::filesystem::is_regular_file(bundled_root_dir + "\\" + CONFIG_FILE))
    {
        config_parent_dir = bundled_root_dir;
    }
    else if( std::filesystem::exists(uiforge_root_dir + "\\" + CONFIG_FILE) && 
        std::filesystem::is_regular_file(uiforge_root_dir + "\\" + CONFIG_FILE))
    {
        config_parent_dir = uiforge_root_dir;
    }
    else
    {
        throw std::runtime_error(
            "Failed to locate config. Be sure a config file is defined in " +
            uiforge_root_dir +
            " or " +
            bundled_root_dir +
            ".");
    }

    uiforge_scripts_dir = std::string(config_parent_dir + "\\" + GET_CONFIG_VAL(config_parent_dir, std::string, "FORGE_SCRIPT_DIR"));

    uiforge_modules_dir = std::string(uiforge_scripts_dir + "\\" + GET_CONFIG_VAL(config_parent_dir, std::string, "FORGE_MODULES_DIR"));

    uiforge_resources_dir = std::string(uiforge_scripts_dir + "\\" + GET_CONFIG_VAL(config_parent_dir, std::string, "FORGE_RESOURCES_DIR"));

    uiforge_profiles_dir = std::string(uiforge_scripts_dir + "\\profiles");

    reload_on_save = GET_CONFIG_VAL(config_parent_dir, unsigned int, "RELOAD_ON_SAVE");
    reload_on_save_poll_ms = GET_CONFIG_VAL(config_parent_dir, unsigned int, "RELOAD_ON_SAVE_POLL_MS");
    // Guard against a missing/zero/garbage poll interval and fall back to a sane default.
    if (reload_on_save_poll_ms <= 0) reload_on_save_poll_ms = 2500;

    settings_icon_file = GET_CONFIG_VAL(config_parent_dir, std::string, "SETTINGS_ICON_FILE");

    settings_icon_size_x = static_cast<float>(GET_CONFIG_VAL(config_parent_dir, unsigned int, "SETTINGS_ICON_SIZE_X"));
    settings_icon_size_y = static_cast<float>(GET_CONFIG_VAL(config_parent_dir, unsigned int, "SETTINGS_ICON_SIZE_Y"));

    max_log_size = GET_CONFIG_VAL(config_parent_dir, unsigned int, "MAX_LOG_SIZE_BYTES");

    max_log_files = GET_CONFIG_VAL(config_parent_dir, unsigned int, "MAX_LOG_FILES");

    log_file_name = config_parent_dir + "\\" + GET_CONFIG_VAL(config_parent_dir, std::string, "LOG_FILE_NAME");

    logging_level  = static_cast<plog::Severity>(GET_CONFIG_VAL(config_parent_dir, unsigned int, "LOGGING_LEVEL"));

    try
    {
        graphics_api_name = GET_CONFIG_VAL(config_parent_dir, std::string, "GRAPHICS_API");
        std::transform(graphics_api_name.begin(), graphics_api_name.end(), graphics_api_name.begin(),
                       [](unsigned char curr_char) { return static_cast<char>(std::tolower(curr_char)); });
    }
    catch(const std::exception&)
    {
        graphics_api_name = "auto";  // Missing key -- fall back to auto-detection
    }
}

/**
 * @brief Logs the current configuration values for debugging purposes.
 */
void LogConfigValues()
{
    PLOG_DEBUG << "Config Parent Directory: " << config_parent_dir; 
    PLOG_DEBUG << "UiForge root directory: " << uiforge_root_dir;
    PLOG_DEBUG << "UiForge scripts directory: " << uiforge_scripts_dir;
    PLOG_DEBUG << "UiForge modules directory: " << uiforge_modules_dir;
    PLOG_DEBUG << "UiForge resources directory: " << uiforge_resources_dir;
    PLOG_DEBUG << "UiForge profiles directory: " << uiforge_profiles_dir;
    PLOG_DEBUG << "Reload on save: " << reload_on_save;
    PLOG_DEBUG << "Reload on save poll ms: " << reload_on_save_poll_ms;
    PLOG_DEBUG << "Settings icon file: " << settings_icon_file;
    PLOG_DEBUG << "Settings icon size x: " << settings_icon_size_x;
    PLOG_DEBUG << "Settings icon size y: " << settings_icon_size_y;
    PLOG_DEBUG << "Max log size: " << max_log_size;
    PLOG_DEBUG << "Max log files: " << max_log_files;
    PLOG_DEBUG << "Log file name: " << log_file_name;
    PLOG_DEBUG << "Logging level: " << static_cast<int>(logging_level);
}

/**
 * @brief Initializes the Lua state and sets up Lua bindings.
 *
 * This function creates a new Lua state, initializes Lua libraries, and 
 * sets up bindings for UiForge and ImGui. Additionally, it initializes the 
 * script manager for handling scripts in the application.
 *
 * @throws std::runtime_error If the Lua state cannot be created.
 */
void InitializeLua()
{
    // Lua state
    uif_lua_state = lua_open();
    if(!uif_lua_state)
    {
        throw std::runtime_error("Failed to create a new lua state.");
    }

    luaL_openlibs(uif_lua_state);
    
    // The shared modules/resources/profiles directories live inside the scripts
    // directory and must never be mistaken for script packages during discovery.
    script_manager = new ForgeScriptManager(uiforge_scripts_dir, uif_lua_state,
        {
            std::filesystem::path(uiforge_modules_dir).filename().string(),
            std::filesystem::path(uiforge_resources_dir).filename().string(),
            std::filesystem::path(uiforge_profiles_dir).filename().string()
        });
    PLOG_DEBUG << "ForgeScriptManager created";
    script_manager->SetReloadOnSave(reload_on_save != 0, reload_on_save_poll_ms);
    script_manager->SetProfilesDirectory(uiforge_profiles_dir);
    script_manager->SetModulesDirectory(uiforge_modules_dir);

    // Need the sol::state_view to initialize the sol bindings
    sol::state_view uif_sol_state_view(uif_lua_state);

    // Register the embedded serpent serializer so both the core (save/load) and scripts
    // (require("serpent")) can use it.
    uif_sol_state_view.require_script(UiForgeSerpent::serpent_module_name, UiForgeSerpent::serpent_lua_source);
    PLOG_DEBUG << "serpent serializer registered";

    // Initialize UiForge specific Lua bindings
    InitializeUiForgeLuaBindings(uif_sol_state_view);

    // Initialize ImGui Lua bindings
    sol_ImGui::Init(uif_sol_state_view);

    // Auto-apply the preferred profile for this process, if one is configured.
    script_manager->ApplyPreferredProfile();
}


/**
 * @brief Resolves a script-supplied resource path to a full filesystem path.
 *
 * Relative paths are resolved against the currently executing packaged script's own
 * resources folder first (when it exists there), then fall back to the shared
 * resources directory. Absolute paths are returned unchanged.
 *
 * @param path The path as provided by the Lua script.
 * @return The resolved filesystem path.
 */
static std::filesystem::path ResolveResourcePath(const std::string& path)
{
    std::filesystem::path resource_path(path);
    if (!resource_path.is_relative())
    {
        return resource_path;
    }

    if (script_manager)
    {
        ForgeScript* current_script = script_manager->GetCurrentlyExecutingScript();
        if (current_script && !current_script->GetPackageResourcesDir().empty())
        {
            const std::filesystem::path local_path =
                std::filesystem::path(current_script->GetPackageResourcesDir()) / resource_path;
            std::error_code ec;
            if (std::filesystem::exists(local_path, ec))
            {
                return local_path;
            }
        }
    }

    return std::filesystem::path(uiforge_resources_dir) / resource_path;
}

/**
 * @brief Initializes UiForge-specific Lua bindings.
 *
 * Sets up the Lua environment for UiForge, including global variables and 
 * extending the Lua package path to include UiForge's modules directory. 
 * This allows Lua scripts to use `require` to load modules.
 *
 * @param lua The `sol::state_view` representing the current Lua state.
 *
 * @details
 * - Creates a Lua table named "UiForge" and populates it with global variables.
 * - Extends the Lua package path to include:
 *   - `modules_path\\?.lua`: For direct module files.
 *   - `modules_path\\?\\?.lua`: For nested modules one level deep.
 * - Calls `InitializeGraphicsApiLuaBindings` to bind the graphics API.
 *
 * @note This function assumes the `uiforge_modules_dir` and `script_manager` is correctly initialized.
 */
void InitializeUiForgeLuaBindings(sol::state_view lua)
{
    PLOG_INFO << "[+] Initializing UiForge Lua Bindings";
    sol::table uiforge_table = lua.create_named_table("UiForge");

    InitializeUiForgeLuaGlobalVariables(uiforge_table);
    
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

    uiforge_table["LoadTexture"] = [](const std::string& path) -> void*
    {
        if (!IGraphicsApi::CreateTextureFromFile)
        {
            return nullptr;
        }

        return IGraphicsApi::CreateTextureFromFile(ResolveResourcePath(path).wstring());
    };

    // Loads a TTF/OTF font for use with ImGui.PushFont. Relative paths resolve the same
    // way as LoadTexture (package resources folder first, then shared resources). Size is
    // in pixels; pass nothing (or 0) to use ImGui's default size and size it at PushFont
    // time. On any failure the default font is returned so PushFont is always safe.
    lua.new_usertype<ImFont>("ImFont", sol::no_constructor);
    uiforge_table["LoadFont"] = [](const std::string& path, sol::optional<float> size_px) -> ImFont*
    {
        if (!ImGui::GetCurrentContext())
        {
            PLOG_WARNING << "LoadFont called before ImGui was initialized: " << path;
            return nullptr;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImFont* fallback = io.FontDefault ? io.FontDefault
                         : (io.Fonts->Fonts.empty() ? nullptr : io.Fonts->Fonts[0]);

        const float size = (size_px && *size_px > 0.0f) ? *size_px : 0.0f;
        const std::filesystem::path font_path = ResolveResourcePath(path);
        const std::string cache_key = font_path.string() + "|" + std::to_string(size);

        auto cached = loaded_fonts.find(cache_key);
        if (cached != loaded_fonts.end())
        {
            return cached->second;
        }

        std::error_code ec;
        if (!std::filesystem::exists(font_path, ec))
        {
            PLOG_WARNING << "LoadFont could not find \"" << font_path.string() << "\". Falling back to the default font.";
            return fallback;
        }

        // NoLoadError keeps a bad file from tripping ImGui's assert path.
        ImFontConfig font_config;
        font_config.Flags |= ImFontFlags_NoLoadError;
        ImFont* font = io.Fonts->AddFontFromFileTTF(font_path.string().c_str(), size, &font_config);
        if (!font)
        {
            PLOG_WARNING << "LoadFont failed to load \"" << font_path.string() << "\". Falling back to the default font.";
            return fallback;
        }

        PLOG_DEBUG << "Loaded font \"" << font_path.string() << "\" at size " << size;
        loaded_fonts[cache_key] = font;
        return font;
    };

    // Creates a texture from raw 32-bit RGBA pixel bytes (row-major, no row padding).
    // Accepts the pixels as a Lua string so FFI buffers can be passed via ffi.string(buf, len).
    // Alpha is standard 0-255. Returns a texture handle usable with ImGui.Image, or nil on failure.
    // Release with UiForge.ReleaseTexture when no longer needed.
    uiforge_table["CreateTextureFromMemory"] = [](const std::string& rgba_pixels, int width, int height) -> void*
    {
        if (!IGraphicsApi::CreateTextureFromMemory)
        {
            return nullptr;
        }

        if (width <= 0 || height <= 0
            || rgba_pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4)
        {
            PLOG_WARNING << "CreateTextureFromMemory: pixel buffer size " << rgba_pixels.size()
                         << " does not match " << width << "x" << height << " RGBA (expected "
                         << static_cast<size_t>(width) * static_cast<size_t>(height) * 4 << " bytes).";
            return nullptr;
        }

        return IGraphicsApi::CreateTextureFromMemory(rgba_pixels.data(), width, height);
    };

    uiforge_table["ReleaseTexture"] = [](void* texture)
    {
        if (!texture || !IGraphicsApi::ReleaseTexture)
        {
            return;
        }
        IGraphicsApi::ReleaseTexture(texture);
    };

    // Loads a sound file (mp3 or wav) for playback. Relative paths resolve the same way
    // as LoadTexture/LoadFont. Returns a sound handle, or nil when the file is missing
    // or cannot be opened. Repeat loads of the same file return the same handle.
    uiforge_table["LoadSound"] = [](const std::string& path) -> sol::optional<int>
    {
        const std::filesystem::path sound_path = ResolveResourcePath(path);

        std::error_code ec;
        if (!std::filesystem::exists(sound_path, ec))
        {
            PLOG_WARNING << "LoadSound could not find \"" << sound_path.string() << "\".";
            return sol::nullopt;
        }

        const int sound_id = AudioManager::Load(sound_path.wstring());
        if (sound_id == 0)
        {
            return sol::nullopt;
        }
        return sound_id;
    };

    // Plays a loaded sound from the beginning. Options table supports volume (0.0 to 1.0,
    // default 1.0) and loop (default false). All sound functions are nil-safe no-ops when
    // given a nil handle, so a failed LoadSound never breaks a script.
    uiforge_table["PlaySound"] = [](sol::optional<int> sound_id, sol::optional<sol::table> options) -> bool
    {
        if (!sound_id)
        {
            return false;
        }

        double volume = 1.0;
        bool loop = false;
        if (options)
        {
            volume = options->get_or("volume", 1.0);
            loop = options->get_or("loop", false);
        }
        return AudioManager::Play(*sound_id, volume, loop);
    };

    uiforge_table["StopSound"] = [](sol::optional<int> sound_id) -> bool
    {
        return sound_id ? AudioManager::Stop(*sound_id) : false;
    };

    uiforge_table["IsSoundPlaying"] = [](sol::optional<int> sound_id) -> bool
    {
        return sound_id ? AudioManager::IsPlaying(*sound_id) : false;
    };

    uiforge_table["SetSoundVolume"] = [](sol::optional<int> sound_id, double volume) -> bool
    {
        return sound_id ? AudioManager::SetVolume(*sound_id, volume) : false;
    };

    uiforge_table["ReleaseSound"] = [](sol::optional<int> sound_id)
    {
        if (sound_id)
        {
            AudioManager::Release(*sound_id);
        }
    };

    // ForgeScriptManager Bindings
    sol::table callback_type_table = lua.create_table();
    callback_type_table["Settings"] = static_cast<int>(ForgeScriptCallbackType::Settings);
    callback_type_table["DisableScript"] = static_cast<int>(ForgeScriptCallbackType::DisableScript);
    callback_type_table["Save"] = static_cast<int>(ForgeScriptCallbackType::Save);
    callback_type_table["Load"] = static_cast<int>(ForgeScriptCallbackType::Load);
    callback_type_table["OnEject"] = static_cast<int>(ForgeScriptCallbackType::OnEject);
    uiforge_table["CallbackType"] = callback_type_table;

    uiforge_table["RegisterCallback"] = [](int callback_type, sol::protected_function callback)
    {
        script_manager->RegisterCallback(static_cast<ForgeScriptCallbackType>(callback_type), callback);
    };
}

/**
 * @brief Sets up global variables in the UiForge Lua table.
 *
 * Populates the "UiForge" Lua table with global paths that can be accessed 
 * by Lua scripts. These include paths to scripts, modules, and resources.
 *
 * @param uiforge_table The Lua table representing the "UiForge" namespace.
 */
void InitializeUiForgeLuaGlobalVariables(sol::table uiforge_table)
{
    PLOG_DEBUG << "[+] Setting up UiForge Lua Globals";
    uiforge_table["scripts_path"] = uiforge_scripts_dir;
    PLOG_DEBUG << "[+] scripts_path: " << uiforge_table["scripts_path"].get<std::string>();
    uiforge_table["modules_path"] = uiforge_modules_dir;
    PLOG_DEBUG << "[+] modules_path: " << uiforge_table["modules_path"].get<std::string>();
    uiforge_table["resources_path"] = uiforge_resources_dir;
    PLOG_DEBUG << "[+] resources_path: " << uiforge_table["resources_path"].get<std::string>();
    uiforge_table["profiles_path"] = uiforge_profiles_dir;
    PLOG_DEBUG << "[+] profiles_path: " << uiforge_table["profiles_path"].get<std::string>();
}

/**
 * @brief Initializes Lua bindings for the graphics API.
 *
 * Creates a Lua usertype for the `IGraphicsApi` interface and exposes its 
 * methods to Lua scripts. The bindings are added to the "UiForge" Lua table.
 *
 * @param uiforge_table The Lua table representing the "UiForge" namespace.
 * @param lua The `sol::state_view` representing the current Lua state.
 */
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
        
        if(script_manager)
        {
            // Give scripts a last chance to clean up while the Lua state is still alive.
            PLOG_INFO << "Running on-eject callbacks...";
            script_manager->RunOnEjectCallbacks();

            PLOG_INFO << "Cleaning up script manager...";
            delete script_manager;
            script_manager = nullptr;
        }

        // MUST COME AFTER SCRIPT MANAGER -- ForgeScripts reference lua state during destruction of sol::protected_function
        if(uif_lua_state)
        {
            PLOG_DEBUG << "Closing Lua State...";
            lua_close(uif_lua_state);

            PLOG_DEBUG << "Setting state to nullptr...";
            uif_lua_state = nullptr;
        }

        // Font pointers are owned by the ImGui context and die with it below.
        loaded_fonts.clear();

        PLOG_INFO << "Releasing sounds...";
        AudioManager::ReleaseAll();

        if (settings_icon && IGraphicsApi::ReleaseTexture)
        {
            IGraphicsApi::ReleaseTexture(settings_icon);
            settings_icon = nullptr;
        }

        if(graphics_api && imgui_impl_initialized)
        {
            PLOG_INFO << "Shutting down imgui graphics api implementation...";
            graphics_api->ShutdownImGuiImpl();
            imgui_impl_initialized = false;
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

        const char* cleanup_msg = "UiForge Cleaned Up!";
        PLOG_DEBUG << cleanup_msg;
        CoreUtils::InfoMessageBox(cleanup_msg);
        FreeLibrary(core_module_handle);
    }


    return;
}
