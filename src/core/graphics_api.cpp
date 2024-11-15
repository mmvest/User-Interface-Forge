#include <stdexcept>
#include <string>
#include "graphics_api.h"
#include "core_utils.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                             IGraphicsApi Class                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void    IGraphicsApi::Cleanup(void* params){}
void    (*IGraphicsApi::InitializeGraphicsApi)(void*)   = nullptr;
bool    (*IGraphicsApi::InitializeImGuiImpl)()          = nullptr;
void    (*IGraphicsApi::NewFrame)()                     = nullptr;
void    (*IGraphicsApi::Render)()                       = nullptr;
void    (*IGraphicsApi::OnGraphicsApiInvoke)(void*)     = nullptr;
void    (*IGraphicsApi::ShutdownImGuiImpl)()            = nullptr;
void*   IGraphicsApi::OriginalFunction                  = nullptr;
void*   IGraphicsApi::HookedFunction                    = nullptr;
HWND    IGraphicsApi::target_window                     = nullptr;
bool    IGraphicsApi::initialized                       = false;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           D3D11GraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
#include "..\..\include\imgui_impl_dx11.h"
ID3D11Device*           D3D11GraphicsApi::d3d11_device            = nullptr;
ID3D11DeviceContext*    D3D11GraphicsApi::d3d11_context           = nullptr;
ID3D11RenderTargetView* D3D11GraphicsApi::main_render_target_view = nullptr;

D3D11GraphicsApi::D3D11GraphicsApi()
{
    IGraphicsApi::InitializeGraphicsApi     = D3D11GraphicsApi::InitializeApi;
    IGraphicsApi::InitializeImGuiImpl       = D3D11GraphicsApi::InitializeImGui;
    IGraphicsApi::NewFrame                  = D3D11GraphicsApi::NewFrame;
    IGraphicsApi::Render                    = D3D11GraphicsApi::Render;
    IGraphicsApi::HookedFunction            = D3D11GraphicsApi::HookedPresent;
    IGraphicsApi::ShutdownImGuiImpl         = D3D11GraphicsApi::ShutdownImGuiImpl;
}

void D3D11GraphicsApi::InitializeApi(void* swap_chain)
/**
 * @brief Initializes the DirectX 11 graphics API by performing a series of essential steps.

* This function takes the IDXGISwapChain instance as input, obtains the context,
* and uses the swap chain to obtain the description, back buffer, and create the render target view.
*
* @param swap_chain The IDXGISwapChain instance that contains the essential information about the DirectX 11 swap chain.
*/
{
    HRESULT result = ((IDXGISwapChain*)swap_chain)->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11_device);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get DirectX device. Error: " + std::to_string(result));
    }

    d3d11_device->GetImmediateContext(&d3d11_context);

    DXGI_SWAP_CHAIN_DESC swap_chain_description;
    result = ((IDXGISwapChain*)swap_chain)->GetDesc(&swap_chain_description);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get the description of the DirectX 11 swapchain. Error: " + std::to_string(result));
    }

    target_window = swap_chain_description.OutputWindow;

    ID3D11Texture2D* back_buffer = nullptr;
    result = ((IDXGISwapChain*)swap_chain)->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get the DirectX backbuffer. Error: " + std::to_string(result));
    }

    result = d3d11_device->CreateRenderTargetView(back_buffer, nullptr, &main_render_target_view);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to create render target view. Error: " + std::to_string(result));
    }

    back_buffer->Release();
}

bool D3D11GraphicsApi::InitializeImGui()
{
    return ImGui_ImplDX11_Init(d3d11_device, d3d11_context);
}

void D3D11GraphicsApi::NewFrame()
{
    ImGui_ImplDX11_NewFrame();
}

void D3D11GraphicsApi::Render()
{
    d3d11_context->OMSetRenderTargets(1, &main_render_target_view, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HRESULT __stdcall D3D11GraphicsApi::HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    IGraphicsApi::OnGraphicsApiInvoke((void*)swap_chain);
    return ((D3D11GraphicsApi::Present)OriginalFunction)(swap_chain, sync_interval, flags);
}

void D3D11GraphicsApi::ShutdownImGuiImpl()
{
    ImGui_ImplDX11_Shutdown();
}

void D3D11GraphicsApi::Cleanup(void* params)
{
    // Release DirectX resources
    if (initialized)
    {
        if (d3d11_device) 
        {
            d3d11_device->Release();
            d3d11_device = nullptr;
        }

        if (d3d11_context)
        {
            d3d11_context->Release();
            d3d11_context = nullptr;
        }

        if (main_render_target_view)
        {
            main_render_target_view->Release();
            main_render_target_view = nullptr;
        }

        initialized = false;
    }

    //CoreUtils::InfoMessageBox("Mod cleaned up");    // Don't delete this -- for some reason cleanup breaks if you do TODO: FIGURE OUT WHY I NEED THIS AND HOW TO FIX IT
}   

D3D11GraphicsApi::~D3D11GraphicsApi()
{
    Cleanup(nullptr);
}

// TODO: Implement the rest of the classes

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           D3D12GraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          VulkanGraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          OpenGLGraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