#include "forgescript_manager.h"
#include "graphics_api.h"   // I don't like doing this, but I need it so I can expose some functions to Lua
#include "luajit\lua.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <Windows.h>
#include "imgui\sol_ImGui.h"
#include "plog\Log.h"
#include "SimpleConfigLibrary\SCL.hpp"
#include "core_utils.h"

#define FIND_SCRIPT_BY_NAME(name) [&name](const std::unique_ptr<ForgeScript>& script){ return script->file_name == name;}

extern int luaopen_imgui(lua_State* L);
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

ForgeScriptManager::ForgeScriptManager(std::string directory_path):scripts_path(directory_path), uif_lua_state(lua_open())
{
    if(!uif_lua_state)
    {
        throw std::runtime_error("Failed to create Lua state.");
    }

    // Get the modules directory
    std::string uiforge_root_dir = CoreUtils::GetUiForgeRootDirectory();
    modules_path = std::string(scripts_path + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "FORGE_MODULES_DIR"));
    PLOG_DEBUG << "UiForge modules directory: " << modules_path;
    
    // Get the resources directory
    resources_path = std::string(scripts_path + "\\" + GET_CONFIG_VAL(uiforge_root_dir, std::string, "FORGE_RESOURCES_DIR"));
    PLOG_DEBUG << "UiForge resources directory: " << resources_path;

    luaL_openlibs(uif_lua_state);

    sol::state_view uif_sol_state_view(uif_lua_state);  // Need the sol::state_view to initialize the imgui bindings

    InitializeUiForgeBindings(uif_sol_state_view);
    
    // Initialize Sol ImGui Bindings
    sol_ImGui::Init(uif_sol_state_view);

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

void ForgeScriptManager::InitializeUiForgeBindings(sol::state_view lua)
{
    PLOG_INFO << "[+] Initializing UiForge Lua Bindings";
    sol::table uiforge_table = lua.create_named_table("UiForge");

    SetupUiForgeLuaGlobals(uiforge_table, lua);
    
    // Add the modules directory to the lua package path
    lua["package"]["path"] = lua["package"]["path"].get<std::string>() + ";" + modules_path;

    // Initialize Graphics API bindings
    InitializeGraphicsApiLuaBindings(uiforge_table, lua);
}

void ForgeScriptManager::SetupUiForgeLuaGlobals(sol::table uiforge_table, sol::state_view lua)
{
    PLOG_DEBUG << "[+] Setting up UiForge Lua Globals";
    uiforge_table["scripts_path"] = scripts_path;
    PLOG_DEBUG << "[+] scripts_path: " << uiforge_table["scripts_path"].get<std::string>();
    uiforge_table["modules_path"] = modules_path;
    PLOG_DEBUG << "[+] modules_path: " << uiforge_table["modules_path"].get<std::string>();
    uiforge_table["resources_path"] = resources_path;
    PLOG_DEBUG << "[+] resources_path: " << uiforge_table["resources_path"].get<std::string>();
}

void ForgeScriptManager::InitializeGraphicsApiLuaBindings(sol::table uiforge_table, sol::state_view lua)
{
    PLOG_DEBUG << "[+] Initializing graphics api Lua bindings";
    sol::usertype<IGraphicsApi> graphics_api_type = lua.new_usertype<IGraphicsApi>( "IGraphicsApi",
        sol::no_constructor,
        "CreateTextureFromFile", IGraphicsApi::CreateTextureFromFile
    );

    uiforge_table["IGraphicsApi"] = graphics_api_type;
}

ForgeScriptManager::~ForgeScriptManager()
{
    if(uif_lua_state)
    {
        PLOG_DEBUG << "Closing Lua State";
        lua_close(uif_lua_state);

        PLOG_DEBUG << "Setting state to nullptr";
        uif_lua_state = nullptr;
    }
    PLOG_DEBUG << "--- End ForgeScriptManager Destructor ---";
}