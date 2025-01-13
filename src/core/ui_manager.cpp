#include "pch.h"
#include "core\ui_manager.h"
#include "core\graphics_api.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WNDPROC UiManager::original_wndproc = nullptr;

UiManager::UiManager(HWND target_window) : target_window(target_window), show_settings(false)
{
    InitializeImGui();
};

UiManager::~UiManager()
{
    CleanupUiManager();
}

void UiManager::InitializeImGui() 
/**
 * @brief Initializes the ImGui library for rendering UI elements.
 * 
 * This function sets up the necessary infrastructure to integrate ImGui into the existing graphics pipeline
 * 
 * @param none
 */
{
    original_wndproc = (WNDPROC)SetWindowLongPtrW(target_window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    if(!original_wndproc) throw std::runtime_error("Failed to set new address for the window procedure. Error: " + std::to_string(GetLastError()));
    
    mod_context = ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO();
    
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    if(!ImGui_ImplWin32_Init(target_window)) throw std::runtime_error("Unable to initialize ImGui Win32 Implementation.");
}

void UiManager::RenderSettingsIcon(void* settings_icon)
{
    unsigned window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("Settings Icon", nullptr, window_flags))
    {
        ImVec2 icon_size(32, 32);
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        ImVec4 icon_tint(1,1,1,0.2);    // Make the default tint transparent
        if(ImGui::InvisibleButton("##UiForge Settings Icon", icon_size))
        {
            show_settings = !show_settings;
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("UiForge Settings");
            icon_tint.w = 1;            // If the icon is hovered, we want to make the alpha 1 so it will not be transparent
        }

        ImGui::SetCursorPos(cursor_pos);
        ImGui::Image(settings_icon, icon_size, ImVec2(0,0), ImVec2(1,1), icon_tint);
    }
    ImGui::End();
}

