/*
 * @file    imgui_lua_bindings.cpp
 * @version 0.1.0
 * @brief   Lua bindings for ImGui. Provides an interface to use basic ImGui functionality in Lua scripts.  
 * 
 * @note    Heads-Up!
 *
 *          Just a quick note about these Lua bindings for ImGui -- They were mostly 
 *          auto-generated. I've done minimal testing, just enough to see them each
 *          work with a basic case. They *should* work, but there might be some
 *          weird edge cases.
 *
 *          Got Issues?
 *          If something breaks or acts weird, feel free to fix it or let me know! Feedback, 
 *          fixes, and ideas are always welcome to make this better for everyone.
 * 
 * @author  mmvest (wereox)
 */


#include "..\..\include\imgui.h"
#include "..\..\include\lua.hpp"

// Helper function to push a boolean to Lua stack
void PushBooleanToLua(lua_State* L, bool value) {
    lua_pushboolean(L, value ? 1 : 0);
}

// Binding for ImGui::SetCurrentContext
int Lua_SetCurrentContext(lua_State* L) {
    // Get the pointer to the ImGuiContext from Lua
    ImGuiContext* ctx = reinterpret_cast<ImGuiContext*>(lua_touserdata(L, 1));
    if (!ctx) {
        luaL_error(L, "Invalid context passed to SetCurrentContext");
        return 0;
    }

    // Set the current ImGui context
    ImGui::SetCurrentContext(ctx);
    return 0; // No return value
}

// Binding for ImGui::Begin
int Lua_Begin(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    bool open = true;
    bool* p_open = nullptr;
    if (!lua_isnoneornil(L, 2)) {
        open = lua_toboolean(L, 2);
        p_open = &open;
    }
    ImGuiWindowFlags flags = 0;
    if (!lua_isnoneornil(L, 3)) {
        flags = static_cast<ImGuiWindowFlags>(luaL_checkinteger(L, 3));
    }

    bool result = ImGui::Begin(name, p_open, flags);
    lua_pushboolean(L, result);

    if (p_open) {
        lua_pushboolean(L, *p_open);
        return 2; // Return both the result and the updated p_open value
    }
    return 1; // Return only the result
}

// Binding for ImGui::End
int Lua_End(lua_State* L) {
    ImGui::End();
    return 0; // No values returned
}

// Binding for ImGui::Text
int Lua_Text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    ImGui::Text("%s", text);
    return 0; // No values returned
}

// Binding for ImGui::Button
int Lua_Button(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    ImVec2 size = ImVec2(0, 0);
    if (lua_gettop(L) > 1) {
        size.x = static_cast<float>(luaL_checknumber(L, 2));
        size.y = static_cast<float>(luaL_checknumber(L, 3));
    }
    bool result = ImGui::Button(label, size);
    lua_pushboolean(L, result);
    return 1; // Return the result
}

// Binding for ImGui::Checkbox
int Lua_Checkbox(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    bool value = lua_toboolean(L, 2);
    bool* v_ptr = &value;
    bool result = ImGui::Checkbox(label, v_ptr);
    lua_pushboolean(L, result);
    lua_pushboolean(L, *v_ptr); // Return the updated value
    return 2; // Return the result and updated value
}

