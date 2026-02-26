#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
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

#include <windows.h>

#include <imgui.h>
#include <plog/Log.h>

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
    save_callback = sol::protected_function();
    load_callback = sol::protected_function();
    on_eject_callback = sol::protected_function();

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

void ForgeScript::RunOnEjectCallback()
{
    if(!on_eject_callback)
    {
        return;
    }

    try
    {
        auto result = on_eject_callback();
        if(!result.valid())
        {
            sol::error err = result;
            PLOG_ERROR << "Script " << file_name << " on-eject callback failed with error: " << err.what();
        }
    }
    catch(const std::exception& err)
    {
        PLOG_ERROR << "Script " << file_name << " on-eject callback threw exception: " << err.what();
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

    // A profile apply is delivered here, after the profile's scripts have executed once.
    // That first pass is what registers their Load callbacks and creates their windows,
    // both of which must exist before saved state and window positions can be applied.
    if (has_pending_profile_apply)
    {
        ApplyPendingProfileState();
    }
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

    // Drop cached user modules so the reloaded scripts pick up module edits too.
    PurgeUserModuleCache();

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

        case ForgeScriptCallbackType::Save:
            currently_executing_script->save_callback = callback;
            PLOG_DEBUG << "Registered script save callback for " << currently_executing_script->GetFileName();
            break;

        case ForgeScriptCallbackType::Load:
            currently_executing_script->load_callback = callback;
            PLOG_DEBUG << "Registered script load callback for " << currently_executing_script->GetFileName();
            break;

        case ForgeScriptCallbackType::OnEject:
            currently_executing_script->on_eject_callback = callback;
            PLOG_DEBUG << "Registered script on-eject callback for " << currently_executing_script->GetFileName();
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

void ForgeScriptManager::SetProfilesDirectory(const std::string& directory_path)
{
    profiles_path = directory_path;
}

void ForgeScriptManager::SetModulesDirectory(const std::string& directory_path)
{
    modules_path = directory_path;
}

void ForgeScriptManager::PurgeUserModuleCache()
{
    if (modules_path.empty())
    {
        return;
    }

    sol::state_view lua(uif_lua_state);
    sol::object loaded_obj = lua["package"]["loaded"];
    if (!loaded_obj.is<sol::table>())
    {
        return;
    }
    sol::table loaded = loaded_obj.as<sol::table>();

    // Collect first, purge after -- removing keys while iterating a Lua table is undefined.
    std::vector<std::string> to_purge;
    for (const auto& entry : loaded)
    {
        if (!entry.first.is<std::string>())
        {
            continue;
        }

        // Resolve the module name the same way require() does with our package.path
        // patterns ("<modules>\?.lua" and "<modules>\?\?.lua"): dots become path
        // separators. Only names backed by a real file under the modules directory
        // are user modules; everything else (built-ins, embedded serpent) is kept.
        std::string relative = entry.first.as<std::string>();
        std::replace(relative.begin(), relative.end(), '.', '\\');

        const std::filesystem::path direct = std::filesystem::path(modules_path) / (relative + ".lua");
        const std::filesystem::path nested = std::filesystem::path(modules_path) / relative / (relative + ".lua");

        std::error_code ec;
        if (std::filesystem::exists(direct, ec) || std::filesystem::exists(nested, ec))
        {
            to_purge.push_back(entry.first.as<std::string>());
        }
    }

    for (const auto& module_name : to_purge)
    {
        loaded[module_name] = sol::lua_nil;
        PLOG_DEBUG << "Purged cached module '" << module_name << "' so the next require() re-reads it from disk.";
    }
}

std::string ForgeScriptManager::GetProfileFilePath(const std::string& profile_name) const
{
    return (std::filesystem::path(profiles_path) / (profile_name + ".profile.lua")).string();
}

std::string ForgeScriptManager::GetPreferredProfilesFilePath() const
{
    return (std::filesystem::path(profiles_path) / "preferred_profiles.lua").string();
}

std::string ForgeScriptManager::GetCurrentProcessName()
{
    char module_path[MAX_PATH] = { 0 };
    GetModuleFileNameA(nullptr, module_path, MAX_PATH);

    std::string process_name = std::filesystem::path(module_path).filename().string();
    std::transform(process_name.begin(), process_name.end(), process_name.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return process_name;
}

// Profile names double as file stems, so reject anything that could escape the
// profiles directory or produce an invalid Windows file name.
static bool IsValidProfileName(const std::string& name)
{
    if (name.empty() || name.size() > 64)
    {
        return false;
    }

    const std::string forbidden = "\\/:*?\"<>|";
    for (const char c : name)
    {
        if (c < 0x20 || forbidden.find(c) != std::string::npos)
        {
            return false;
        }
    }

    // Trailing dots/spaces are silently stripped by Windows and would break round-tripping.
    const char last = name.back();
    return last != '.' && last != ' ';
}

sol::object ForgeScriptManager::LoadDataFile(const std::string& file_path) const
{
    sol::state_view lua(uif_lua_state);

    try
    {
        std::ifstream in(file_path, std::ios::binary);
        if (!in)
        {
            PLOG_ERROR << "Failed to open " << file_path << " for reading.";
            return sol::lua_nil;
        }
        std::string contents{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };

        sol::load_result chunk = lua.load(contents, "@" + file_path);
        if (!chunk.valid())
        {
            sol::error err = chunk;
            PLOG_ERROR << "Failed to parse " << file_path << ": " << err.what();
            return sol::lua_nil;
        }

        // These files are plain data. Run them in an empty environment so a tampered
        // file cannot reach globals or call into the rest of the Lua state.
        sol::protected_function chunk_function = chunk;
        sol::environment sandbox(lua, sol::create);
        sol::set_environment(sandbox, chunk_function);

        auto chunk_result = chunk_function();
        if (!chunk_result.valid())
        {
            sol::error err = chunk_result;
            PLOG_ERROR << "Failed to execute " << file_path << ": " << err.what();
            return sol::lua_nil;
        }

        return chunk_result;
    }
    catch (const std::exception& err)
    {
        PLOG_ERROR << "Exception while reading " << file_path << ": " << err.what();
        return sol::lua_nil;
    }
}

bool ForgeScriptManager::WriteDataFileAtomic(const std::string& file_path, sol::object data) const
{
    sol::state_view lua(uif_lua_state);

    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(file_path).parent_path(), ec);
    if (ec)
    {
        PLOG_ERROR << "Failed to create directory for " << file_path << ": " << ec.message();
        return false;
    }

    sol::object serpent_module = lua["package"]["loaded"]["serpent"];
    if (!serpent_module.is<sol::table>())
    {
        PLOG_ERROR << "serpent module is not loaded; cannot write " << file_path;
        return false;
    }
    sol::protected_function serialize = serpent_module.as<sol::table>()["block"];

    sol::table serialize_options = lua.create_table();
    serialize_options["comment"] = false;
    auto serialize_result = serialize(data, serialize_options);
    if (!serialize_result.valid())
    {
        sol::error err = serialize_result;
        PLOG_ERROR << "Failed to serialize data for " << file_path << ": " << err.what();
        return false;
    }

    const std::string temp_file = file_path + ".tmp";

    // Write to a temp file and rename over the real one so a crash mid-write
    // can never corrupt an existing file.
    {
        std::ofstream out(temp_file, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            PLOG_ERROR << "Failed to open " << temp_file << " for writing.";
            return false;
        }
        out << "return " << serialize_result.get<std::string>() << "\n";
    }

    std::filesystem::rename(temp_file, file_path, ec);
    if (ec)
    {
        PLOG_ERROR << "Failed to move " << temp_file << " into place: " << ec.message();
        std::filesystem::remove(temp_file, ec);
        return false;
    }

    return true;
}

void ForgeScriptManager::SaveProfile(const std::string& profile_name)
{
    if (profiles_path.empty())
    {
        PLOG_WARNING << "SaveProfile called but no profiles directory is configured.";
        return;
    }

    if (!IsValidProfileName(profile_name))
    {
        PLOG_ERROR << "Invalid profile name: '" << profile_name << "'";
        return;
    }

    sol::state_view lua(uif_lua_state);
    sol::table profile = lua.create_table();
    sol::table profile_scripts = lua.create_table();

    for (const auto& script : scripts)
    {
        if (!script->IsEnabled())
        {
            continue;
        }

        const std::string script_name = std::filesystem::path(script->GetFileName()).filename().string();
        sol::table entry = lua.create_table();

        if (script->save_callback)
        {
            try
            {
                auto callback_result = script->save_callback();
                if (!callback_result.valid())
                {
                    sol::error err = callback_result;
                    PLOG_ERROR << "Script " << script->GetFileName() << " save callback failed with error: " << err.what();
                }
                else
                {
                    sol::object state = callback_result;
                    if (state.is<sol::table>())
                    {
                        entry["state"] = state;
                    }
                    else if (state != sol::lua_nil)
                    {
                        PLOG_WARNING << "Script " << script->GetFileName() << " save callback returned a non-table value; state not saved.";
                    }
                }
            }
            catch (const std::exception& err)
            {
                PLOG_ERROR << "Exception while saving state for " << script->GetFileName() << ": " << err.what();
            }
        }

        // The script is recorded in the profile (and will be re-enabled on apply) even
        // when it has no saved state.
        profile_scripts[script_name] = entry;
    }

    profile["scripts"] = profile_scripts;

    // Capture every window's position/size/collapsed state so applying the profile
    // can put the UI back exactly where the user arranged it.
    const char* window_settings = ImGui::SaveIniSettingsToMemory(nullptr);
    profile["window_settings"] = std::string(window_settings ? window_settings : "");

    const std::string profile_file = GetProfileFilePath(profile_name);
    if (WriteDataFileAtomic(profile_file, profile))
    {
        current_profile_name = profile_name;
        PLOG_INFO << "Saved profile '" << profile_name << "' to " << profile_file;
    }
}

void ForgeScriptManager::ApplyProfile(const std::string& profile_name)
{
    if (profiles_path.empty())
    {
        PLOG_WARNING << "ApplyProfile called but no profiles directory is configured.";
        return;
    }

    const std::string profile_file = GetProfileFilePath(profile_name);
    if (!IsValidProfileName(profile_name) || !std::filesystem::exists(profile_file))
    {
        PLOG_ERROR << "Profile '" << profile_name << "' does not exist.";
        return;
    }

    sol::object profile_obj = LoadDataFile(profile_file);
    if (!profile_obj.is<sol::table>())
    {
        PLOG_ERROR << "Profile file " << profile_file << " did not return a table; not applied.";
        return;
    }

    sol::object scripts_obj = profile_obj.as<sol::table>()["scripts"];
    if (!scripts_obj.is<sol::table>())
    {
        PLOG_ERROR << "Profile file " << profile_file << " has no scripts table; not applied.";
        return;
    }
    sol::table profile_scripts = scripts_obj.as<sol::table>();

    // Enable exactly the scripts the profile lists, disabling everything else.
    // Disable() runs each script's disable callback so the outgoing set can clean up.
    for (const auto& script : scripts)
    {
        const std::string script_name = std::filesystem::path(script->GetFileName()).filename().string();
        const bool should_enable = profile_scripts[script_name].valid() && profile_scripts[script_name].get_type() == sol::type::table;

        if (should_enable && !script->IsEnabled())
        {
            script->Enable();
        }
        else if (!should_enable && script->IsEnabled())
        {
            script->Disable();
        }
    }

    // Saved state and window positions cannot be applied yet. Newly enabled scripts
    // have to run once first to register their Load callbacks and create their windows,
    // so the rest of the apply is queued for the end of the next RunScripts() pass.
    pending_profile_scripts = scripts_obj;
    sol::object window_settings = profile_obj.as<sol::table>()["window_settings"];
    pending_window_settings = window_settings.is<std::string>() ? window_settings.as<std::string>() : std::string();
    has_pending_profile_apply = true;

    current_profile_name = profile_name;
    PLOG_INFO << "Applying profile '" << profile_name << "'";
}

void ForgeScriptManager::ApplyPendingProfileState()
{
    has_pending_profile_apply = false;

    if (pending_profile_scripts.is<sol::table>())
    {
        sol::table profile_scripts = pending_profile_scripts.as<sol::table>();

        for (const auto& script : scripts)
        {
            if (!script->IsEnabled() || !script->load_callback)
            {
                continue;
            }

            const std::string script_name = std::filesystem::path(script->GetFileName()).filename().string();
            sol::object entry = profile_scripts[script_name];
            if (!entry.is<sol::table>())
            {
                continue;
            }

            sol::object state = entry.as<sol::table>()["state"];
            if (!state.is<sol::table>())
            {
                continue;
            }

            try
            {
                auto callback_result = script->load_callback(state);
                if (!callback_result.valid())
                {
                    sol::error err = callback_result;
                    PLOG_ERROR << "Script " << script->GetFileName() << " load callback failed with error: " << err.what();
                    continue;
                }
                PLOG_INFO << "Restored profile state for " << script->GetFileName();
            }
            catch (const std::exception& err)
            {
                PLOG_ERROR << "Exception while restoring state for " << script->GetFileName() << ": " << err.what();
            }
        }
    }

    if (!pending_window_settings.empty())
    {
        ApplyWindowSettings(pending_window_settings);
    }

    pending_profile_scripts = sol::object(sol::lua_nil);
    pending_window_settings.clear();
}

void ForgeScriptManager::ApplyWindowSettings(const std::string& ini_blob)
{
    // Hand the blob back to ImGui so windows created after this point read their
    // saved placement on creation.
    ImGui::LoadIniSettingsFromMemory(ini_blob.c_str(), ini_blob.size());

    // ImGui only consults ini settings when a window is created, so windows that
    // already exist are repositioned explicitly. The blob format is a sequence of
    // "[Window][<name>]" sections holding "Pos=x,y", "Size=w,h", "Collapsed=0/1".
    std::istringstream stream(ini_blob);
    std::string line;
    std::string window_name;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (!line.empty() && line.front() == '[')
        {
            // Section header. Only "[Window][<name>]" sections are interesting.
            window_name.clear();
            const std::string window_prefix = "[Window][";
            if (line.compare(0, window_prefix.size(), window_prefix) == 0 && line.back() == ']')
            {
                window_name = line.substr(window_prefix.size(), line.size() - window_prefix.size() - 1);
            }
            continue;
        }

        if (window_name.empty())
        {
            continue;
        }

        float x = 0.0f;
        float y = 0.0f;
        int collapsed = 0;
        if (sscanf_s(line.c_str(), "Pos=%f,%f", &x, &y) == 2)
        {
            ImGui::SetWindowPos(window_name.c_str(), ImVec2(x, y), ImGuiCond_Always);
        }
        else if (sscanf_s(line.c_str(), "Size=%f,%f", &x, &y) == 2)
        {
            ImGui::SetWindowSize(window_name.c_str(), ImVec2(x, y), ImGuiCond_Always);
        }
        else if (sscanf_s(line.c_str(), "Collapsed=%d", &collapsed) == 1)
        {
            ImGui::SetWindowCollapsed(window_name.c_str(), collapsed != 0, ImGuiCond_Always);
        }
    }
}

std::vector<std::string> ForgeScriptManager::ListProfiles() const
{
    std::vector<std::string> profile_names;

    if (profiles_path.empty() || !std::filesystem::exists(profiles_path))
    {
        return profile_names;
    }

    const std::string profile_suffix = ".profile.lua";
    for (const auto& entry : std::filesystem::directory_iterator(profiles_path))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::string file_name = entry.path().filename().string();
        if (file_name.size() > profile_suffix.size() &&
            file_name.compare(file_name.size() - profile_suffix.size(), profile_suffix.size(), profile_suffix) == 0)
        {
            profile_names.push_back(file_name.substr(0, file_name.size() - profile_suffix.size()));
        }
    }

    std::sort(profile_names.begin(), profile_names.end());
    return profile_names;
}

