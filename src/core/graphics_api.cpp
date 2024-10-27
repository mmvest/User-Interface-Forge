#include <stdexcept>
#include <string>
#include "graphics_api.h"
#include "core_utils.h"

// DirectX 11
#include "..\..\include\imgui_impl_dx11.h"

void*       IGraphicsApi::original_function     = nullptr;
void*       IGraphicsApi::HookedFunction        = nullptr;
bool        IGraphicsApi::initialized           = false;
UiManager*  IGraphicsApi::ui_manager            = nullptr;

// Initialize D3D11 static variables
ID3D11Device*           D3D11GraphicsApi::d3d11_device              = nullptr;
ID3D11DeviceContext*    D3D11GraphicsApi::d3d11_context             = nullptr;
ID3D11RenderTargetView* D3D11GraphicsApi::main_render_target_view   = nullptr;
HWND                    D3D11GraphicsApi::target_window             = nullptr;

D3D11GraphicsApi::D3D11GraphicsApi()
{
    HookedFunction = D3D11GraphicsApi::HookedPresent;
}

void D3D11GraphicsApi::InitializeGraphicsApi(void* swap_chain)
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

HRESULT __stdcall D3D11GraphicsApi::HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    if (!initialized)
    {
        try
        {
            InitializeGraphicsApi((void *)swap_chain);
            ui_manager = new UiManager(target_window);
            ImGui_ImplDX11_Init(d3d11_device, d3d11_context);
        }
        catch (const std::exception& err)
        {
            CoreUtils::ErrorMessageBox(err.what());
            FreeLibrary(GetModuleHandleA(NULL));
            return ((D3D11GraphicsApi::Present)original_function)(swap_chain, sync_interval, flags);
        }

        initialized = true;
    }

    ImGui_ImplDX11_NewFrame();
    ui_manager->RenderUiElements();
    d3d11_context->OMSetRenderTargets(1, &main_render_target_view, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    CoreUtils::ProcessCustomInputs();  // Put this here so it will return straight into calling the original D3D11 function
    return ((D3D11GraphicsApi::Present)original_function)(swap_chain, sync_interval, flags);
}

void D3D11GraphicsApi::CleanupGraphicsApi(void* params)
{
    // Release DirectX resources
    if (d3d11_context) {
        d3d11_context->Release();
        d3d11_context = nullptr;
    }

    if (d3d11_device) {
        d3d11_device->Release();
        d3d11_device = nullptr;
    }
}   

D3D11GraphicsApi::~D3D11GraphicsApi()
{
    CleanupGraphicsApi(nullptr);
}