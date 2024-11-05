local imgui = require("..\\bin\\imgui_lua_bindings")

-- Start a new ImGui frame
imgui.NewFrame()

-- Display a window
imgui.Begin("Demo Window")
imgui.Text("Hello from Lua with ImGui!")
imgui.End()

-- Render the ImGui frame
imgui.Render()