std::string ForgeScriptManager::GetCurrentProfileName() const
{
    return current_profile_name;
}

std::string ForgeScriptManager::GetPreferredProfileName() const
{
    if (profiles_path.empty())
    {
        return std::string();
    }

    const std::string preferred_file = GetPreferredProfilesFilePath();
    if (!std::filesystem::exists(preferred_file))
    {
        return std::string();
    }

    sol::object preferred_obj = LoadDataFile(preferred_file);
    if (!preferred_obj.is<sol::table>())
    {
        return std::string();
    }

    sol::object profile_name = preferred_obj.as<sol::table>()[GetCurrentProcessName()];
    return profile_name.is<std::string>() ? profile_name.as<std::string>() : std::string();
}

void ForgeScriptManager::SetPreferredProfile(const std::string& profile_name)
{
    if (profiles_path.empty())
    {
        PLOG_WARNING << "SetPreferredProfile called but no profiles directory is configured.";
        return;
    }

    if (!IsValidProfileName(profile_name))
    {
        PLOG_ERROR << "Invalid profile name: '" << profile_name << "'";
        return;
    }

    sol::state_view lua(uif_lua_state);

    // Preserve the preferred profiles of other processes by loading the existing map first.
    const std::string preferred_file = GetPreferredProfilesFilePath();
    sol::table preferred = lua.create_table();
    if (std::filesystem::exists(preferred_file))
    {
        sol::object existing = LoadDataFile(preferred_file);
        if (existing.is<sol::table>())
        {
            preferred = existing.as<sol::table>();
        }
    }

    const std::string process_name = GetCurrentProcessName();
    preferred[process_name] = profile_name;

    if (WriteDataFileAtomic(preferred_file, preferred))
    {
        PLOG_INFO << "Set preferred profile for " << process_name << " to '" << profile_name << "'";
    }
}