void UiManager::RenderSettingsWindow(ForgeScriptManager& script_manager)
{
    static ForgeScript* selected_script;    // Static so the selected script stays selected

    if (ImGui::Begin("UiForge Settings"))
    {
        ImGuiStyle& style = ImGui::GetStyle();
        float line_height = ImGui::GetTextLineHeightWithSpacing();
        float checkbox_height = ImGui::GetFrameHeight();
        ImVec2 parent_window_size = ImGui::GetContentRegionAvail();
        ImVec2 forgescript_list_window_size(parent_window_size.x * 0.3f - style.ItemSpacing.x * 0.5, parent_window_size.y * 0.75f);
        ImVec2 settings_child_window_size(parent_window_size.x * 0.7f - style.ItemSpacing.x * 0.5, parent_window_size.y * 0.75f);
        
        if(ImGui::BeginChild("ForgeScript List", forgescript_list_window_size, ImGuiChildFlags_Border))
        {
            for (unsigned idx = 0; idx < script_manager.GetScriptCount(); idx++)
            {
                ForgeScript* current_script = script_manager.GetScript(idx);
                std::filesystem::path script_path(current_script->GetFileName());
                bool is_current_script_selected = (selected_script == current_script);
                if(ImGui::Selectable((std::string("##selectable_") + script_path.filename().string()).c_str(), is_current_script_selected, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, line_height)))
                {
                    // If we click on a selected script and it is already selected, then deselect
                    if (is_current_script_selected)
                    {
                        selected_script = nullptr;
                    }
                    else
                    {
                        selected_script = current_script;
                    }
                }
                ImGui::SameLine();
                ImGui::Checkbox((std::string("##toggle_") + script_path.filename().string()).c_str(), &current_script->enabled);
                ImGui::SameLine();
                ImGui::TextUnformatted(script_path.filename().string().c_str()); // Manually placing name instead of using checkbox label so that clicking the name doesn't toggle the checkbox
            }
            ImGui::EndChild();
        }

        ImGui::SameLine();

        if(ImGui::BeginChild("ForgeScript Settings", settings_child_window_size, ImGuiChildFlags_Border))
        {
            if(ImGui::BeginTabBar("ForgeScript Settings Tabs"))
            {
                if(ImGui::BeginTabItem("Settings"))
                {
                    if(selected_script)
                    {
                        try
                        {
                            selected_script->RunSettingsCallback();
                        }
                        catch(const std::exception& err)
                        {
                            selected_script->Disable();
                            PLOG_ERROR << err.what();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if(ImGui::BeginTabItem("Debug"))
                {
                    // If a script is selected, show stats about that script
                    if(selected_script)
                    {
                        size_t avg_time_loading     = (selected_script->stats.times_executed) ? selected_script->stats.total_time_loading_from_mem / selected_script->stats.times_executed : 0;
                        size_t avg_time_executing   = (selected_script->stats.times_executed) ? selected_script->stats.total_time_executing / selected_script->stats.times_executed : 0;
                        ImGui::Text("File Name                                  : %s", selected_script->GetFileName().c_str());
                        ImGui::Text("File Contents Size                         : %llu bytes", selected_script->stats.script_size);
                        ImGui::Text("Time to Read File                          : %llu microseconds", selected_script->stats.time_to_read_file_contents);
                        ImGui::Text("Time to Hash File                          : %llu microseconds", selected_script->stats.time_to_hash_file_contents);
                        ImGui::Text("Avg Time Loading Script From Memory        : %llu microseconds", avg_time_loading);
                        ImGui::Text("Avg Time Executing                         : %llu microseconds", avg_time_executing);
                        ImGui::Text("Number of Times Script Executed            : %llu", selected_script->stats.times_executed);
                    }

                    // If no script is selected, show total stats
                    if(!selected_script)
                    {
                        script_manager.UpdateDebugStats();
                        size_t avg_time_loading     = (script_manager.stats.times_executed) ? script_manager.stats.total_time_loading_from_mem / script_manager.stats.times_executed : 0;
                        size_t avg_time_executing   = (script_manager.stats.times_executed) ? script_manager.stats.total_time_executing / script_manager.stats.times_executed : 0;
                        ImGui::Text("Total File Contents Size                   : %llu bytes", script_manager.stats.script_size);
                        ImGui::Text("Time Reading Files                         : %llu microseconds", script_manager.stats.time_to_read_file_contents);
                        ImGui::Text("Time Hashing Files                         : %llu microseconds", script_manager.stats.time_to_hash_file_contents);
                        ImGui::Text("Avg Time Loading Scripts From Memory       : %llu microseconds", avg_time_loading);
                        ImGui::Text("Avg Time Executing Scripts                 : %llu microseconds", avg_time_executing);
                        ImGui::Text("Number of Times Scripts Executed           : %llu", script_manager.stats.times_executed);                        
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void UiManager::RenderUiElements(ForgeScriptManager& script_manager, void* settings_icon)
{
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // Execute all UI Mods
    // CreateTestWindow();
    RenderSettingsIcon(settings_icon);
    if(show_settings) RenderSettingsWindow(script_manager);
    script_manager.RunScripts();

    ImGui::Render();
}

void UiManager::CleanupUiManager()
{
    if(mod_context)
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(mod_context);
    }


    if(original_wndproc)
    {
        if(!SetWindowLongPtrW(target_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wndproc)))
        {
            std::string err_msg = std::string("Failed to restore original window procedure. Error: ") + std::to_string(GetLastError());
            // TODO: Log error message
        }
    }
}

LRESULT WINAPI UiManager::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    return CallWindowProcW(original_wndproc, hWnd, msg, wParam, lParam);
}

void UiManager::CreateTestWindow()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    ImGui::Begin("Module Template", nullptr, window_flags);                        

    // Text label
    ImGui::Text("Hello, world!");

    // Button
    if (ImGui::Button("Click Me!")) {
        ImGui::Text("Button Clicked!");
    }

    // Checkbox
    static bool checkboxValue = false;
    ImGui::Checkbox("Checkbox", &checkboxValue);

    // Radio Buttons
    static int radioButtonValue = 0;
    ImGui::RadioButton("Radio A", &radioButtonValue, 0);
    ImGui::RadioButton("Radio B", &radioButtonValue, 1);
    ImGui::RadioButton("Radio C", &radioButtonValue, 2);

    // Slider
    static float sliderValue = 0.0f;
    ImGui::SliderFloat("Slider", &sliderValue, 0.0f, 1.0f);

    // Color Picker
    static float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    ImGui::ColorEdit4("Color Picker", color);

    // Input Text
    static char inputText[128] = "Enter text here...";
    ImGui::InputText("Input Text", inputText, IM_ARRAYSIZE(inputText));

    // Combo Box
    const char* items[] = { "Item 1", "Item 2", "Item 3", "Item 4" };
    static int itemIndex = 0;
    ImGui::Combo("Combo Box", &itemIndex, items, IM_ARRAYSIZE(items));

    // List Box
    static int listBoxItemIndex = 0;
    ImGui::ListBox("List Box", &listBoxItemIndex, items, IM_ARRAYSIZE(items), 4);

    // Tooltip
    ImGui::Text("Hover over me!");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This is a tooltip");
    }

    // Tree View
    if (ImGui::TreeNode("Tree Node")) {
        ImGui::Text("Tree Item 1");
        ImGui::Text("Tree Item 2");
        ImGui::TreePop();
    }

    // Collapsible Header
    if (ImGui::CollapsingHeader("Collapsible Header")) {
        ImGui::Text("Collapsible content here.");
    }

    // End the ImGui window
    ImGui::End();  
}