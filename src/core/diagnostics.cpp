/**
 * @file diagnostics.cpp
 * @brief One-time environment inventories written to the core log.
 *
 * Everything here answers questions the rest of the log cannot: which swapchain we latched
 * onto, whether that swapchain belongs to the API we hooked, which window owns it, and who
 * else is loaded in the process. All of it runs on the render thread from the present hook,
 * so the per-frame entry point stays a pointer comparison and a counter.
 *
 * @author  mmvest (wereox)
 */

#include <Windows.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <string>

#include <plog/Log.h>

#include "core\diagnostics.h"
#include "core\graphics_api.h"
#include "core\ui_manager.h"

namespace
{
    // All of this state is touched only from the render thread inside the present hook.
    constexpr size_t        MAX_TRACKED_SWAP_CHAINS             = 16;
    constexpr unsigned      MAX_LOGGED_TARGET_WINDOW_CHANGES    = 5;
    constexpr unsigned long long PRESENT_HEARTBEAT_FRAMES       = 3600;   // About one line per minute at 60fps

    void*                   seen_swap_chains[MAX_TRACKED_SWAP_CHAINS]    = {};
    size_t                  seen_swap_chain_count                        = 0;
    unsigned long long      present_count                                = 0;
    unsigned                target_window_change_count                   = 0;

    bool HasSeenSwapChain(void* swap_chain)
    {
        for (size_t idx = 0; idx < seen_swap_chain_count && idx < MAX_TRACKED_SWAP_CHAINS; ++idx)
        {
            if (seen_swap_chains[idx] == swap_chain) return true;
        }
        return false;
    }

    void RememberSwapChain(void* swap_chain)
    {
        if (seen_swap_chain_count < MAX_TRACKED_SWAP_CHAINS)
        {
            seen_swap_chains[seen_swap_chain_count] = swap_chain;
        }
        ++seen_swap_chain_count;
    }

    const char* SwapEffectName(DXGI_SWAP_EFFECT effect)
    {
        switch (effect)
        {
            case DXGI_SWAP_EFFECT_DISCARD:          return "discard";
            case DXGI_SWAP_EFFECT_SEQUENTIAL:       return "sequential";
            case DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL:  return "flip_sequential";
            case DXGI_SWAP_EFFECT_FLIP_DISCARD:     return "flip_discard";
            default:                                return "unknown";
        }
    }