void ForgeScriptManager::ClearPreferredProfile()
{
    if (profiles_path.empty())
    {
        return;
    }

    const std::string preferred_file = GetPreferredProfilesFilePath();
    if (!std::filesystem::exists(preferred_file))
    {
        return;
    }

    sol::object existing = LoadDataFile(preferred_file);
    if (!existing.is<sol::table>())
    {
        return;
    }

    sol::table preferred = existing.as<sol::table>();
    const std::string process_name = GetCurrentProcessName();
    preferred[process_name] = sol::lua_nil;

    if (WriteDataFileAtomic(preferred_file, preferred))
    {
        PLOG_INFO << "Cleared preferred profile for " << process_name;
    }
}

void ForgeScriptManager::ApplyPreferredProfile()
{
    const std::string profile_name = GetPreferredProfileName();
    if (profile_name.empty())
    {
        PLOG_DEBUG << "No preferred profile configured for " << GetCurrentProcessName();
        return;
    }

    if (!std::filesystem::exists(GetProfileFilePath(profile_name)))
    {
        PLOG_WARNING << "Preferred profile '" << profile_name << "' for " << GetCurrentProcessName()
                     << " no longer exists; skipping.";
        return;
    }

    PLOG_INFO << "Auto-applying preferred profile '" << profile_name << "' for " << GetCurrentProcessName();
    ApplyProfile(profile_name);
}

void ForgeScriptManager::RunOnEjectCallbacks()
{
    for (const auto& script : scripts)
    {
        script->RunOnEjectCallback();
    }
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
