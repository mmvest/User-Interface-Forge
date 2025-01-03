#pragma once

#include <string>
#include <vector>
#include <memory>   // For std::unique_ptr
#include "luajit\lua.hpp"
#include "sol\sol.hpp"


// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                            ForgeScript Class                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
// This class provides a way to manage individual lua scripts. Each instance
// is a single lua file, and has access to the script file name and script
// contents. You can run a script by calling the Run() function. Scripts can be 
// enabled or disabled. Disabled scripts will not run when Run() is called.

class ForgeScript
{
    public:
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
         * @brief Retrieves the contents of the Lua script file.
         * 
         * @return The contents of the Lua script as a string.
         */
        std::string GetContents();

        /**
         * @brief Destroys the ForgeScript object.
         */
        ~ForgeScript();

    private:
        std::string file_name;      // The name of the Lua file
        std::string file_contents;  // The contents of the script
        bool enabled;               // Flag to indicate if the script is enabled
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          ForgeScriptManager Class                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
// The ForgeScriptManager provides a set of methods for the core to use for
// managing scripts at a higher level. This functionality includes adding and
// removing scripts from a vector of scripts, running all the scripts, and
// retrieving a script by name.

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
         * @brief Retrieves a pointer to a specific Lua script by its file name.
         * 
         * @param file_name The file name of the Lua script to retrieve.
         * @return A pointer to the requested ForgeScript, or nullptr if the script does not exist.
         * @note The returned pointer must not be deleted by the caller.
         */
        ForgeScript* GetScript(const std::string file_name);

        /**
         * @brief Destroys the ForgeScriptManager instance and releases all managed scripts.
         * 
         * @note the Lua state (`lua_State*`) provided during construction is not cleaned up 
         * by this destructor. It is the caller's responsibility to manage the lifetime of the 
         * Lua state.
         */
        ~ForgeScriptManager();
        
    private:
        lua_State* uif_lua_state;                           // The lua state for the ForgeScripts to use when running
        std::string scripts_path;                           // The full path to the lua scripts that will be made into ForgeScript objects
        std::vector<std::unique_ptr<ForgeScript>> scripts;  // Vector to hold all ForgeScripts
};