#pragma once

// forgescript_manager.cpp

#ifndef FORGESCRIPT_MANAGER_H
#define FORGESCRIPT_MANAGER_H

#include <string>
#include <vector>
#include <memory>  // For std::unique_ptr
#include <lua.hpp> // Include the Lua headers
#include <filesystem> // For handling filesystem operations

// Forward declaration
class ForgeScript;

// ForgeScript class definition
class ForgeScript {
public:
    // Constructor to initialize with the Lua file name
    ForgeScript(const std::string& filename);

    // Method to run the Lua script
    void Run(lua_State* L);

    // Method to enable the script
    void Enable();

    // Method to disable the script
    void Disable();

    // Method to check if the script is enabled
    bool IsEnabled() const;

    // Destructor to clean up resources
    ~ForgeScript();

private:
    std::string filename; // The name of the Lua file
    bool enabled;         // Flag to indicate if the script is enabled
};

// ForgeScriptManager class definition
class ForgeScriptManager {
public:
    // Constructor to load all scripts from the specified directory
    ForgeScriptManager(const std::string& directory);

    // Method to add a new ForgeScript
    void AddScript(const std::string& filename);

    // Method to remove a ForgeScript by its filename
    void RemoveScript(const std::string& filename);

    // Method to run all enabled scripts
    void RunEnabledScripts(lua_State* L);

    // Method to get a reference to a specific ForgeScript
    ForgeScript* GetScript(const std::string& filename);

    // Destructor to clean up all scripts
    ~ForgeScriptManager();

private:
    std::vector<std::unique_ptr<ForgeScript>> scripts; // Vector to hold all ForgeScripts
    std::string directory; // Directory where Lua scripts are located
};

#endif // FORGESCRIPT_MANAGER_H