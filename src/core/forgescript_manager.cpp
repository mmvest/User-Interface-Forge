#include <filesystem>
#include <fstream>
#include "core\forgescript_manager.h"
#include "core\util.h"
#include "luajit\lua.hpp"
#include "plog\Log.h"

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
        throw std::runtime_error(err_msg);
    }

    int call_result = lua_pcall(curr_lua_state, 0, 0,  0);
    if(call_result != LUA_OK)
    {
        
        const char* lua_error = lua_tostring(curr_lua_state, -1);
        std::string err_msg = "Error calling Lua Script " + file_name + ": " + lua_error;
        lua_pop(curr_lua_state, 1);
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

std::string ForgeScript::GetFileName()
{
    return file_name;
}

std::string ForgeScript::GetContents()
{
    return file_contents;
}

ForgeScript::~ForgeScript(){}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          ForgeScriptManager Class                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define FIND_SCRIPT_BY_NAME(name) [&name](const std::unique_ptr<ForgeScript>& script){ return script->GetFileName() == name;}

ForgeScriptManager::ForgeScriptManager(std::string scripts_path, lua_State* uif_lua_state):scripts_path(scripts_path), uif_lua_state(uif_lua_state)
{
    if(!uif_lua_state)
    {
        throw std::runtime_error("Cannot construct ForgeScriptManager with null or invalid Lua State.");
    }

    // Get all of the scripts from the directory and add them to the script vector
    for(const auto& entry : std::filesystem::directory_iterator(scripts_path))
    {
        if(!entry.is_regular_file()) continue;

        if (entry.path().extension() == ".lua") AddScript(scripts_path + "\\" + entry.path().filename().string());
    }
}

void ForgeScriptManager::AddScript(const std::string file_name)
{
    try
    {
        scripts.emplace_back(std::make_unique<ForgeScript>(file_name));
    }
    catch(const std::exception& err)
    {
        PLOG_ERROR << "Error adding script: " << err.what();
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
            PLOG_ERROR << "Error running script " << script->GetFileName() << ": " << err.what();
            CoreUtils::ErrorMessageBox(err.what());
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

ForgeScriptManager::~ForgeScriptManager(){}