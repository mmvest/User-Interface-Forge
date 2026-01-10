#include <stdexcept>
#include <string>

#include "directx\WICTextureLoader.h"
#include "imgui\imgui_impl_dx11.h"
#include "plog\Log.h"

#include "core\graphics_api.h"


// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                             IGraphicsApi Class                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
void    IGraphicsApi::Cleanup(void* params){}
void    (*IGraphicsApi::InitializeGraphicsApi)(void*)                                               = nullptr;
bool    (*IGraphicsApi::InitializeImGuiImpl)()                                                      = nullptr;
void    (*IGraphicsApi::NewFrame)()                                                                 = nullptr;
void    (*IGraphicsApi::Render)()                                                                   = nullptr;
void    (*IGraphicsApi::UpdateRenderTarget)(void*)                                                  = nullptr;
void    (*IGraphicsApi::OnGraphicsApiInvoke)(void*)                                                 = nullptr;
void*   (*IGraphicsApi::CreateTextureFromFile)(const std::wstring& file_path)                       = nullptr;
void    (*IGraphicsApi::ShutdownImGuiImpl)()                                                        = nullptr;
void*   IGraphicsApi::OriginalFunction                                                              = nullptr;
void*   IGraphicsApi::HookedFunction                                                                = nullptr;
HWND    IGraphicsApi::target_window                                                                 = nullptr;
bool    IGraphicsApi::initialized                                                                   = false;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           D3D11GraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
ID3D11Device*           D3D11GraphicsApi::d3d11_device            = nullptr;
ID3D11DeviceContext*    D3D11GraphicsApi::d3d11_context           = nullptr;
ID3D11RenderTargetView* D3D11GraphicsApi::main_render_target_view = nullptr;
IDXGISwapChain*         D3D11GraphicsApi::current_swap_chain      = nullptr;
UINT                    D3D11GraphicsApi::cached_backbuffer_width = 0;
UINT                    D3D11GraphicsApi::cached_backbuffer_height = 0;

D3D11GraphicsApi::D3D11GraphicsApi(void(*OnGraphicsApiInvoke)(void*) = nullptr)
{
    IGraphicsApi::OnGraphicsApiInvoke       = OnGraphicsApiInvoke;
    IGraphicsApi::InitializeGraphicsApi     = D3D11GraphicsApi::InitializeApi;
    IGraphicsApi::InitializeImGuiImpl       = D3D11GraphicsApi::InitializeImGui;
    IGraphicsApi::NewFrame                  = D3D11GraphicsApi::NewFrame;
    IGraphicsApi::UpdateRenderTarget        = D3D11GraphicsApi::UpdateRenderTarget;
    IGraphicsApi::Render                    = D3D11GraphicsApi::Render;
    IGraphicsApi::HookedFunction            = D3D11GraphicsApi::HookedPresent;
    IGraphicsApi::CreateTextureFromFile     = D3D11GraphicsApi::CreateTextureFromFile;
    IGraphicsApi::ShutdownImGuiImpl         = D3D11GraphicsApi::ShutdownImGuiImpl;
}

void D3D11GraphicsApi::InitializeApi(void* swap_chain)
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

    current_swap_chain = (IDXGISwapChain*)swap_chain;
    cached_backbuffer_width = swap_chain_description.BufferDesc.Width;
    cached_backbuffer_height = swap_chain_description.BufferDesc.Height;
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

void D3D11GraphicsApi::UpdateRenderTarget(void* swap_chain)
{
    if (!swap_chain)
    {
        return;
    }

    // To update the render target, we need to do a few things.
    // First, get the wap chain passed in from the Present function and check
    // if it is the same as our current swapchain. If it is, we need to 
    // reset our cached data.
    IDXGISwapChain* dxgi_swap_chain = (IDXGISwapChain*)swap_chain;
    if (dxgi_swap_chain != current_swap_chain)
    {
        current_swap_chain = dxgi_swap_chain;
        cached_backbuffer_width = 0;
        cached_backbuffer_height = 0;
    }

    // Next we need the swapchain description so we can get the output window HWND
    // for comparing the target window we are using with the current actual output
    // window.
    DXGI_SWAP_CHAIN_DESC swap_chain_description;
    HRESULT result = dxgi_swap_chain->GetDesc(&swap_chain_description);
    if (FAILED(result))
    {
        PLOG_WARNING << "Failed to get swapchain description while updating render target. Error: " << result;
        return;
    }

    if (target_window != swap_chain_description.OutputWindow)
    {
        target_window = swap_chain_description.OutputWindow;
    }

    // Next we need to check if the size of the backbuffers changed. If they changed from the last time
    // then we know the size changed and we need to handle that.
    const bool size_changed = cached_backbuffer_width != swap_chain_description.BufferDesc.Width
        || cached_backbuffer_height != swap_chain_description.BufferDesc.Height;

    // To handle the size changing, we need to essentially reset the state of our d3d11 objects
    // (the device, context, and render target) by getting rid of the old and retrieving the new.
    // We do this because the old device, context, and render target reference out-dated information.
    // Without this, whenever any of this data changes, UiForge breaks. No bueno.
    // Along with those, we cache the new backbuffer width and height so we can track changes to those.
    // After this, we SHOULD be good and UiForge should render properly in the new windows.
    if (!main_render_target_view || size_changed)
    {
        if (!d3d11_device || !d3d11_context)
        {
            ID3D11Device* new_device = nullptr;
            result = dxgi_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&new_device);
            if (FAILED(result))
            {
                PLOG_WARNING << "Failed to get DirectX device while updating render target. Error: " << result;
                return;
            }

            if (d3d11_device)
            {
                d3d11_device->Release();
            }
            d3d11_device = new_device;

            if (d3d11_context)
            {
                d3d11_context->Release();
            }
            d3d11_device->GetImmediateContext(&d3d11_context);
        }

        if (main_render_target_view)
        {
            main_render_target_view->Release();
            main_render_target_view = nullptr;
        }

        ID3D11Texture2D* back_buffer = nullptr;
        result = dxgi_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
        if (FAILED(result))
        {
            PLOG_WARNING << "Failed to get the DirectX backbuffer while updating render target. Error: " << result;
            return;
        }

        result = d3d11_device->CreateRenderTargetView(back_buffer, nullptr, &main_render_target_view);
        back_buffer->Release();
        if (FAILED(result))
        {
            PLOG_WARNING << "Failed to recreate render target view. Error: " << result;
            return;
        }

        cached_backbuffer_width = swap_chain_description.BufferDesc.Width;
        cached_backbuffer_height = swap_chain_description.BufferDesc.Height;
    }
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

void* D3D11GraphicsApi::CreateTextureFromFile(const std::wstring& file_path)
{
    void * out_texture_view = nullptr;
    PLOG_DEBUG << "Creating texture of " << file_path;
    HRESULT result = DirectX::CreateWICTextureFromFile( d3d11_device, d3d11_context, file_path.c_str(), nullptr, (ID3D11ShaderResourceView**)&out_texture_view );
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to load texture. Returned HRESULT: " << result;
    }
    else
    {
        PLOG_DEBUG << "Texture created: " << out_texture_view;
    }

    return out_texture_view;
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
        
        // TODO: Find out what cleanup is associated with CreateTextureFromFile

        current_swap_chain = nullptr;
        cached_backbuffer_width = 0;
        cached_backbuffer_height = 0;
        initialized = false;
    }
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