// Binding for ImGui::Image
int Lua_Image(lua_State* L) {
    ImTextureID texture_id = reinterpret_cast<ImTextureID>(lua_touserdata(L, 1));
    ImVec2 image_size = ImVec2(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
    ImVec2 uv0 = ImVec2(0, 0), uv1 = ImVec2(1, 1);
    ImVec4 tint_col = ImVec4(1, 1, 1, 1), border_col = ImVec4(0, 0, 0, 0);

    if (lua_gettop(L) > 3) {
        uv0.x = luaL_optnumber(L, 4, 0);
        uv0.y = luaL_optnumber(L, 5, 0);
        uv1.x = luaL_optnumber(L, 6, 1);
        uv1.y = luaL_optnumber(L, 7, 1);
        tint_col.x = luaL_optnumber(L, 8, 1);
        tint_col.y = luaL_optnumber(L, 9, 1);
        tint_col.z = luaL_optnumber(L, 10, 1);
        tint_col.w = luaL_optnumber(L, 11, 1);
        border_col.x = luaL_optnumber(L, 12, 0);
        border_col.y = luaL_optnumber(L, 13, 0);
        border_col.z = luaL_optnumber(L, 14, 0);
        border_col.w = luaL_optnumber(L, 15, 0);
    }

    ImGui::Image(texture_id, image_size, uv0, uv1, tint_col, border_col);
    return 0; // No return value
}

// Binding for ImGui::ImageButton
int Lua_ImageButton(lua_State* L) {
    const char* str_id = luaL_checkstring(L, 1);
    ImTextureID texture_id = reinterpret_cast<ImTextureID>(lua_touserdata(L, 2));
    ImVec2 image_size = ImVec2(luaL_checknumber(L, 3), luaL_checknumber(L, 4));
    ImVec2 uv0 = ImVec2(0, 0), uv1 = ImVec2(1, 1);
    ImVec4 bg_col = ImVec4(0, 0, 0, 0), tint_col = ImVec4(1, 1, 1, 1);

    if (lua_gettop(L) > 4) {
        uv0.x = luaL_optnumber(L, 5, 0);
        uv0.y = luaL_optnumber(L, 6, 0);
        uv1.x = luaL_optnumber(L, 7, 1);
        uv1.y = luaL_optnumber(L, 8, 1);
        bg_col.x = luaL_optnumber(L, 9, 0);
        bg_col.y = luaL_optnumber(L, 10, 0);
        bg_col.z = luaL_optnumber(L, 11, 0);
        bg_col.w = luaL_optnumber(L, 12, 0);
        tint_col.x = luaL_optnumber(L, 13, 1);
        tint_col.y = luaL_optnumber(L, 14, 1);
        tint_col.z = luaL_optnumber(L, 15, 1);
        tint_col.w = luaL_optnumber(L, 16, 1);
    }

    bool result = ImGui::ImageButton(str_id, texture_id, image_size, uv0, uv1, bg_col, tint_col);
    lua_pushboolean(L, result);
    return 1; // Return the result
}

// Binding for ImGui::SliderFloat
int Lua_SliderFloat(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float value = static_cast<float>(luaL_checknumber(L, 2));
    float min_value = static_cast<float>(luaL_checknumber(L, 3));
    float max_value = static_cast<float>(luaL_checknumber(L, 4));
    const char* format = luaL_optstring(L, 5, "%.3f");
    ImGuiSliderFlags flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(L, 6, 0));

    bool result = ImGui::SliderFloat(label, &value, min_value, max_value, format, flags);
    lua_pushboolean(L, result);
    lua_pushnumber(L, value); // Return the updated value
    return 2; // Return the result and updated value
}

// SliderInt
int Lua_SliderInt(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int value = luaL_checkinteger(L, 2);
    int min_value = luaL_checkinteger(L, 3);
    int max_value = luaL_checkinteger(L, 4);
    const char* format = luaL_optstring(L, 5, "%d");
    ImGuiSliderFlags flags = static_cast<ImGuiSliderFlags>(luaL_optinteger(L, 6, 0));

    bool result = ImGui::SliderInt(label, &value, min_value, max_value, format, flags);
    lua_pushboolean(L, result);
    lua_pushinteger(L, value);
    return 2;
}

// RadioButton
int Lua_RadioButton(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int value = luaL_checkinteger(L, 2);
    int button_value = luaL_checkinteger(L, 3);

    bool result = ImGui::RadioButton(label, &value, button_value);
    lua_pushboolean(L, result);
    lua_pushinteger(L, value);
    return 2;
}

// ProgressBar
int Lua_ProgressBar(lua_State* L) {
    float fraction = static_cast<float>(luaL_checknumber(L, 1));
    ImVec2 size = ImVec2(-FLT_MIN, 0);
    if (lua_gettop(L) > 1) {
        size.x = static_cast<float>(luaL_optnumber(L, 2, -FLT_MIN));
        size.y = static_cast<float>(luaL_optnumber(L, 3, 0));
    }
    const char* overlay = luaL_optstring(L, 4, nullptr);

    ImGui::ProgressBar(fraction, size, overlay);
    return 0;
}

// InputText
int Lua_InputText(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    size_t buf_size = luaL_checkinteger(L, 3);
    char* buf = new char[buf_size]();
    const char* initial_value = luaL_optstring(L, 2, "");
    strncpy(buf, initial_value, buf_size - 1);

    ImGuiInputTextFlags flags = static_cast<ImGuiInputTextFlags>(luaL_optinteger(L, 4, 0));
    bool result = ImGui::InputText(label, buf, buf_size, flags);

    lua_pushboolean(L, result);
    lua_pushstring(L, buf);
    delete[] buf;
    return 2;
}

