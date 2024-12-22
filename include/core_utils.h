#pragma once
#define CONFIG_FILE "config"
#define GET_CONFIG_VAL(root_dir, val_type, val_name) scl::config_file(std::string(root_dir + "\\" + CONFIG_FILE), scl::config_file::READ).get<val_type>(val_name)
namespace CoreUtils
{
    std::string GetUiForgeRootDirectory();
    void ErrorMessageBox(const char* err_msg);
    void InfoMessageBox(const char* info_msg);
    void ProcessCustomInputs();
}
