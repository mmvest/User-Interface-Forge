#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "plog\Log.h"

#include "core\util.h"
#include "core\forgescript_manager.h"

// Time stuff is hard
static std::chrono::system_clock::time_point FileTimeToSystemClock(std::filesystem::file_time_type file_time)
{
    return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
}

static std::string FormatLocalTime(std::chrono::system_clock::time_point tp)
{
    const std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_local{};
    localtime_s(&tm_local, &tt);

    // Use std::ostringstream because std::put_time is a stream manipulator that only
    // works with output streams. It formats the std::tm into text inside the stream,
    // and oss.str() retrieves the resulting formatted string.
    std::ostringstream oss;
    oss << std::put_time(&tm_local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                            ForgeScript Class                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

ForgeScript::ForgeScript() : enabled(false) {}

ForgeScript::ForgeScript(const std::string file_name) : enabled(false), file_name(file_name)
{
    stats = { 0 };
    LoadFromDisk();

    last_reload_time = std::chrono::system_clock::now();
    has_reload_time = true;
}

void ForgeScript::LoadFromDisk()
{
    std::ifstream file(file_name, std::ios::in | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + file_name);
    }

    // Read the file contents into the file_contents string
    auto start_time = std::chrono::steady_clock::now();
    file.seekg(0, std::ios::end);
    file_contents.clear();
    file_contents.reserve(file.tellg());
    file.seekg(0, std::ios::beg);
    file_contents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto end_time = std::chrono::steady_clock::now();
    stats.time_to_read_file_contents = std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count();

    stats.script_size = file_contents.size();

    // hash the file contents
    start_time = std::chrono::steady_clock::now();
    hash = std::hash<std::string>{}(file_contents);
    end_time = std::chrono::steady_clock::now();
    stats.time_to_hash_file_contents = std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count();

    try
    {
        observed_write_time = std::filesystem::last_write_time(file_name);
        loaded_write_time = observed_write_time;
        has_write_time = true;
    }
    catch(...)
    {
        has_write_time = false;
    }
}

void ForgeScript::EnsureLuaEnvironment(lua_State* curr_lua_state)
{
    // Ideally, I wanted to isolate the environments for each script so I can get rid of a bunch of annoying
    // behaviors and such associated with them all sharing a global space. This function does just that.
    // So what is actually going on here? Lua has a registry which is a place where Lua can store persistent info
    // between script runs. Since we want each script to have its own environment, we need to create that environment,
    // then store it in the registry so it can be used across runs. We also want scripts to be able to access our global
    // states when necessary, which means we need to create a metatable and set the __index property which is how the
    // lua script knows what to do if it tries to read a key that doesn't exist in its own table. Doing so allows calls
    // to uiforge and imgui stuff to fall through into the global space which is the behavior we want.

    // If we already have an environment for the script, bail out
    if (env_ref != LUA_NOREF)
    {
        return;
    }

    // To keep myself straight, here are comments that show the stack. Note that
    // env = the custom environment for the script
    // mt = the metatable for that environment
    // env_ref = the reference to the environment table in the registry


    lua_newtable(curr_lua_state);                       // [env]
    lua_newtable(curr_lua_state);                       // [env, mt]
    lua_pushvalue(curr_lua_state, LUA_GLOBALSINDEX);    // [env, mt, _G]
    lua_setfield(curr_lua_state, -2, "__index");        // [env, mt] (We popped _G and set it as the __index value in the metatable)
    lua_setmetatable(curr_lua_state, -2);               // [env] (we set mt as the metatable for env)

    // Now we want to tell env that its global environment table IS itself.
    lua_pushvalue(curr_lua_state, -1);      // [env, env] (yeah we push env on the stack again here)
    lua_setfield(curr_lua_state, -2, "_G"); // [env] (We set env._G = env, which pops env from the top of the stack)

    env_ref = luaL_ref(curr_lua_state, LUA_REGISTRYINDEX); // pop env, get the env_ref
}

void ForgeScript::ResetLuaEnvironment(lua_State* curr_lua_state)
{
    // If there is no environment to reset, bail out.
    if (env_ref == LUA_NOREF)
    {
        return;
    }

    luaL_unref(curr_lua_state, LUA_REGISTRYINDEX, env_ref);
    env_ref = LUA_NOREF;
}

void ForgeScript::Run(lua_State* curr_lua_state)
{
    if(!IsEnabled())
    {
        return;
    }

    auto start_time = std::chrono::steady_clock::now();
    int load_result = luaL_loadbuffer(curr_lua_state, file_contents.c_str(), file_contents.size(), file_name.c_str());
    if(load_result != LUA_OK)
    {
        
        // Retrieve error from stack
        const char* lua_error = lua_tostring(curr_lua_state, -1);
        std::string err_msg = "Error loading Lua Script " + file_name + ": " + lua_error;
        lua_pop(curr_lua_state, 1);
        throw std::runtime_error(err_msg);
    }
    auto end_time = std::chrono::steady_clock::now();
    stats.total_time_loading_from_mem += std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    // This is how we run the script in its own "isolated" environment
    EnsureLuaEnvironment(curr_lua_state);
    lua_rawgeti(curr_lua_state, LUA_REGISTRYINDEX, env_ref); // push env
    lua_setfenv(curr_lua_state, -2);                         // set env

    start_time = std::chrono::steady_clock::now();
    int call_result = lua_pcall(curr_lua_state, 0, 0,  0);
    if(call_result != LUA_OK)
    {
        const char* lua_error = lua_tostring(curr_lua_state, -1);
        std::string err_msg = "Error calling Lua Script " + file_name + ": " + lua_error;
        lua_pop(curr_lua_state, 1);
        throw std::runtime_error(err_msg);
    }
    end_time = std::chrono::steady_clock::now();
    stats.total_time_executing += std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
    stats.times_executed++;
}

void ForgeScript::Reload(lua_State* curr_lua_state)
{
    std::string new_contents;
    ForgeScriptDebug new_stats = stats;
    std::size_t new_hash = hash;
    bool new_has_write_time = false;
    std::filesystem::file_time_type new_write_time{};

    try
    {
        // Try to open the file, if we can't then error out.
        std::ifstream file(file_name, std::ios::in | std::ios::binary);
        if (!file)
        {
            PLOG_ERROR << "Could not open script file for reload: " << file_name;
            return;
        }

        // Grab the new contents and the stats related to that for the debug window and such.
        auto start_time = std::chrono::steady_clock::now();
        file.seekg(0, std::ios::end);
        new_contents.reserve(file.tellg());
        file.seekg(0, std::ios::beg);
        new_contents.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto end_time = std::chrono::steady_clock::now();
        new_stats.time_to_read_file_contents = std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count();
        new_stats.script_size = new_contents.size();

        start_time = std::chrono::steady_clock::now();
        new_hash = std::hash<std::string>{}(new_contents);
        end_time = std::chrono::steady_clock::now();
        new_stats.time_to_hash_file_contents = std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count();

        // Get the write time of the file
        try
        {
            new_write_time = std::filesystem::last_write_time(file_name);
            new_has_write_time = true;
        }
        catch(...)
        {
            new_has_write_time = false;
        }
    }
    catch (const std::exception& err)
    {
        PLOG_ERROR << "Exception reading script for reload (" << file_name << "): " << err.what();
        return;
    }

    // Try to parse the new lua file to make sure it is valid
    const int is_script_valid = luaL_loadbuffer(curr_lua_state, new_contents.c_str(), new_contents.size(), file_name.c_str());
    if (is_script_valid != LUA_OK)
    {
        const char* lua_error = lua_tostring(curr_lua_state, -1);
        PLOG_ERROR << "Hot-reload failed for " << file_name << ": " << (lua_error ? lua_error : "(unknown error)");
        lua_pop(curr_lua_state, 1);

        // Don't spam retries every frame for the same bad save. We'll try again when the file changes again.
        if (new_has_write_time)
        {
            observed_write_time = new_write_time;
            loaded_write_time = new_write_time;
            has_write_time = true;
        }
        return;
    }
    lua_pop(curr_lua_state, 1); // pop the parsed chunk

    // We want to perform any cleanup from the script disable callback so we can cleanly close the script.
    const bool was_enabled = enabled;
    if (was_enabled)
    {
        Disable();
    }

    // Clear callbacks.
    settings_callback = sol::protected_function();
    disable_script_callback = sol::protected_function();

    ResetLuaEnvironment(curr_lua_state);

    // Apply new version.
    file_contents.swap(new_contents);
    hash = new_hash;
    stats.time_to_read_file_contents = new_stats.time_to_read_file_contents;
    stats.time_to_hash_file_contents = new_stats.time_to_hash_file_contents;
    stats.script_size = new_stats.script_size;

    // Reset runtime stats for the new version.
    stats.total_time_executing = 0;
    stats.total_time_loading_from_mem = 0;
    stats.times_executed = 0;

    if (new_has_write_time)
    {
        observed_write_time = new_write_time;
        loaded_write_time = new_write_time;
        has_write_time = true;
    }

    last_reload_time = std::chrono::system_clock::now();
    has_reload_time = true;

    // Persist the enabled state -- so if it was enabled before the reload, it'll be enabled now
    if (was_enabled)
    {
        Enable();
    }
}

void ForgeScript::RunSettingsCallback()
{
    if(!IsEnabled())
    {
        return;
    }

    if(!settings_callback)
    {
        // throw std::runtime_error(std::string("Script " + file_name + " settings callback has not been set and cannot be called").c_str());
        // Instead of throwing an error, decided just to return for now. Might still log a warning message though.
        return;
    }

    auto result = settings_callback();
    if(!result.valid())
    {
        sol::error err = result;
        throw std::runtime_error(std::string("Script " + file_name + " settings callback failed with error: " + err.what()).c_str());
    }  
}

void ForgeScript::RunDisableScriptCallback()
{
    if(!disable_script_callback)
    {
        return;
    }

    try
    {
        auto result = disable_script_callback();
        if(!result.valid())
        {
            sol::error err = result;
            PLOG_ERROR << "Script " << file_name << " disable callback failed with error: " << err.what();
        }
    }
    catch(const std::exception& err)
    {
        PLOG_ERROR << "Script " << file_name << " disable callback threw exception: " << err.what();
    }
}

void ForgeScript::Enable()
{
    enabled = true;
}

void ForgeScript::Disable()
{
    // Added this check as a "just in case" -- wouldn't want to accidentally run disable script callbacks
    // when the script is already disabled!
    if(!enabled)
    {
        return;
    }

    enabled = false;
    RunDisableScriptCallback();
}

bool ForgeScript::IsEnabled() const
{
    return enabled;
}

std::string ForgeScript::GetFileName()
{
    return file_name;
}

std::filesystem::file_time_type ForgeScript::GetLastWriteTime() const
{
    return observed_write_time;
}

std::chrono::system_clock::time_point ForgeScript::GetLastReloadTime() const
{
    return last_reload_time;
}

std::string ForgeScript::GetLastWriteTimeString() const
{
    if (!has_write_time)
    {
        return "N/A";
    }

    return FormatLocalTime(FileTimeToSystemClock(observed_write_time));
}

std::string ForgeScript::GetLastReloadTimeString() const
{
    if (!has_reload_time)
    {
        return "N/A";
    }

    return FormatLocalTime(last_reload_time);
}

bool ForgeScript::IsOutOfDateOnDisk()
{
    try
    {
        const auto current_write_time = std::filesystem::last_write_time(file_name);
        observed_write_time = current_write_time;

        if (!has_write_time)
        {
            loaded_write_time = current_write_time;
            has_write_time = true;
            return false;
        }

        return current_write_time != loaded_write_time;
    }
    catch(...)
    {
        return false;
    }
}

std::string ForgeScript::GetContents()
{
    return file_contents;
}

std::size_t ForgeScript::GetHash()
{
    return hash;
}

ForgeScript::~ForgeScript(){}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          ForgeScriptManager Class                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define FIND_SCRIPT_BY_NAME(name) [&name](const std::unique_ptr<ForgeScript>& script){ return script->GetFileName() == name;}

ForgeScriptManager::ForgeScriptManager(std::string scripts_path, lua_State* uif_lua_state)
    : scripts_path(scripts_path), uif_lua_state(uif_lua_state), currently_executing_script(nullptr)
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
    // periodically poll for file changes and queue reloads before executing the frame.
    if (reload_on_save_enabled)
    {
        const auto now = std::chrono::steady_clock::now();
        const bool should_poll = !reload_on_save_has_polled ||
            (std::chrono::duration_cast<std::chrono::milliseconds>(now - reload_on_save_last_poll).count() >= reload_on_save_poll_ms);

        if (should_poll)
        {
            reload_on_save_last_poll = now;
            reload_on_save_has_polled = true;

            for (const auto& script : scripts)
            {
                if (script->IsEnabled() && script->IsOutOfDateOnDisk())
                {
                    pending_reload.emplace(script->GetFileName());
                }
            }
        }
    }

    ProcessPendingReloads();

    for(const auto& script : scripts)
    {
        try
        {
            currently_executing_script = script.get();
            script->Run(uif_lua_state);
        }
        catch(const std::exception& err)
        {
            PLOG_ERROR << "Error running script " << script->GetFileName() << ": " << err.what();
            CoreUtils::ErrorMessageBox(err.what());
            script->Disable();
        }
    }

    currently_executing_script = nullptr;
}

