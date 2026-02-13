#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "luajit\lua.hpp"
#include "sol\sol.hpp"

/**
 * @brief The ForgeScriptCallbackType identifies a Lua callback that a script can register with the ForgeScriptManager.
 *
 *  - Settings: Used to render a script's UI settings inside the UiForge Settings panel. Executed when the script 
 *    is selected and the "Settings" tab is drawn (ForgeScript::RunSettingsCallback()).
 * 
 *  - DisableScript: Used for script teardown/cleanup. Executed once when a script transitions from enabled to
 *   disabled (ForgeScript::Disable() -> ForgeScript::RunDisableScriptCallback()).
 */
enum class ForgeScriptCallbackType : uint8_t
{
    Settings = 0,
    DisableScript = 1,
};

struct ForgeScriptDebug
{
    size_t time_to_read_file_contents;
    size_t time_to_hash_file_contents;
    size_t total_time_loading_from_mem;
    size_t total_time_executing;
    size_t times_executed;
    size_t script_size;
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                            ForgeScript Class                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
/**
 * @class ForgeScript
 * @brief This class provides a way to manage individual lua scripts.
 * 
 * Each instance is a single lua file, and has access to the script file name and
 * script contents. You can run a script by calling the `Run()` function. Scripts 
 * can be enabled or disabled. Disabled scripts will not run when `Run()` is called.
 */
class ForgeScript
{
    public:

        /**
         * @brief Default ForgeScript constructor -- just creates an empty ForgeScript.
         */
        ForgeScript();

        /**
         * @brief Constructs a ForgeScript object by reading the contents of a Lua script file.
         * 
         * @param file_name The name of the Lua script file to load.
         * @throws std::runtime_error If the file cannot be opened or read.
         */
        ForgeScript(const std::string file_name);

        /**
         * @brief Executes the Lua script using the given Lua state.
         * 
         * @param curr_lua_state A pointer to the Lua state (`lua_State*`) in which the script will be executed.
         * @throws std::runtime_error If the script fails to load or execute in the Lua state.
         * @note This function does nothing if the script is disabled (see IsEnabled()).
         */
        void Run(lua_State* curr_lua_state);

        /**
         * @brief Reload script contents from disk and reset its isolated Lua environment.
         *
         * Preserves the enabled/disabled state of the script. If the script was enabled,
         * the disable callback is executed before reloading.
         *
         * @param curr_lua_state The Lua state used to manage registry references for the script environment.
         */
        void Reload(lua_State* curr_lua_state);

        /**
         * @brief Executes the Lua Scripts registered settings callback function.
         * 
         * @throws std::runtime_error If the function fails to execute or the settings callback has not been set. 
         * @note This function does nothing if the script is disabled (see IsEnabled()).
         */
        void RunSettingsCallback();

        /**
         * @brief Executes the Lua Scripts registered disable callback function.
         *
         * @note This function is intended to be called when a script is disabled.
         */
        void RunDisableScriptCallback();

        /**
         * @brief Enables the script, allowing it to be executed.
         */
        void Enable();

        /**
         * @brief Disables the script, preventing it from being executed.
         */
        void Disable();

        /**
         * @brief Checks if the script is currently enabled.
         * 
         * @return true if the script is enabled; false otherwise.
         */
        bool IsEnabled() const;

        /**
         * @brief Retrieves the name of the Lua script file associated with this object.
         * 
         * @return The file name of the Lua script as a string.
         */
        std::string GetFileName();

        /**
         * @brief Returns the last observed write time of the script file on disk.
         */
        std::filesystem::file_time_type GetLastWriteTime() const;

        /**
         * @brief Returns the last time this script was loaded/reloaded.
         */
        std::chrono::system_clock::time_point GetLastReloadTime() const;

        /**
         * @brief Formats the last observed write time into a human-readable string.
         */
        std::string GetLastWriteTimeString() const;

        /**
         * @brief Formats the last reload time into a human-readable string.
         */
        std::string GetLastReloadTimeString() const;