// ColorEdit4
int Lua_ColorEdit4(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    float color[4] = {
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_checknumber(L, 5)),
    };
    ImGuiColorEditFlags flags = static_cast<ImGuiColorEditFlags>(luaL_optinteger(L, 6, 0));

    bool result = ImGui::ColorEdit4(label, color, flags);
    lua_pushboolean(L, result);
    lua_pushnumber(L, color[0]);
    lua_pushnumber(L, color[1]);
    lua_pushnumber(L, color[2]);
    lua_pushnumber(L, color[3]);
    return 5;
}

// Combo
int Lua_Combo(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    int current_item = luaL_checkinteger(L, 2);

    luaL_checktype(L, 3, LUA_TTABLE);
    int items_count = lua_rawlen(L, 3);

    const char** items = new const char*[items_count];
    for (int i = 0; i < items_count; i++) {
        lua_rawgeti(L, 3, i + 1);
        items[i] = luaL_checkstring(L, -1);
        lua_pop(L, 1);
    }

    int popup_max_height = luaL_optinteger(L, 4, -1);
    bool result = ImGui::Combo(label, &current_item, items, items_count, popup_max_height);

    lua_pushboolean(L, result);
    lua_pushinteger(L, current_item);

    delete[] items;
    return 2;
}

// IsItemHovered
int Lua_IsItemHovered(lua_State* L) {
    ImGuiHoveredFlags flags = static_cast<ImGuiHoveredFlags>(luaL_optinteger(L, 1, 0));
    lua_pushboolean(L, ImGui::IsItemHovered(flags));
    return 1;
}

// SetTooltip
int Lua_SetTooltip(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    ImGui::SetTooltip("%s", text);
    return 0;
}

// TreeNode
int Lua_TreeNode(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    lua_pushboolean(L, ImGui::TreeNode(label));
    return 1;
}

// TreePop
int Lua_TreePop(lua_State* L) {
    ImGui::TreePop();
    return 0;
}

// CollapsingHeader
int Lua_CollapsingHeader(lua_State* L) {
    const char* label = luaL_checkstring(L, 1);
    ImGuiTreeNodeFlags flags = static_cast<ImGuiTreeNodeFlags>(luaL_optinteger(L, 2, 0));
    lua_pushboolean(L, ImGui::CollapsingHeader(label, flags));
    return 1;
}

// PushStyleColor
int Lua_PushStyleColor(lua_State* L) {
    ImGuiCol idx = static_cast<ImGuiCol>(luaL_checkinteger(L, 1));
    ImVec4 color = ImVec4(
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_checknumber(L, 5))
    );
    ImGui::PushStyleColor(idx, color);
    return 0;
}

// PopStyleColor
int Lua_PopStyleColor(lua_State* L) {
    int count = luaL_optinteger(L, 1, 1);
    ImGui::PopStyleColor(count);
    return 0;
}

// SameLine
int Lua_SameLine(lua_State* L) {
    float offset = static_cast<float>(luaL_optnumber(L, 1, 0.0f));
    float spacing = static_cast<float>(luaL_optnumber(L, 2, -1.0f));
    ImGui::SameLine(offset, spacing);
    return 0;
}

// Module functions
static const struct luaL_Reg ImGuiBindings[] = {
    {"SetCurrentContext", Lua_SetCurrentContext},
    {"Begin", Lua_Begin},
    {"End", Lua_End},
    {"Text", Lua_Text},
    {"Button", Lua_Button},
    {"Checkbox", Lua_Checkbox},
    {"Image", Lua_Image},
    {"ImageButton", Lua_ImageButton},
    {"SliderFloat", Lua_SliderFloat},
    {"SliderInt", Lua_SliderInt},
    {"RadioButton", Lua_RadioButton},
    {"ProgressBar", Lua_ProgressBar},
    {"InputText", Lua_InputText},
    {"ColorEdit4", Lua_ColorEdit4},
    {"Combo", Lua_Combo},
    {"IsItemHovered", Lua_IsItemHovered},
    {"SetTooltip", Lua_SetTooltip},
    {"TreeNode", Lua_TreeNode},
    {"TreePop", Lua_TreePop},
    {"CollapsingHeader", Lua_CollapsingHeader},
    {"PushStyleColor", Lua_PushStyleColor},
    {"PopStyleColor", Lua_PopStyleColor},
    {"SameLine", Lua_SameLine},
    {NULL, NULL} // Sentinel
};

// Lua module entry point
extern "C" __declspec(dllexport) int luaopen_imgui(lua_State* L) {
    luaL_newlib(L, ImGuiBindings);
    return 1; // Return the module table
}

