#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <lua.hpp>
#include <sol/sol.hpp>

/**
 * @brief The ForgeScriptCallbackType identifies a Lua callback that a script can register with the ForgeScriptManager.
 *
 *  - Settings: Used to render a script's UI settings inside the UiForge Settings panel. Executed when the script 
 *    is selected and the "Settings" tab is drawn (ForgeScript::RunSettingsCallback()).
 * 
 *  - DisableScript: Used for script teardown/cleanup. Executed once when a script transitions from enabled to
 *   disabled (ForgeScript::Disable() -> ForgeScript::RunDisableScriptCallback()).
 *
 *  - Save: Persistent state serialization. The callback takes no arguments and returns a plain-data table
 *    (or nil to skip). The table is captured into the profile being saved
 *    (ForgeScriptManager::SaveProfile(), triggered from the File > Save Profile menu).
 *
 *  - Load: Persistent state deserialization. The callback receives the table previously produced by the
 *    Save callback when a profile is applied (ForgeScriptManager::ApplyProfile()).
 *
 *  - OnEject: Last-chance cleanup when the core is ejecting/unloading. Executed for every script (enabled
 *    or not) right before the script manager is destroyed (ForgeScriptManager::RunOnEjectCallbacks()).
 */
enum class ForgeScriptCallbackType : uint8_t
{
    Settings = 0,
    DisableScript = 1,
    Save = 2,
    Load = 3,
    OnEject = 4,
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
         * @brief Executes the Lua Scripts registered on-eject callback function.
         *
         * Errors are logged and swallowed so one script cannot interrupt the eject sequence.
         *
         * @note This function is intended to be called once, right before the core unloads.
         */
        void RunOnEjectCallback();

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
         * @brief Marks this script as a packaged script living in its own subdirectory.
         *
         * A packaged script is a script directory dropped into the scripts folder
         * (e.g. "scripts\\my_mod\\my_mod.lua"). If the package directory contains a
         * "modules" folder, that folder is prepended to package.path while the script
         * runs so its local require() calls resolve locally before the shared modules
         * directory. If it contains a "resources" folder, relative resource paths
         * (e.g. UiForge.LoadTexture) are resolved against it before the shared
         * resources directory.
         *
         * @param directory_path Full path to the script's package directory.
         */
        void SetPackageDirectory(const std::string& directory_path);

        /**
         * @brief Returns the package's local modules directory, or "" when the script
         * is not packaged or the package has no modules folder.
         */
        std::string GetPackageModulesDir() const;

        /**
         * @brief Returns the package's local resources directory, or "" when the script
         * is not packaged or the package has no resources folder.
         */
        std::string GetPackageResourcesDir() const;

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
        sol::protected_function save_callback;              // Function to run to get persistent state for save
        sol::protected_function load_callback;              // Function to run to restore persistent state on load
        sol::protected_function on_eject_callback;          // Function to run when the core is ejecting/unloading
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

        std::string package_dir;                    // Root directory of a packaged script ("" for loose scripts)
        std::string package_modules_dir;            // "<package_dir>\modules" when it exists, else ""
        std::string package_resources_dir;          // "<package_dir>\resources" when it exists, else ""

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
         * Discovers both loose .lua files sitting directly in the scripts directory and
         * packaged scripts living in their own subdirectory (see RefreshScripts()).
         *
         * @param directory_path The path to the directory containing Lua scripts.
         * @param uif_lua_state A pointer to the Lua state used for script execution.
         * @param excluded_subdirs Names of subdirectories that must never be treated as
         * script packages (e.g. the shared modules, resources, and profiles directories).
         * Compared case-insensitively.
         * @throws std::runtime_error If the Lua state is null or invalid.
         */
        ForgeScriptManager(std::string directory_path, lua_State* uif_lua_state,
                           std::vector<std::string> excluded_subdirs = {});