    // Which device implements a swapchain tells us whether the host is really running the API
    // we hooked. A d3d12 answer while we are bound to d3d11 explains an overlay that never draws.
    const char* SwapChainDeviceName(IDXGISwapChain* swap_chain)
    {
        ID3D11Device* device_11 = nullptr;
        if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&device_11)) && device_11)
        {
            device_11->Release();
            return "d3d11";
        }

        ID3D12Device* device_12 = nullptr;
        if (SUCCEEDED(swap_chain->GetDevice(__uuidof(ID3D12Device), (void**)&device_12)) && device_12)
        {
            device_12->Release();
            return "d3d12";
        }

        return "neither d3d11 nor d3d12";
    }

    // GetWindowText sends WM_GETTEXT to the owning thread, which would block the render thread
    // whenever that thread is busy or sitting in a modal dialog. The timeout keeps a diagnostic
    // dump from turning into a hang.
    std::wstring WindowTitle(HWND hwnd)
    {
        wchar_t     title[128]  = {};
        DWORD_PTR   unused      = 0;
        if (!SendMessageTimeoutW(hwnd, WM_GETTEXT, ARRAYSIZE(title), (LPARAM)title, SMTO_ABORTIFHUNG, 50, &unused))
        {
            return L"<unavailable>";
        }
        return title;
    }

    struct WindowInventoryContext
    {
        DWORD   pid;
        HWND    swapchain_window;
        HWND    imgui_root_window;
        HWND    foreground_window;
        bool    found_swapchain_window;
        int     window_count;
    };

    std::string BuildWindowTags(HWND hwnd, WindowInventoryContext* context)
    {
        std::string tags;
        if (hwnd == context->swapchain_window)
        {
            tags += " [SWAPCHAIN]";
            context->found_swapchain_window = true;
        }
        if (hwnd == context->imgui_root_window)  tags += " [IMGUI_ROOT]";
        if (hwnd == context->foreground_window)  tags += " [FOREGROUND]";
        if (UiManager::IsWindowHooked(hwnd))     tags += " [HOOKED]";
        return tags;
    }

    void LogWindow(HWND hwnd, WindowInventoryContext* context)
    {
        wchar_t class_name[128] = {};
        GetClassNameW(hwnd, class_name, ARRAYSIZE(class_name));

        RECT client_rect = {};
        GetClientRect(hwnd, &client_rect);

        const LONG  style           = GetWindowLongW(hwnd, GWL_STYLE);
        const DWORD owning_thread   = GetWindowThreadProcessId(hwnd, nullptr);

        PLOG_INFO << "[window] " << (void*)hwnd
                  << " parent " << (void*)GetParent(hwnd)
                  << " root " << (void*)GetAncestor(hwnd, GA_ROOT)
                  << " tid " << owning_thread
                  << " client " << (client_rect.right - client_rect.left) << "x" << (client_rect.bottom - client_rect.top)
                  << ((style & WS_CHILD) ? " child" : " toplevel")
                  << (IsWindowVisible(hwnd) ? " visible" : " hidden")
                  << " class " << class_name
                  << " title \"" << WindowTitle(hwnd) << "\""
                  << BuildWindowTags(hwnd, context);

        ++context->window_count;
    }

    BOOL CALLBACK LogChildWindowProc(HWND hwnd, LPARAM lparam)
    {
        LogWindow(hwnd, reinterpret_cast<WindowInventoryContext*>(lparam));
        return TRUE;
    }

    BOOL CALLBACK LogTopLevelWindowProc(HWND hwnd, LPARAM lparam)
    {
        WindowInventoryContext* context = reinterpret_cast<WindowInventoryContext*>(lparam);

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != context->pid)
        {
            return TRUE;
        }

        LogWindow(hwnd, context);
        EnumChildWindows(hwnd, LogChildWindowProc, lparam);
        return TRUE;
    }

    // A graphics runtime loaded from anywhere but the system directory means a proxy DLL owns
    // the present path before we ever see it. ReShade and Special K both work this way.
    void ReportGraphicsRuntime(const MODULEENTRY32W& module_entry, const wchar_t* system_dir, size_t system_dir_length)
    {
        static const wchar_t* GRAPHICS_RUNTIMES[] = { L"dxgi.dll", L"d3d11.dll", L"d3d12.dll", L"d3d9.dll", L"opengl32.dll", L"vulkan-1.dll" };

        bool is_graphics_runtime = false;
        for (const wchar_t* runtime_name : GRAPHICS_RUNTIMES)
        {
            if (_wcsicmp(module_entry.szModule, runtime_name) == 0)
            {
                is_graphics_runtime = true;
                break;
            }
        }

        if (!is_graphics_runtime)
        {
            return;
        }

        if (system_dir_length == 0 || _wcsnicmp(module_entry.szExePath, system_dir, system_dir_length) != 0)
        {
            PLOG_WARNING << "Graphics runtime " << module_entry.szModule
                         << " is loaded from outside the system directory, so a proxy DLL may own the present path: "
                         << module_entry.szExePath;
            return;
        }

        PLOG_INFO << "Graphics runtime loaded: " << module_entry.szModule;
    }
}

void CoreDiagnostics::LogModuleInventory()
{
    // Snapshots fail with ERROR_BAD_LENGTH when a module loads mid-walk, so retry a few times.
    HANDLE snapshot = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snapshot != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH)
        {
            break;
        }
    }

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        PLOG_WARNING << "Could not snapshot loaded modules. Error: " << GetLastError();
        return;
    }

    wchar_t     system_dir[MAX_PATH]    = {};
    const UINT  system_dir_length       = GetSystemDirectoryW(system_dir, ARRAYSIZE(system_dir));

    MODULEENTRY32W module_entry = {};
    module_entry.dwSize = sizeof(module_entry);

    PLOG_INFO << "--- Module inventory ---";

    int module_count = 0;
    if (Module32FirstW(snapshot, &module_entry))
    {
        do
        {
            const BYTE* module_start = module_entry.modBaseAddr;
            const BYTE* module_end   = module_entry.modBaseAddr + module_entry.modBaseSize;

            PLOG_INFO << "[module] " << (void*)module_start << " - " << (void*)module_end
                      << " (" << (module_entry.modBaseSize / 1024) << " KB) "
                      << module_entry.szModule << "  " << module_entry.szExePath;

            ReportGraphicsRuntime(module_entry, system_dir, system_dir_length);
            ++module_count;
        } while (Module32NextW(snapshot, &module_entry));
    }
    else
    {
        PLOG_WARNING << "Could not read the first module entry. Error: " << GetLastError();
    }

    CloseHandle(snapshot);
    PLOG_INFO << "--- Module inventory complete, " << module_count << " modules loaded ---";
}

