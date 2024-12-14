#include "imgui.h"

// MUST include this with your imgui build if you want the scripts to work.

extern "C" {

    // Have to pass in a pointer to a vec because you cannot make user-defined types (such as ImVec2)
    // external
    IMGUI_API void NewImVec2(float x, float y, ImVec2* out) {
        if (out) {
            *out = ImVec2(x, y); // Populate the struct directly
        }
    }

    // Create a new ImVec4 by populating the out parameter
    IMGUI_API void NewImVec4(float a, float b, float c, float d, ImVec4* out) {
        if (out) {
            *out = ImVec4(a, b, c, d); // Populate the struct directly
        }
    }

    // SetCurrentContext
    IMGUI_API void SetCurrentContext(ImGuiContext* ctx) {
        ImGui::SetCurrentContext(ctx);
    }

    // Begin
    IMGUI_API bool Begin(const char* name, bool* p_open, int flags) {
        return ImGui::Begin(name, p_open, flags);
    }

    // End
    IMGUI_API void End() {
        ImGui::End();
    }

    // Text
    IMGUI_API void Text(const char* fmt) {
        ImGui::Text("%s", fmt);
    }

    // Button
    IMGUI_API bool Button(const char* label) {
        return ImGui::Button(label);
    }

    // Checkbox
    IMGUI_API bool Checkbox(const char* label, bool* v) {
        return ImGui::Checkbox(label, v);
    }

    // // Image
    // void Image(void* texture_id, ImVec2 size, ImVec2 uv0, ImVec2 uv1) {
    //     ImGui::Image(texture_id, size, uv0, uv1);
    // }

    // // ImageButton
    // bool ImageButton(const char* str_id, void* texture_id, ImVec2 size, ImVec2 uv0, ImVec2 uv1, int frame_padding, ImVec4 bg_col, ImVec4 tint_col) {
    //     return ImGui::ImageButton(str_id, texture_id, size, uv0, uv1, frame_padding, bg_col, tint_col);
    // }

    // SliderFloat
    IMGUI_API bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, float power) {
        return ImGui::SliderFloat(label, v, v_min, v_max, format, power);
    }

    // SliderInt
    IMGUI_API bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format) {
        return ImGui::SliderInt(label, v, v_min, v_max, format);
    }

    // RadioButton
    IMGUI_API bool RadioButton(const char* label, bool active) {
        return ImGui::RadioButton(label, active);
    }

    // ProgressBar
    IMGUI_API void ProgressBar(float fraction, ImVec2 size_arg, const char* overlay) {
        ImGui::ProgressBar(fraction, size_arg, overlay);
    }

    // InputText
    IMGUI_API bool InputText(const char* label, char* buf, size_t buf_size, int flags, void* callback, void* user_data) {
        return ImGui::InputText(label, buf, buf_size, flags, (ImGuiInputTextCallback)callback, user_data);
    }

    // ColorEdit4
    IMGUI_API bool ColorEdit4(const char* label, float* col, int flags) {
        return ImGui::ColorEdit4(label, col, flags);
    }

    // Combo
    IMGUI_API bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items) {
        return ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
    }

    // IsItemHovered
    IMGUI_API bool IsItemHovered() {
        return ImGui::IsItemHovered();
    }

    // SetTooltip
    IMGUI_API void SetTooltip(const char* fmt) {
        ImGui::SetTooltip("%s", fmt);
    }

    // TreeNode
    IMGUI_API bool TreeNode(const char* label) {
        return ImGui::TreeNode(label);
    }

    // TreePop
    IMGUI_API void TreePop() {
        ImGui::TreePop();
    }

    // CollapsingHeader
    IMGUI_API bool CollapsingHeader(const char* label, int flags) {
        return ImGui::CollapsingHeader(label, flags);
    }

    // PushStyleColor
    IMGUI_API void PushStyleColor(int idx, ImVec4 col) {
        ImGui::PushStyleColor(idx, col);
    }

    // PopStyleColor
    IMGUI_API void PopStyleColor(int count) {
        ImGui::PopStyleColor(count);
    }

    // SameLine
    IMGUI_API void SameLine(float offset_from_start_x, float spacing) {
        ImGui::SameLine(offset_from_start_x, spacing);
    }
}
