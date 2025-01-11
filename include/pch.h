#pragma once

#include <Windows.h>
#include <thread>
#include <stdexcept>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <fstream>
#include <codecvt>
#include <functional>   //std::hash
#include <chrono>       //std::chrono::high_resolution_clock


#include "kiero\kiero.h"

#include "luajit\lua.hpp"

#include "plog\Log.h"
#include "plog\Initializers\RollingFileInitializer.h"

#include "core\util.h"

#include "imgui\imgui.h"
#include "imgui\imgui_impl_win32.h"
#include "imgui\imgui_impl_dx11.h"
#include "imgui\sol_ImGui.h"

#include "scl\SCL.hpp"

#include "sol\sol.hpp"

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>
#include "directx\WICTextureLoader.h"