void ForgeScriptManager::SetReloadOnSave(bool enabled, unsigned poll_ms)
{
    reload_on_save_enabled = enabled;
    reload_on_save_poll_ms = poll_ms;
    reload_on_save_has_polled = false;
}

void ForgeScriptManager::RequestReload(const std::string& file_name)
{
    pending_reload.emplace(file_name);
}

void ForgeScriptManager::RequestReloadAll()
{
    for (const auto& script : scripts)
    {
        pending_reload.emplace(script->GetFileName());
    }
}

void ForgeScriptManager::ProcessPendingReloads()
{
    if (pending_reload.empty())
    {
        return;
    }

    for (const auto& file_name : pending_reload)
    {
        ForgeScript* script = GetScript(file_name);
        if (!script)
        {
            continue;
        }

        try
        {
            script->Reload(uif_lua_state);
        }
        catch (const std::exception& err)
        {
            PLOG_ERROR << "Error reloading script " << file_name << ": " << err.what();
            CoreUtils::ErrorMessageBox(err.what());
            script->Disable();
        }
    }

    pending_reload.clear();
}

void ForgeScriptManager::RegisterCallback(ForgeScriptCallbackType type, sol::protected_function callback)
{
    if(!currently_executing_script)
    {
        PLOG_ERROR << "Attempted to register a script callback, but there is no currently executing script.";
        return;
    }

    if(!callback.valid())
    {
        PLOG_ERROR << "Invalid script callback for " << currently_executing_script->GetFileName();
        return;
    }

    switch(type)
    {
        case ForgeScriptCallbackType::Settings:
            currently_executing_script->settings_callback = callback;
            PLOG_DEBUG << "Registered script settings callback for " << currently_executing_script->GetFileName();
            break;

        case ForgeScriptCallbackType::DisableScript:
            currently_executing_script->disable_script_callback = callback;
            PLOG_DEBUG << "Registered script disable callback for " << currently_executing_script->GetFileName();
            break;

        default:
            PLOG_WARNING << "Unrecognized script callback type (" << static_cast<int>(type)
                         << ") attempted to be registered for " << currently_executing_script->GetFileName();
            break;
    }
}

