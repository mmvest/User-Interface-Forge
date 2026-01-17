// Minimal D3D11 window with keyboard controls for mode switching.
// Note/Disclaimer?? This is completely AI generated using GPT-5.2.
// I Looked over it briefly to make sure it seemed like everything was
// correct at a high level. Contributors, feel free to poke at it and make improvements.
//
// Keys:
//   1 = Windowed
//   2 = True fullscreen (exclusive) via IDXGISwapChain::SetFullscreenState(TRUE)
//   3 = Borderless fullscreen (windowed swapchain, WS_POPUP covering the monitor)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstdarg>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ------------------------------------------------------------
// Logging / error helpers
// ------------------------------------------------------------
static void dbgprint(const wchar_t* fmt, ...)
{
    wchar_t buf[2048]{};
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");

    // Also emit to stderr (useful when running from console).
    fwprintf(stderr, L"%s\n", buf);
}

static std::wstring HrToStr(HRESULT hr)
{
    wchar_t* msg = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD lang = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    if (FormatMessageW(flags, nullptr, (DWORD)hr, lang, (LPWSTR)&msg, 0, nullptr) && msg)
    {
        std::wstring s = msg;
        LocalFree(msg);
        // Trim trailing whitespace/newlines.
        while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ' || s.back() == L'\t'))
            s.pop_back();
        return s;
    }
    wchar_t fallback[64];
    swprintf_s(fallback, L"HRESULT 0x%08X", (unsigned)hr);
    return fallback;
}

#define HR_LOG_RETURN(hr, what) do { \
    HRESULT _hr = (hr); \
    if (FAILED(_hr)) { \
        dbgprint(L"%s failed: 0x%08X (%s)", L##what, (unsigned)_hr, HrToStr(_hr).c_str()); \
        return false; \
    } \
} while(0)

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while(0)

// ------------------------------------------------------------
// Globals (simple sample)
// ------------------------------------------------------------
static HWND g_hwnd = nullptr;

static ID3D11Device*           g_device = nullptr;
static ID3D11DeviceContext*    g_ctx = nullptr;
static IDXGISwapChain*         g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;

static bool g_inExclusive = false;
static bool g_borderless = false;

static DWORD g_savedStyle = 0;
static DWORD g_savedExStyle = 0;
static RECT  g_savedRect{};

static bool g_running = true;

// Forward declarations
static bool CreateDeviceAndSwapChain(HWND hwnd);
static bool CreateBackbufferRTV();
static void CleanupD3D();
static bool ResizeSwapChain(UINT w, UINT h);
static void Render();
static bool SetWindowed();
static bool SetExclusiveFullscreen();
static bool SetBorderlessFullscreen();
static RECT GetMonitorRectForWindow(HWND hwnd);

// ------------------------------------------------------------
// D3D11 setup
// ------------------------------------------------------------
static bool CreateDeviceAndSwapChain(HWND hwnd)
{
    // Determine initial client size
    RECT rc{};
    GetClientRect(hwnd, &rc);
    UINT w = (UINT)(rc.right - rc.left);
    UINT h = (UINT)(rc.bottom - rc.top);
    if (w == 0) w = 1280;
    if (h == 0) h = 720;

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = w;
    sd.BufferDesc.Height = h;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // useful for exclusive fullscreen toggles

    UINT createFlags = 0;
#if defined(_DEBUG)
    // If you have the debug layer installed, this helps.
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL fls[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    D3D_FEATURE_LEVEL outFL = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        fls, (UINT)_countof(fls),
        D3D11_SDK_VERSION,
        &sd,
        &g_swap,
        &g_device,
        &outFL,
        &g_ctx
    );

    if (FAILED(hr))
    {
        // Fallback to WARP if hardware creation fails.
        dbgprint(L"Hardware device creation failed: 0x%08X (%s). Trying WARP...", (unsigned)hr, HrToStr(hr).c_str());
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createFlags,
            fls, (UINT)_countof(fls),
            D3D11_SDK_VERSION,
            &sd,
            &g_swap,
            &g_device,
            &outFL,
            &g_ctx
        );
        HR_LOG_RETURN(hr, "D3D11CreateDeviceAndSwapChain (WARP)");
    }

    dbgprint(L"D3D11 device created. Feature level: 0x%04X", (unsigned)outFL);

    HR_LOG_RETURN(CreateBackbufferRTV() ? S_OK : E_FAIL, "CreateBackbufferRTV");
    return true;
}

