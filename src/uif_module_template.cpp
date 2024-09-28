#include "..\libs\imgui\imgui.h"

extern "C" __declspec(dllexport) void ShowUiMod(ImGuiContext* mod_context)
{
    ImGuiWindowFlags window_flags;                              // Put your flags for the window style here

    ImGui::SetCurrentContext(mod_context);                      // IMPORTANT! Be sure to set the context or it will crash
    ImGui::Begin("Module Template", nullptr, window_flags);                        

    // ===> Put your ImGui code here between ImGui::Begin() and ImGui::End() <===

    ImGui::End();      
}