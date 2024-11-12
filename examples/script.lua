local imgui = require("..\\bin\\imgui_lua_bindings")

imgui.NewFrame()

imgui.Begin("Demo Window")
imgui.Text("Hello from Lua with ImGui!")
imgui.End()

imgui.Render()