        /**
         * @brief Poll the script file write time and report if it differs from the loaded version.
         *
         * Updates the script's last observed write time.
         *
         * @return true if the file on disk appears newer/different than the currently loaded contents.
         */
        bool IsOutOfDateOnDisk();


        /**
         * @brief Retrieves the contents of the Lua script file.
         * 
         * @return The contents of the Lua script as a string.
         */
        std::string GetContents();


        /**
         * @brief Retrieve the hash value of the file contents of the Lua script file.
         * 
         * @return The hash value of the file contents as a size_t.
         */
        std::size_t GetHash();

        /**
         * @brief Determines if two ForgeScripts are the same by comparing their file names and the hash of their contents.
         */
        bool operator==(const ForgeScript& other) const
        {
            return file_name == other.file_name && hash == other.hash;
        }

        /**
         * @brief Destroys the ForgeScript object.
         */
        ~ForgeScript();

        bool enabled;                                       // Flag to indicate if the script is enabled
        ForgeScriptDebug stats;                             // Keep track of some debug stats for each script
        sol::protected_function settings_callback;          // Function to run to display script settings
        sol::protected_function disable_script_callback;    // Function to run when script is disabled
    private:
        /**
         * @brief Reads the script file from disk into memory.
         *
         * Populates `file_contents`, updates `stats.script_size`, and recomputes `hash`.
         * Also captures the file's `last_write_time` so reload-on-save can detect changes.
         *
         * @throws std::runtime_error If the file cannot be opened/read.
         */
        void LoadFromDisk();

        /**
         * @brief Ensure this script has a per-script Lua environment table in the registry.
         *
         * Creates a new environment table and stores it under `env_ref` if one does not exist.
         * The environment falls back to the real global table (`_G`) via `__index = _G` so
         * scripts can still access globals like `ImGui` and `UiForge`
         *
         * Also sets `env._G = env` so script code cannot escape isolation via the `_G` global.
         *
         * @param curr_lua_state The active Lua state used to allocate and reference the environment.
         */
        void EnsureLuaEnvironment(lua_State* curr_lua_state);

        /**
         * @brief Dispose of the script's isolated Lua environment.
         *
         * Unrefs the registry entry referenced by `env_ref`, allowing the old environment
         * and any script globals stored within to be garbage collected.
         *
         * @param curr_lua_state The active Lua state used to unref the environment.
         */
        void ResetLuaEnvironment(lua_State* curr_lua_state);

        std::string file_name;                      // The name of the Lua file
        std::string file_contents;                  // The contents of the script
        std::size_t hash;                           // hash of the contents, for quick comparison

        int env_ref = LUA_NOREF;                    // Registry ref to this script's isolated environment table
        std::filesystem::file_time_type loaded_write_time{};     // Write time of the file when contents were last loaded
        std::filesystem::file_time_type observed_write_time{};   // Most recently observed write time on disk
        bool has_write_time = false;
        std::chrono::system_clock::time_point last_reload_time{};
        bool has_reload_time = false;
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          ForgeScriptManager Class                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
/**
 * @class ForgeScriptManager
 * @brief Provides methods for managing scripts at a higher level.
 *
 * The ForgeScriptManager includes functionality for:
 * 
 * - Adding and removing scripts from a vector of scripts.
 * 
 * - Running all the scripts.
 * 
 * - Retrieving a script by name.
 * 
 *
 * These methods are designed for the core to interact with and manage
 * scripts efficiently.
 */
class ForgeScriptManager
{
    public:
        /**
         * @brief Constructs a ForgeScriptManager instance and loads all scripts from the specified directory.
         * 
         * @param directory_path The path to the directory containing Lua scripts.
         * @param uif_lua_state A pointer to the Lua state used for script execution.
         * @throws std::runtime_error If the Lua state is null or invalid.
         */
        ForgeScriptManager(std::string directory_path, lua_State* uif_lua_state);

        /**
         * @brief Adds a new Lua script to the manager.
         * 
         * @param file_name The full path to the Lua script file to add.
         * @note If the script fails to load, an error is logged, and the script is not added.
         */
        void AddScript(const std::string file_name);