        /**
         * @brief Adds a new Lua script to the manager.
         *
         * @param file_name The full path to the Lua script file to add.
         * @param package_dir Full path to the script's package directory when the script
         * is a packaged script, or "" (the default) for a loose script.
         * @note If the script fails to load, an error is logged, and the script is not added.
         */
        void AddScript(const std::string file_name, const std::string package_dir = std::string());

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
         * @brief Sets the directory where profile files are written and read.
         *
         * @param directory_path Full path to the profiles directory. Created on demand when saving.
         */
        void SetProfilesDirectory(const std::string& directory_path);

        /**
         * @brief Sets the modules directory used to detect which require()d modules are user modules.
         *
         * When set, script reloads purge matching entries from package.loaded so edited modules
         * are re-read from disk (see PurgeUserModuleCache()).
         *
         * @param directory_path Full path to the Lua modules directory.
         */
        void SetModulesDirectory(const std::string& directory_path);

        /**
         * @brief Saves the current setup as a named profile.
         *
         * A profile captures the set of enabled scripts, the per-script state returned by each
         * enabled script's Save callback, and the current ImGui window settings (positions,
         * sizes, collapsed state). The profile is serialized with serpent and written atomically
         * (temp file + rename) to "<profiles dir>\\<name>.profile.lua". Errors are logged per
         * script and do not stop the rest of the profile from saving.
         *
         * @param profile_name The profile name. Also used as the file stem, so it must not
         * contain path separators or other characters invalid in file names.
         */
        void SaveProfile(const std::string& profile_name);

        /**
         * @brief Applies a named profile.
         *
         * Enables every script listed in the profile, disables every script that is not listed,
         * and queues the profile's per-script states and window settings to be delivered at the
         * end of the next RunScripts() pass. The one-pass delay lets newly enabled scripts run
         * once so they can register their Load callbacks and create their windows before state
         * and window positions are applied.
         *
         * The profile file is parsed inside an empty sandbox environment (profiles are plain
         * data and must not be able to touch globals).
         *
         * @param profile_name The profile name to apply.
         */
        void ApplyProfile(const std::string& profile_name);

        /**
         * @brief Lists the names of all saved profiles in the profiles directory.
         */
        std::vector<std::string> ListProfiles() const;

        /**
         * @brief Returns the name of the most recently saved or applied profile, or "" if none.
         */
        std::string GetCurrentProfileName() const;

        /**
         * @brief Returns the preferred profile name for the current process, or "" if none is set.
         *
         * Preferred profiles are stored per process executable name in
         * "<profiles dir>\\preferred_profiles.lua" so a profile made for one game cannot be
         * auto-applied inside a different process.
         */
        std::string GetPreferredProfileName() const;

        /**
         * @brief Sets the given profile as the preferred profile for the current process.
         */
        void SetPreferredProfile(const std::string& profile_name);

        /**
         * @brief Clears the preferred profile for the current process.
         */
        void ClearPreferredProfile();

        /**
         * @brief Applies the preferred profile for the current process, if one is set.
         *
         * Intended to be called once during core initialization, after the script manager and
         * Lua state are fully set up. Does nothing when no preferred profile is configured or
         * the profile file no longer exists.
         */
        void ApplyPreferredProfile();

        /**
         * @brief Runs the OnEject callback of every script that registered one.
         *
         * Intended to be called once during core cleanup, before the script manager is destroyed.
         */
        void RunOnEjectCallbacks();

        /**
         * @brief Scans the scripts directory for new scripts and adds any that aren't already loaded.
         *
         * Two kinds of scripts are discovered:
         * - Loose scripts: .lua files sitting directly in the scripts directory.
         * - Packaged scripts: a subdirectory containing an entry script named after the
         *   directory ("<dir>\\<dir>.lua"), or "main.lua" / "init.lua" as fallbacks. The
         *   package may optionally contain its own "modules" and "resources" folders,
         *   which take priority over the shared directories for that script.
         *
         * Subdirectories listed in the excluded set passed to the constructor (the shared
         * modules/resources/profiles directories) are never treated as packages.
         */
        void RefreshScripts();

        /**
         * @brief Returns the script currently being executed by the manager, or nullptr.
         *
         * Set while a script's main chunk runs inside RunScripts() and while a settings
         * callback runs via RunSettingsCallback(). Used to resolve per-script resources.
         */
        ForgeScript* GetCurrentlyExecutingScript() const;

