local ffi = require("ffi")

-- move these directories to the config so they can be dynamic
package.path = uiforge_modules_dir .. "\\?.lua" .. ";" .. package.path
local imgui = require("imgui")

state = state or {
    checkbox_state = ffi.new("bool[1]", true),
    slider_float_value = ffi.new("float[1]", 0.5),
    slider_int_value = ffi.new("int[1]", 10),
    radio_button_value = 1,
    input_text_buffer = ffi.new("char[256]", "Hello, ImGui!"),
    color_value = ffi.new("float[4]", {1.0, 0.0, 0.0, 1.0}),
    combo_selected = ffi.new("int[1]", 0),
    combo_items = ffi.new("const char*[3]", {"Item 1", "Item 2", "Item 3"}),
    progress_bar_value = 0.4,
}

-- Create ImVec2 and ImVec4 instances for testing
local size = ffi.new("ImVec2")
imgui.NewImVec2(200, 20, size)

local color = ffi.new("ImVec4")
imgui.NewImVec4(0.5, 0.5, 0.5, 1.0, color)

-- Set the current ImGui context
-- local context = mod_context
-- if context then
--     imgui.SetCurrentContext(context)
-- end

-- Start a new window
if imgui.Begin("Test Window", ffi.cast("void*", nil), 0) then
    -- Text
    imgui.Text("Testing all functions.")

    -- -- Button
    -- if imgui.Button("Click Me") then
    --     print("Button clicked!")
    -- end

    -- -- Checkbox
    -- if imgui.Checkbox("Check this", state.checkbox_state) then
    --     print("Checkbox state:", state.checkbox_state[0])
    -- end

    -- -- SliderFloat
    -- if imgui.SliderFloat("Float Slider", state.slider_float_value, 0.0, 1.0, "%.2f", 1.0) then
    --     print("SliderFloat value:", state.slider_float_value[0])
    -- end

    -- -- SliderInt
    -- if imgui.SliderInt("Int Slider", state.slider_int_value, 0, 20, "%d") then
    --     print("SliderInt value:", state.slider_int_value[0])
    -- end

    -- -- RadioButton
    -- if imgui.RadioButton("Option 1", state.radio_button_value == 1) then
    --     state.radio_button_value = 1
    -- end
    -- if imgui.RadioButton("Option 2", state.radio_button_value == 2) then
    --     state.radio_button_value = 2
    -- end
    -- print("RadioButton selected:", state.radio_button_value)

    -- -- ProgressBar
    -- state.progress_bar_value = (state.progress_bar_value + 0.01) % 1.0
    -- imgui.ProgressBar(state.progress_bar_value, size, "Progress Bar")

    -- -- InputText
    -- if imgui.InputText("Input Text", state.input_text_buffer, ffi.sizeof(state.input_text_buffer), 0, nil, nil) then
    --     print("InputText value:", ffi.string(state.input_text_buffer))
    -- end

    -- -- ColorEdit4
    -- if imgui.ColorEdit4("Edit Color", state.color_value, 0) then
    --     print("ColorEdit4 value:", table.concat({state.color_value[0], state.color_value[1], state.color_value[2], state.color_value[3]}, ", "))
    -- end

    -- -- -- Combo
    -- -- if imgui.Combo("Select Item", state.combo_selected, state.combo_items, 3, 5) then
    -- --     print("Combo selected:", state.combo_items[state.combo_selected[0] + 1])
    -- -- end

    -- -- IsItemHovered and Tooltip
    -- if imgui.IsItemHovered() then
    --     imgui.SetTooltip("Hovered over combo box!")
    -- end

    -- -- TreeNode and TreePop
    -- if imgui.TreeNode("TreeNode Example") then
    --     imgui.Text("Inside a TreeNode")
    --     imgui.TreePop()
    -- end

    -- -- CollapsingHeader
    -- if imgui.CollapsingHeader("Collapsible Header", 0) then
    --     imgui.Text("Inside Collapsible Header")
    -- end

    -- -- PushStyleColor and PopStyleColor
    -- imgui.PushStyleColor(0, color)
    -- imgui.Text("Styled Text")
    -- imgui.PopStyleColor(1)

    -- -- SameLine
    -- imgui.Text("Text A")
    -- imgui.SameLine()
    -- imgui.Text("Text B")
end

-- End the window
imgui.End()