        /**
         * @brief Removes a Lua script from the manager by its file name.
         * 
         * @param file_name The file name of the Lua script to remove.
         * @note If no script with the specified name exists, the method has no effect.
         */
        void RemoveScript(const std::string file_name);

        /**
         * @brief Executes all enabled Lua scripts managed by this instance.
         * 
         * Each script is run using the provided Lua state. If a script encounters an error
         * during execution, the error is logged, and the script is disabled to prevent further execution.
         * 
         * @note Only enabled scripts are executed. Disabled scripts remain inactive.
         */
        void RunScripts();

        /**
         * @brief Configure reload-on-save behavior for all managed scripts.
         *
         * @param enabled If true, scripts are polled for file timestamp changes and reloaded automatically.
         * @param poll_ms Minimum time between polling passes (milliseconds).
         */
        void SetReloadOnSave(bool enabled, uint32_t poll_ms);

        /**
         * @brief Request a script reload by its full file path.
         *
         * @note The reload occurs outside of script execution so we don't screw something up.
         */
        void RequestReload(const std::string& file_name);

        /**
         * @brief Request a reload for all scripts.
         */
        void RequestReloadAll();
        
        /**
         * @brief Registers a callback function on the currently executing script.
         *
         * @param type Which callback slot to register.
         * @param callback A valid Lua function to register.
         *
         * @note Unrecognized callback types are ignored with a log warning.
         */
        void RegisterCallback(ForgeScriptCallbackType type, sol::protected_function callback);

        /**
         * @brief Retrieves a pointer to a specific Lua script by its file name.
         * 
         * @param file_name The file name of the Lua script to retrieve.
         * @return A pointer to the requested ForgeScript, or nullptr if the script does not exist.
         * @note The returned pointer must not be deleted by the caller.
         */
        ForgeScript* GetScript(const std::string file_name);

        /**
         * @brief Retrieves a pointer to a specific Lua script by its index.
         * 
         * @param index The index to access.
         * @return A pointer to the requested ForgeScript, or nullptr if the script does not exist.
         * @note The returned pointer must not be deleted by the caller.
         */
        ForgeScript* GetScript(const unsigned index);

        /**
         * @brief Retrieve the number of scripts in the scripts list.
         * 
         * @return An unsigned value indicating the number of scripts in the list.
         */
        unsigned GetScriptCount();

        /**
         * @brief Scans the scripts directory for new scripts and adds any that aren't already loaded.
         */
        void RefreshScripts();

        /**
         * @brief Retrieve the debug stats of all scripts managed by the script manager and update the managers debug info with it.
         */
        void ForgeScriptManager::UpdateDebugStats();

        /**
         * @brief Destroys the ForgeScriptManager instance and releases all managed scripts.
         * 
         * @note the Lua state (`lua_State*`) provided during construction is not cleaned up 
         * by this destructor. It is the caller's responsibility to manage the lifetime of the 
         * Lua state.
         */
        ~ForgeScriptManager();
        
        ForgeScriptDebug stats;

    private:
        /**
         * @brief Apply any pending reload requests.
         *
         * Reload requests are queued (e.g. from the UI button or reload-on-save polling)
         * and then processed at a safe point in the normal loop (outside of script execution).
         *
         * On reload failure, the error is logged and the script is disabled.
         */
        void ProcessPendingReloads();

        lua_State* uif_lua_state;                           // The lua state for the ForgeScripts to use when running
        std::string scripts_path;                           // The full path to the lua scripts that will be made into ForgeScript objects
        std::vector<std::unique_ptr<ForgeScript>> scripts;  // Vector to hold all ForgeScripts
        ForgeScript* currently_executing_script;            // Pointer to the currently executing ForgeScript

        std::unordered_set<std::string> pending_reload;     // Full script paths pending reload

        bool reload_on_save_enabled = false;
        uint32_t reload_on_save_poll_ms = 2500;
        std::chrono::steady_clock::time_point reload_on_save_last_poll{};
        bool reload_on_save_has_polled = false;
};