ForgeScript* ForgeScriptManager::GetScript(const std::string file_name)
{
    auto iter = std::find_if(scripts.begin(), scripts.end(), FIND_SCRIPT_BY_NAME(file_name));
    
    if(iter != scripts.end()) return iter->get();
    
    return nullptr;
}

ForgeScript* ForgeScriptManager::GetScript(const unsigned index)
{
    try
    {
        return scripts.at(index).get();
    }
    catch (const std::out_of_range& err)
    {
        PLOG_ERROR << "Script index out of range.";
    }

    return nullptr;
}

unsigned ForgeScriptManager::GetScriptCount()
{
    return scripts.size();
}

void ForgeScriptManager::RefreshScripts()
{
    std::unordered_set<std::string> known_names;
    known_names.reserve(scripts.size());

    for (const auto& script : scripts)
    {
        known_names.emplace(std::filesystem::path(script->GetFileName()).filename().string());
    }

    for (const auto& entry : std::filesystem::directory_iterator(scripts_path))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".lua") continue;

        const std::string name = entry.path().filename().string();
        if (known_names.find(name) != known_names.end()) continue;

        AddScript(scripts_path + "\\" + name);
        known_names.emplace(name);
    }
}

void ForgeScriptManager::UpdateDebugStats()
{
    // Reset stats every time we update because we should only include the most recent information
    stats = { 0 };
    for (const auto& script : scripts)
    {
        ForgeScript* current_script = script.get();
        stats.script_size += current_script->stats.script_size;
        stats.time_to_hash_file_contents += current_script->stats.time_to_hash_file_contents;
        stats.time_to_read_file_contents += current_script->stats.time_to_read_file_contents;
        stats.times_executed += current_script->stats.times_executed;
        stats.total_time_executing += current_script->stats.total_time_executing;
        stats.total_time_loading_from_mem += current_script->stats.total_time_loading_from_mem;
    }
}

ForgeScriptManager::~ForgeScriptManager(){}
