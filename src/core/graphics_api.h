#pragma once

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>

#include "ui_manager.h"

enum class GraphicsApiType
{
    DirectX11,
    DirectX12,
    Vulkan,
    OpenGL
};

class IGraphicsApi
{
    public:
        virtual ~IGraphicsApi() {}

        // Perhaps use these in the case that they can help make a more generalized solution
        //virtual void InitializeGraphicsApi(void* params);
        // virtual bool InitializeImGuiImpl();
        // virtual void NewFrame();
        // virtual void RenderDrawData();
        // virtual void SetRenderTarget();
        virtual void CleanupGraphicsApi(void* params);

        static void* OriginalFunction;
        static void* HookedFunction;
        static bool initialized;
        static UiManager* ui_manager;
};

class D3D11GraphicsApi : public IGraphicsApi
{
    public:
        D3D11GraphicsApi();

        static void InitializeGraphicsApi(void* swap_chain);
        // bool InitializeImGuiImpl() override;
        // void NewFrame() override;
        // void RenderDrawData() override;
        // void SetRenderTarget() override;
        static HRESULT __stdcall HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
        void CleanupGraphicsApi(void* params) override;
        typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

        ~D3D11GraphicsApi() override;

    private:
        static ID3D11Device* d3d11_device;
        static ID3D11DeviceContext* d3d11_context;
        static ID3D11RenderTargetView* main_render_target_view;
        static HWND target_window;
        static D3D11GraphicsApi* instance;
        
};

class D3D12GraphicsApi : public IGraphicsApi
{
    public:
        D3D12GraphicsApi();

        // void InitializeGraphicsApi(void* params) override;
        // bool InitializeImGuiImpl() override;
        // void NewFrame() override;
        // void RenderDrawData() override;
        // void SetRenderTarget() override;
        // void CleanupGraphicsApi(void* params) override;

        ~D3D12GraphicsApi();
};

class VulkanGraphicsApi : public IGraphicsApi
{
    public:
        VulkanGraphicsApi();

        // void InitializeGraphicsApi(void* params) override;
        // bool InitializeImGuiImpl() override;
        // void NewFrame() override;
        // void RenderDrawData() override;
        // void SetRenderTarget() override;
        // void CleanupGraphicsApi(void* params) override;

        ~VulkanGraphicsApi();
};