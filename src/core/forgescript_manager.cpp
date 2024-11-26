#include "..\..\include\forgescript_manager.h"
#include "..\..\include\lua.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <Windows.h>


#define FIND_SCRIPT_BY_NAME(name) [&name](const std::unique_ptr<ForgeScript>& script){ return script->file_name == name;}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                            ForgeScript Class                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
ForgeScript::ForgeScript(const std::string file_name) : file_name(file_name), enabled(true)
{
    std::ifstream file(file_name, std::ios::in | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + file_name);
    }

    // Read the file contents into the file_contents string
    file.seekg(0, std::ios::end);
    file_contents.reserve(file.tellg());
    file.seekg(0, std::ios::beg);
    file_contents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void ForgeScript::Run(lua_State* curr_lua_state)
{
    if(!IsEnabled())
    {
        return;
    }

    int load_result = luaL_loadbuffer(curr_lua_state, file_contents.c_str(), file_contents.size(), file_name.c_str());
    if(load_result != LUA_OK)
    {
        
        // Retrieve error from stack
        const char* lua_error = lua_tostring(curr_lua_state, -1);
        std::string err_msg = "Error loading Lua Script " + file_name + ": " + lua_error;
        lua_pop(curr_lua_state, 1);
        // TODO: Log the error
        throw std::runtime_error(err_msg);
    }

    int call_result = lua_pcall(curr_lua_state, 0, 0,  0);
    if(call_result != LUA_OK)
    {
        
        const char* lua_error = lua_tostring(curr_lua_state, -1);
        std::string err_msg = "Error calling Lua Script " + file_name + ": " + lua_error;
        lua_pop(curr_lua_state, 1);
        // TODO: Log the error
        throw std::runtime_error(err_msg);
    }
}

void ForgeScript::Enable()
{
    enabled = true;
}

void ForgeScript::Disable()
{
    enabled = false;
}

bool ForgeScript::IsEnabled() const
{
    return enabled;
}

std::string ForgeScript::GetContents()
{
    return file_contents;
}

ForgeScript::~ForgeScript(){}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          ForgeScriptManager Class                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

ForgeScriptManager::ForgeScriptManager(const std::string& directory_path)
{
    uif_lua_state = luaL_newstate();
    if(!uif_lua_state)
    {
        throw std::runtime_error("Failed to create Lua state.");
    }

    luaL_openlibs(uif_lua_state);

    // Get all of the scripts from the directory and add them to the script vector
    for(const auto& entry : std::filesystem::directory_iterator(directory_path))
    {
        if(!entry.is_regular_file()) continue;

        if (entry.path().extension() == ".lua") AddScript(directory_path + "\\" + entry.path().filename().string());
    }
}

void ForgeScriptManager::AddScript(const std::string file_name)
{
    try
    {
        scripts.emplace_back(std::make_unique<ForgeScript>(file_name));
    }
    catch(const std::exception& e)
    {
        // TODO: Log the error somehow but don't crash... still need to 
        // figure out how I want to do logging

    }
}

void ForgeScriptManager::RemoveScript(const std::string file_name)
{
    scripts.erase
    (
        std::remove_if(scripts.begin(), scripts.end(), FIND_SCRIPT_BY_NAME(file_name)),
        scripts.end()
    );
}

void ForgeScriptManager::RunScripts()
{
    for(const auto& script : scripts)
    {
        try
        {
            script->Run(uif_lua_state);
        }
        catch(const std::exception& err)
        {
            std::thread([err] { MessageBoxA(nullptr, err.what(), "UiForge Error",  MB_OK | MB_ICONERROR); }).detach();
            script->Disable();
        }
    }
}

ForgeScript* ForgeScriptManager::GetScript(const std::string file_name)
{
    auto iter = std::find_if(scripts.begin(), scripts.end(), FIND_SCRIPT_BY_NAME(file_name));
    
    if(iter != scripts.end()) return iter->get();
    
    return nullptr;
}

ForgeScriptManager::~ForgeScriptManager()
{
    if(uif_lua_state) lua_close(uif_lua_state);
}