static bool CreateBackbufferRTV()
{
    SAFE_RELEASE(g_rtv);

    ID3D11Texture2D* backbuf = nullptr;
    HRESULT hr = g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuf);
    if (FAILED(hr))
    {
        dbgprint(L"IDXGISwapChain::GetBuffer failed: 0x%08X (%s)", (unsigned)hr, HrToStr(hr).c_str());
        return false;
    }

    hr = g_device->CreateRenderTargetView(backbuf, nullptr, &g_rtv);
    SAFE_RELEASE(backbuf);

    if (FAILED(hr))
    {
        dbgprint(L"ID3D11Device::CreateRenderTargetView failed: 0x%08X (%s)", (unsigned)hr, HrToStr(hr).c_str());
        return false;
    }
    return true;
}

static bool ResizeSwapChain(UINT w, UINT h)
{
    if (!g_swap) return false;
    if (w == 0 || h == 0) return true; // minimized

    SAFE_RELEASE(g_rtv);

    HRESULT hr = g_swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        dbgprint(L"ResizeBuffers(%u,%u) failed: 0x%08X (%s)", w, h, (unsigned)hr, HrToStr(hr).c_str());
        return false;
    }
    return CreateBackbufferRTV();
}

static void CleanupD3D()
{
    if (g_swap && g_inExclusive)
    {
        // Best effort to leave exclusive fullscreen to avoid "stuck" display mode.
        g_swap->SetFullscreenState(FALSE, nullptr);
        g_inExclusive = false;
    }

    SAFE_RELEASE(g_rtv);
    SAFE_RELEASE(g_swap);
    SAFE_RELEASE(g_ctx);
    SAFE_RELEASE(g_device);
}

static void Render()
{
    if (!g_ctx || !g_rtv) return;

    // Bind RTV
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);

    // Simple clear
    const float clear[4] = { 0.07f, 0.10f, 0.13f, 1.0f };
    g_ctx->ClearRenderTargetView(g_rtv, clear);

    HRESULT hr = g_swap->Present(1, 0);

    if (FAILED(hr))
    {
        // Device removed / reset etc.
        dbgprint(L"Present failed: 0x%08X (%s)", (unsigned)hr, HrToStr(hr).c_str());
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            HRESULT reason = g_device ? g_device->GetDeviceRemovedReason() : hr;
            dbgprint(L"Device lost. RemovedReason: 0x%08X (%s)", (unsigned)reason, HrToStr(reason).c_str());
        }
        // Exit cleanly on Present failure to keep sample simple.
        g_running = false;
        PostQuitMessage((int)hr);
    }
}

// ------------------------------------------------------------
// Window / fullscreen mode helpers
// ------------------------------------------------------------
static RECT GetMonitorRectForWindow(HWND hwnd)
{
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
        return mi.rcMonitor;

    RECT fallback{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &fallback, 0);
    return fallback;
}

static void SaveWindowPlacement()
{
    g_savedStyle = (DWORD)GetWindowLongPtrW(g_hwnd, GWL_STYLE);
    g_savedExStyle = (DWORD)GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
    GetWindowRect(g_hwnd, &g_savedRect);
}

static void RestoreWindowPlacement()
{
    SetWindowLongPtrW(g_hwnd, GWL_STYLE, (LONG_PTR)g_savedStyle);
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, (LONG_PTR)g_savedExStyle);

    SetWindowPos(
        g_hwnd, nullptr,
        g_savedRect.left, g_savedRect.top,
        g_savedRect.right - g_savedRect.left,
        g_savedRect.bottom - g_savedRect.top,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
    );
}

static bool SetWindowed()
{
    if (!g_swap) return false;

    // If currently in exclusive fullscreen, exit it first.
    if (g_inExclusive)
    {
        HRESULT hr = g_swap->SetFullscreenState(FALSE, nullptr);
        if (FAILED(hr))
        {
            dbgprint(L"SetFullscreenState(FALSE) failed: 0x%08X (%s)", (unsigned)hr, HrToStr(hr).c_str());
            return false;
        }
        g_inExclusive = false;
    }

    if (g_borderless)
    {
        // Restore original window styles / size.
        RestoreWindowPlacement();
        g_borderless = false;
    }

    // Ensure swapchain knows it's windowed (should already be).
    return true;
}

static bool SetExclusiveFullscreen()
{
    if (!g_swap) return false;

    // If borderless, restore window first (exclusive doesn't require it, but avoids style oddities)
    if (g_borderless)
    {
        if (!SetWindowed())
            return false;
    }

    // Request exclusive fullscreen on current output.
    HRESULT hr = g_swap->SetFullscreenState(TRUE, nullptr);
    if (FAILED(hr))
    {
        dbgprint(L"SetFullscreenState(TRUE) failed: 0x%08X (%s)", (unsigned)hr, HrToStr(hr).c_str());
        return false;
    }
    g_inExclusive = true;

    // Optional: you can set a specific display mode here. We'll let DXGI pick.
    // But ensure the window is brought to front.
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    return true;
}

