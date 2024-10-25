#include <stdexcept>
#include <string>
#include "graphics_api.h"
#include "core_utils.h"

// DirectX 11
#include "..\..\include\imgui_impl_dx11.h"


class D3D11GraphicsApi : public IGraphicsApi
{
    private:
        ID3D11Device* d3d11_device = nullptr;
        ID3D11DeviceContext* d3d11_context = nullptr;
        ID3D11RenderTargetView* main_render_target_view = nullptr;
        HWND target_window = nullptr;
        typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

    public:
        D3D11GraphicsApi()
        {
            // SUUUUPER janky... I know
            HookedFunction = reinterpret_cast<void*>(+[](D3D11GraphicsApi* obj, IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags) -> HRESULT
            {
                return obj->HookedPresent(swap_chain, sync_interval, flags);
            });
        }

        void InitializeGraphicsApi(void* swap_chain) override
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

        bool InitializeImGuiImpl() override
        {
           return ImGui_ImplDX11_Init(d3d11_device, d3d11_context);
        }

        void NewFrame() override
        {
            ImGui_ImplDX11_NewFrame();
        }

        void RenderDrawData() override
        {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        void SetRenderTarget() override
        {
            d3d11_context->OMSetRenderTargets(1, &main_render_target_view, nullptr);
        }

        HRESULT __stdcall HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
        {
            if (!initialized)
            {
                try
                {
                    InitializeGraphicsApi((void *)swap_chain);
                    ui_manager = UiManager();
                    InitializeImGuiImpl();
                }
                catch (const std::exception& err)
                {
                    CoreUtils::ErrorMessageBox(err.what());
                    FreeLibrary(core_module);
                    return ((D3D11GraphicsApi::Present)original_function)(swap_chain, sync_interval, flags);
                }

                initialized = true;
            }


            ui_manager.RenderUiElements();

            CoreUtils::ProcessCustomInputs();  // Put this here so it will return straight into calling the original D3D11 function
            return ((D3D11GraphicsApi::Present)original_function)(swap_chain, sync_interval, flags);
        }

        void CleanupGraphicsApi(void* params) override
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

        ~D3D11GraphicsApi() override
        {
            CleanupGraphicsApi(nullptr);
        }

};