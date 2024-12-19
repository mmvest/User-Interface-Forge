state = state or {
    window_open         = true,
    button_clicked      = false,
    checkbox_checked    = false,
    radio_buttons       = 0,
    slider_float        = 0.5,
    slider_int          = 5,
    combo_items         = {"Item 01", "Item 02", "Item 03"},
    combo_index         = 0,
    color4              = { 1, .5, .25, 1},
    progress_percent        = 0
}

-- Make a window

if ImGui.Begin("Hello, UiForge!", state.window_open, ImGuiWindowFlags.MenuBar) then
    
    -- Menu Bars
    if ImGui.BeginMenuBar() then
        if ImGui.BeginMenu("Menu 01") then
            if ImGui.MenuItem("Item 01") then
                -- Do a thing
            end
            if ImGui.MenuItem("Item 02") then
                -- Do a thing
            end
            ImGui.EndMenu()
        end
        if ImGui.BeginMenu("Menu 02") then
            if ImGui.MenuItem("Item 01") then
                -- Do a thing
            end
            if ImGui.MenuItem("Item 02") then
                -- Do a thing
            end
            ImGui.EndMenu()
        end
        ImGui.EndMenuBar()
    end

    -- Display text
    ImGui.Text("You can display text like this.")
    ImGui.BulletText("This is a bullet point!")
    
    -- Button
    if ImGui.Button("Click Me!", 200, 50) then
        state.button_clicked = not state.button_clicked
    end   -- Make Buttons like this
    if state.button_clicked then
        ImGui.Text("Button Clicked!")
    
    end

    -- Checkbox
    state.checkbox_checked = ImGui.Checkbox("I am a checkbox", state.checkbox_checked)
    ImGui.SameLine()

    -- Tooltips
    ImGui.Text("(?)")
    if ImGui.IsItemHovered() then
        ImGui.SetTooltip("This is a tooltip!")
    end

    -- Radio buttons -- I use an override of the function that isn't in meta.lua
    if ImGui.RadioButton("Radio 01", state.radio_buttons == 1) then
        state.radio_buttons = 1
    end

    if ImGui.RadioButton("Radio 02", state.radio_buttons == 2) then
        state.radio_buttons = (state.radio_buttons == 2) and 0 or 2   -- Makes the button toggleable on and off by clicking it again
    end

    -- Sliders
    state.slider_float = ImGui.SliderFloat("Float Slider", state.slider_float, 0, 1.0)
    state.slider_int = ImGui.SliderInt("Int Slider", state.slider_int, 0, 10, "Slider Text: " .. state.slider_int)

    -- Combo Box
    state.combo_index = ImGui.Combo("Combo Box", state.combo_index, state.combo_items, #state.combo_items)

    -- Color Picker
    state.color4 = ImGui.ColorEdit4("Color Picker", state.color4)

    -- Progress Bar
    local progress_text = string.format("Loading... (%.1f%%)", state.progress_percent * 100)
    ImGui.ProgressBar(state.progress_percent, 200, 20, progress_text)
    state.progress_percent = state.progress_percent + .001
    if state.progress_percent >= 1 then
        state.progress_percent = 0
    end

    -- Tabs
    if ImGui.BeginTabBar("Tabs") then
        if ImGui.BeginTabItem("Tab 01") then
            ImGui.Text("This is the content for tab 1")
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("Tab 02") then
            ImGui.Button("This is the content for tab 2")
            ImGui.EndTabItem()
        end
        if ImGui.BeginTabItem("Tab 03") then
            ImGui.Text("This is the content for tab 3")
            ImGui.EndTabItem()
        end
        ImGui.EndTabBar()
    end

end

-- End the window (MUST BE CALLED REGARDLESS OF THE RESULT OF BEGIN)
ImGui.End()