#pragma once

#include <string>
#include <vector>
#include <memory>   // For std::unique_ptr
#include "luajit\lua.hpp"
#include "sol\sol.hpp"

class ForgeScript
{
    public:
        // Constructor to initialize with the Lua file name
        ForgeScript(const std::string file_name);

        // Method to run the Lua script
        void Run(lua_State* curr_lua_state);

        // Method to enable the script
        void Enable();

        // Method to disable the script
        void Disable();

        // Method to check if the script is enabled
        bool IsEnabled() const;

        std::string GetContents();

        // Destructor to clean up resources
        ~ForgeScript();

        std::string file_name;      // The name of the Lua file

    private:
        std::string file_contents;  // The contents of the script
        bool enabled;               // Flag to indicate if the script is enabled
};

class ForgeScriptManager
{
    public:
        // Constructor to load all scripts from the specified directory_path
        ForgeScriptManager(const std::string& directory_path);

        // Method to add a new ForgeScript
        void AddScript(const std::string file_name);

        // Method to remove a ForgeScript by its file_name
        void RemoveScript(const std::string file_name);

        // Method to run all enabled scripts
        void RunScripts();

        // Method to get a reference to a specific ForgeScript
        ForgeScript* GetScript(const std::string file_name);

        // Destructor to clean up all scripts
        ~ForgeScriptManager();
        
        lua_State* uif_lua_state;
        // sol::state_view uif_sol_state_view;

    private:

        std::vector<std::unique_ptr<ForgeScript>> scripts;  // Vector to hold all ForgeScripts
        std::string directory_path;                         // Directory where Lua scripts are located
};