        /**
         * @brief Runs a script's settings callback with the manager's currently
         * executing script context set, so per-script resource resolution and callback
         * registration work from inside settings callbacks too.
         *
         * @param script The script whose settings callback should run. No-op when null.
         * @throws std::runtime_error Propagated from the settings callback on failure.
         */
        void RunSettingsCallback(ForgeScript* script);

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

        /**
         * @brief Builds the file path for a named profile, "<profiles dir>\\<name>.profile.lua".
         */
        std::string GetProfileFilePath(const std::string& profile_name) const;

        /**
         * @brief Builds the path of the per-process preferred profile map file.
         */
        std::string GetPreferredProfilesFilePath() const;

        /**
         * @brief Returns the current process executable name, lowercased (e.g. "pcsx2-qt.exe").
         */
        static std::string GetCurrentProcessName();

        /**
         * @brief Loads and executes a plain-data Lua file inside an empty sandbox environment.
         *
         * @return The table the file returned, or sol::lua_nil on any failure (logged).
         */
        sol::object LoadDataFile(const std::string& file_path) const;

        /**
         * @brief Serializes a table with serpent and writes "return <data>" atomically
         * (temp file + rename) to the given path. Creates parent directories on demand.
         *
         * @return true on success. Failures are logged.
         */
        bool WriteDataFileAtomic(const std::string& file_path, sol::object data) const;

        /**
         * @brief Delivers a pending profile apply queued by ApplyProfile().
         *
         * Runs at the end of RunScripts(), after the profile's scripts have executed once and
         * registered their callbacks. Passes each saved state table to the owning script's Load
         * callback and restores window positions from the profile's window settings.
         */
        void ApplyPendingProfileState();

        /**
         * @brief Restores window positions, sizes, and collapsed state from an ImGui ini blob.
         *
         * The blob is what ImGui::SaveIniSettingsToMemory() produced when the profile was saved.
         * It is handed back to ImGui so windows created later pick it up, and each [Window]
         * entry is also applied to existing windows by name via ImGui::SetWindowPos() and
         * friends, since ImGui only reads ini settings on window creation.
         */
        static void ApplyWindowSettings(const std::string& ini_blob);

        /**
         * @brief Removes user modules from package.loaded so the next require() re-reads them from disk.
         *
         * Only module names that resolve to a .lua file under the modules directory are purged;
         * built-in libraries and embedded modules are untouched. Called before pending script
         * reloads are applied so a hot reload picks up module edits too.
         *
         * @note Scripts that are not being reloaded keep whatever references they already hold to
         * the old module tables until they are reloaded themselves.
         */
        void PurgeUserModuleCache();

        lua_State* uif_lua_state;                           // The lua state for the ForgeScripts to use when running
        std::string scripts_path;                           // The full path to the lua scripts that will be made into ForgeScript objects
        std::vector<std::string> excluded_subdirs;          // Lowercased names of scripts subdirectories that are never script packages
        std::string profiles_path;                          // The full path to the directory holding profile files
        std::string current_profile_name;                   // Name of the most recently saved/applied profile ("" if none)

        bool has_pending_profile_apply = false;             // True when ApplyProfile queued state for the end of the next RunScripts pass
        sol::object pending_profile_scripts;                // Profile "scripts" table awaiting delivery to Load callbacks
        std::string pending_window_settings;                // Profile ImGui ini blob awaiting application
        std::string modules_path;                           // The full path to the Lua modules directory (for cache purging on reload)
        std::vector<std::unique_ptr<ForgeScript>> scripts;  // Vector to hold all ForgeScripts
        ForgeScript* currently_executing_script;            // Pointer to the currently executing ForgeScript

        std::unordered_set<std::string> pending_reload;     // Full script paths pending reload

        bool reload_on_save_enabled = false;
        uint32_t reload_on_save_poll_ms = 2500;
        std::chrono::steady_clock::time_point reload_on_save_last_poll{};
        bool reload_on_save_has_polled = false;
};
