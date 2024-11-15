package.cpath = package.cpath .. ";.\\uif_mods\\imgui_lua_bindings.dll"

local imgui = require("imgui")

local context = ModContext
if context then
    imgui.SetCurrentContext(context)
end

-- Persistent state table
state = state or {
    checkbox_state = true,
    slider_float_value = 0.5,
    slider_int_value = 10,
    radio_button_value = 1,
    input_text_buffer = "Hello, ImGui!",
    color_value = {1.0, 0.0, 0.0, 1.0},
    combo_selected = 0,
    combo_items = {"Item 1", "Item 2", "Item 3"},
    progress_bar_value = 0.4,
    tree_node_open = false
}

-- Begin a new ImGui window
if imgui.Begin("Test Window") then
    -- Text
    imgui.Text("This is a text widget.")

    -- Button
    if imgui.Button("Click Me") then
        print("Button clicked!")
    end

    -- Checkbox
    local changed, updated_checkbox_state = imgui.Checkbox("Check this", state.checkbox_state)
    if changed then
        state.checkbox_state = updated_checkbox_state
        print("Checkbox state:", state.checkbox_state)
    end

    -- SliderFloat
    local changed, updated_float = imgui.SliderFloat("Float Slider", state.slider_float_value, 0.0, 1.0)
    if changed then
        state.slider_float_value = updated_float
        print("SliderFloat value:", state.slider_float_value)
    end

    -- SliderInt
    local changed, updated_int = imgui.SliderInt("Int Slider", state.slider_int_value, 0, 20)
    if changed then
        state.slider_int_value = updated_int
        print("SliderInt value:", state.slider_int_value)
    end

    -- RadioButton
    if imgui.RadioButton("Option 1", state.radio_button_value, 1) then
        state.radio_button_value = 1
    end
    if imgui.RadioButton("Option 2", state.radio_button_value, 2) then
        state.radio_button_value = 2
    end

    -- ProgressBar
    state.progress_bar_value = (state.progress_bar_value + 0.01) % 1.0
    imgui.ProgressBar(state.progress_bar_value, 200, 20, "Loading...")

    -- InputText -- Doesn't Work
    local changed, updated_text = imgui.InputText("Input Text", state.input_text_buffer, 256)
    if changed then
        state.input_text_buffer = updated_text
        print("InputText value:", state.input_text_buffer)
    end

    -- ColorEdit4
    local changed, r, g, b, a = imgui.ColorEdit4("Edit Color", state.color_value[1], state.color_value[2], state.color_value[3], state.color_value[4])
    if changed then
        state.color_value = {r, g, b, a}
        print("ColorEdit4 value:", table.concat(state.color_value, ", "))
    end

    -- Combo
    local changed, selected = imgui.Combo("Select Item", state.combo_selected, state.combo_items)
    if changed then
        state.combo_selected = selected
        print("Combo selected:", state.combo_items[state.combo_selected + 1])
    end

    -- Tooltip on hover
    if imgui.IsItemHovered() then
        imgui.SetTooltip("You hovered over the combo!")
    end

    -- TreeNode and TreePop
    if imgui.TreeNode("TreeNode Example") then
        imgui.Text("This is inside a TreeNode.")
        imgui.TreePop()
    end

    -- CollapsingHeader
    if imgui.CollapsingHeader("Collapsible Header") then
        imgui.Text("This is inside a collapsible header.")
    end

    -- PushStyleColor and PopStyleColor
    imgui.PushStyleColor(0, 0.2, 0.7, 0.9, 1.0) -- Change text color
    imgui.Text("Styled Text")
    imgui.PopStyleColor(1)

    -- SameLine
    imgui.Text("Text A")
    imgui.SameLine()
    imgui.Text("Text B")
end

-- End the window
imgui.End()
