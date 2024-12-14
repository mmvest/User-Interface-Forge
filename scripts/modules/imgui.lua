local ffi = require("ffi")

local imgui_path = uiforge_bin_dir .. "\\imgui_directx11_1.91.2.dll"

local imgui = ffi.load(imgui_path)

-- Define the imgui function signatures
ffi.cdef[[
    typedef struct { float x, y; } ImVec2;
    typedef struct { float x, y, z, w; } ImVec4;

    void NewImVec2(float x, float y, ImVec2* out);
    void NewImVec4(float a, float b, float c, float d, ImVec4* out);
    void SetCurrentContext(void* context);
    bool Begin(const char* name, bool* p_open, int flags);
    void End();
    void Text(const char* fmt);
    bool Button(const char* label);
    bool Checkbox(const char* label, bool* v);
    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, float power);
    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format);
    bool RadioButton(const char* label, bool active);
    void ProgressBar(float fraction, ImVec2 size_arg, const char* overlay);
    bool InputText(const char* label, char* buf, size_t buf_size, int flags, void* callback, void* user_data);
    bool ColorEdit4(const char* label, float* col, int flags);
    bool Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items);
    bool IsItemHovered();
    void SetTooltip(const char* fmt);
    bool TreeNode(const char* label);
    void TreePop();
    bool CollapsingHeader(const char* label, int flags);
    void PushStyleColor(int idx, ImVec4 col);
    void PopStyleColor(int count);
    void SameLine(float offset_from_start_x, float spacing);
]]


--Helper to ensure the context is set
local function ensure_context()
    local context = mod_context
    if context then
        imgui.SetCurrentContext(context)
    else
        error("ImGui context is not set! Please ensure 'mod_context' is initialized.")
    end
end



-- Wrap the Begin function to automatically ensure context
local wrapped_imgui = setmetatable({}, {
    __index = function(_, key)
        if key == "Begin" then
            -- Wrap Begin with context management
            return function(name, p_open, flags)
                ensure_context()
                return imgui.Begin(name, p_open, flags)
            end
        end
        -- Return other functions as-is
        return imgui[key]
    end
})

return wrapped_imgui