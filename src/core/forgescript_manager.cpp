#include "pch.h"
#include "core\forgescript_manager.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                            ForgeScript Class                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

ForgeScript::ForgeScript(){}

ForgeScript::ForgeScript(const std::string file_name) : file_name(file_name), enabled(false)
{
    std::ifstream file(file_name, std::ios::in | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + file_name);
    }
    stats = { 0 };

    // Read the file contents into the file_contents string
    auto start_time = std::chrono::steady_clock::now();
    file.seekg(0, std::ios::end);
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

void ForgeScript::RunSettingsCallback()
{
    if(!IsEnabled())
    {
        return;
    }

    if(!settings_callback)
    {
        throw std::runtime_error(std::string("Script " + file_name + " settings callback has not been set and cannot be called").c_str());
    }

    auto result = settings_callback();
    if(!result.valid())
    {
        sol::error err = result;
        throw std::runtime_error(std::string("Script " + file_name + " settings callback failed with error: " + err.what()).c_str());
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

std::size_t ForgeScript::GetHash()
{
    return hash;
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
}

void ForgeScriptManager::RegisterScriptSettings(sol::protected_function callback)
{
    // Makes sure the callback is valid and that the reference to the currently executing script isn't null
    // although the case where the executing script is null should never happen.
    if (callback.valid() && currently_executing_script)
    {
        currently_executing_script->settings_callback = callback;
        PLOG_DEBUG << "Registered script settings callback for " << currently_executing_script->GetFileName();
    } 
    else 
    {
        PLOG_ERROR << "Invalid script settings callback.";
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