static bool SetBorderlessFullscreen()
{
    if (!g_hwnd || !g_swap) return false;

    // Must not be in exclusive
    if (g_inExclusive)
    {
        HRESULT hr = g_swap->SetFullscreenState(FALSE, nullptr);
        if (FAILED(hr))
        {
            dbgprint(L"SetFullscreenState(FALSE) failed while switching to borderless: 0x%08X (%s)", (unsigned)hr, HrToStr(hr).c_str());
            return false;
        }
        g_inExclusive = false;
    }

    if (!g_borderless)
    {
        SaveWindowPlacement();
    }

    RECT mon = GetMonitorRectForWindow(g_hwnd);

    // Make window borderless
    DWORD style = (DWORD)GetWindowLongPtrW(g_hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);

    style &= ~(WS_OVERLAPPEDWINDOW);
    style |= WS_POPUP;

    // Optional: keep it topmost for borderless fullscreen feel
    // but you can remove TOPMOST if you dislike it.
    exStyle |= WS_EX_APPWINDOW;

    SetWindowLongPtrW(g_hwnd, GWL_STYLE, (LONG_PTR)style);
    SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, (LONG_PTR)exStyle);

    SetWindowPos(
        g_hwnd, HWND_TOP,
        mon.left, mon.top,
        mon.right - mon.left,
        mon.bottom - mon.top,
        SWP_FRAMECHANGED | SWP_NOACTIVATE
    );

    ShowWindow(g_hwnd, SW_SHOW);
    g_borderless = true;
    return true;
}

// ------------------------------------------------------------
// Win32 window proc
// ------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        if (g_swap)
        {
            UINT w = LOWORD(lParam);
            UINT h = HIWORD(lParam);

            if (wParam == SIZE_MINIMIZED)
                return 0;

            if (!ResizeSwapChain(w, h))
            {
                dbgprint(L"ResizeSwapChain failed on WM_SIZE. Exiting.");
                g_running = false;
                PostQuitMessage(1);
            }
        }
        return 0;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == '1')
        {
            if (!SetWindowed())
            {
                dbgprint(L"Failed to switch to windowed. Exiting.");
                g_running = false;
                PostQuitMessage(2);
            }
            return 0;
        }
        if (wParam == '2')
        {
            if (!SetExclusiveFullscreen())
            {
                dbgprint(L"Failed to switch to exclusive fullscreen. Returning to windowed (best effort).");
                SetWindowed();
            }
            return 0;
        }
        if (wParam == '3')
        {
            if (!SetBorderlessFullscreen())
            {
                dbgprint(L"Failed to switch to borderless fullscreen. Returning to windowed (best effort).");
                SetWindowed();
            }
            return 0;
        }
        break;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ------------------------------------------------------------
// Entry
// ------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    // Optional: allocate a console if launched from explorer and you want stderr visible.
    // Comment out if you dislike consoles popping up.
    // AllocConsole(); freopen("CONOUT$", "w", stdout); freopen("CONOUT$", "w", stderr);

    const wchar_t* kClassName = L"D3D11FullscreenModesWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc))
    {
        dbgprint(L"RegisterClassExW failed. GetLastError=%lu", GetLastError());
        return 1;
    }

    // Create an initial overlapped window
    RECT r{ 0,0,1280,720 };
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);

    g_hwnd = CreateWindowExW(
        0,
        kClassName,
        L"D3D11 Window (1=Windowed, 2=Exclusive FS, 3=Borderless FS, Esc=Quit)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, hInst, nullptr
    );

    if (!g_hwnd)
    {
        dbgprint(L"CreateWindowExW failed. GetLastError=%lu", GetLastError());
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // Save initial placement for restoring after borderless
    SaveWindowPlacement();

    if (!CreateDeviceAndSwapChain(g_hwnd))
    {
        dbgprint(L"Failed to initialize D3D11. Exiting.");
        CleanupD3D();
        DestroyWindow(g_hwnd);
        return 1;
    }

    // Main loop
    MSG msg{};
    while (g_running)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!g_running) break;
        Render();
    }

    // Clean shutdown
    CleanupD3D();

    if (g_hwnd)
    {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }

    UnregisterClassW(kClassName, hInst);
    return (int)msg.wParam;
}