void CoreDiagnostics::LogWindowInventory(const char* reason)
{
    WindowInventoryContext context   = {};
    context.pid                      = GetCurrentProcessId();
    context.swapchain_window         = IGraphicsApi::target_window;
    context.imgui_root_window        = UiManager::GetImGuiRootWindow();
    context.foreground_window        = GetForegroundWindow();

    DWORD foreground_pid = 0;
    GetWindowThreadProcessId(context.foreground_window, &foreground_pid);

    PLOG_INFO << "--- Window inventory (" << reason << ") ---";
    PLOG_INFO << "Swapchain output window " << (void*)context.swapchain_window
              << ", ImGui root window " << (void*)context.imgui_root_window
              << ", foreground window " << (void*)context.foreground_window
              << " owned by pid " << foreground_pid
              << (foreground_pid == context.pid ? " (ours)" : " (another process)");

    EnumWindows(LogTopLevelWindowProc, reinterpret_cast<LPARAM>(&context));

    // EnumWindows never returns message-only windows, so a swapchain window that does not turn
    // up here is a hidden helper window and almost certainly not the one the user is looking at.
    if (context.swapchain_window && !context.found_swapchain_window)
    {
        PLOG_WARNING << "Swapchain output window " << (void*)context.swapchain_window
                     << " was not found among this process's windows. It may be message-only or foreign.";
    }

    PLOG_INFO << "--- Window inventory complete, " << context.window_count << " windows ---";
}

void CoreDiagnostics::LogSwapChainInfo(void* swap_chain, const char* reason)
{
    if (!swap_chain)
    {
        PLOG_WARNING << "[swapchain] (" << reason << ") null swapchain passed to the present hook";
        return;
    }

    IDXGISwapChain* dxgi_swap_chain = (IDXGISwapChain*)swap_chain;

    DXGI_SWAP_CHAIN_DESC description = {};
    if (FAILED(dxgi_swap_chain->GetDesc(&description)))
    {
        PLOG_WARNING << "[swapchain] (" << reason << ") " << swap_chain << " did not return a description";
        return;
    }

    PLOG_INFO << "[swapchain] (" << reason << ") " << swap_chain
              << " device " << SwapChainDeviceName(dxgi_swap_chain)
              << " window " << (void*)description.OutputWindow
              << " " << description.BufferDesc.Width << "x" << description.BufferDesc.Height
              << " format " << description.BufferDesc.Format
              << " buffers " << description.BufferCount
              << " effect " << SwapEffectName(description.SwapEffect)
              << (description.Windowed ? " windowed" : " fullscreen")
              << " frame " << present_count;
}

void CoreDiagnostics::LogSwapChainIfNew(void* swap_chain)
{
    ++present_count;

    if (swap_chain && !HasSeenSwapChain(swap_chain))
    {
        RememberSwapChain(swap_chain);
        LogSwapChainInfo(swap_chain, seen_swap_chain_count == 1 ? "first present" : "new swapchain");
    }

    if ((present_count % PRESENT_HEARTBEAT_FRAMES) == 0)
    {
        PLOG_INFO << "[present] " << present_count << " frames through the hook, "
                  << seen_swap_chain_count << " distinct swapchains seen, current output window "
                  << (void*)IGraphicsApi::target_window;
    }
}

void CoreDiagnostics::OnTargetWindowChanged(HWND new_window)
{
    ++target_window_change_count;

    if (target_window_change_count <= MAX_LOGGED_TARGET_WINDOW_CHANGES)
    {
        PLOG_WARNING << "Render target window changed to " << (void*)new_window
                     << " (change number " << target_window_change_count << ")";
        LogWindowInventory("target window changed");
        return;
    }

    // Past the cap this is almost certainly two swapchains alternating, which tears down and
    // reinitializes the ImGui win32 backend every frame. The count is the useful part now.
    if ((target_window_change_count % PRESENT_HEARTBEAT_FRAMES) == 0)
    {
        PLOG_WARNING << "Render target window has now changed " << target_window_change_count
                     << " times, more than one swapchain is likely presenting";
    }
}
