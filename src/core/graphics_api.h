#pragma once

#include <functional>

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>

#include "ui_manager.h"

class IGraphicsApi
{
    public:
        virtual ~IGraphicsApi() {}

        virtual void InitializeGraphicsApi(void* params);
        virtual bool InitializeImGuiImpl();
        virtual void NewFrame();
        virtual void RenderDrawData();
        virtual void SetRenderTarget();
        virtual void CleanupGraphicsApi(void* params);

        void* original_function = nullptr;

        static void* HookedFunction;

    protected:
        bool initialized = false;
        UiManager ui_manager;
};

class D3D11GraphicsApi : public IGraphicsApi
{
    public:
        D3D11GraphicsApi();

        void InitializeGraphicsApi(void* swap_chain) override;
        bool InitializeImGuiImpl() override;
        void NewFrame() override;
        void RenderDrawData() override;
        void SetRenderTarget() override;
        HRESULT __stdcall HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
        void CleanupGraphicsApi(void* params) override;

        ~D3D11GraphicsApi() override;

    private:
        ID3D11Device* d3d11_device = nullptr;
        ID3D11DeviceContext* d3d11_context = nullptr;
        ID3D11RenderTargetView* main_render_target_view = nullptr;
        IDXGISwapChain* swap_chain = nullptr;
        HWND target_window = nullptr;
};

class D3D12GraphicsApi : public IGraphicsApi
{
    public:
        D3D12GraphicsApi();

        void InitializeGraphicsApi(void* params) override;
        bool InitializeImGuiImpl() override;
        void NewFrame() override;
        void RenderDrawData() override;
        void SetRenderTarget() override;
        void CleanupGraphicsApi(void* params) override;

        ~D3D12GraphicsApi();
};

class VulkanGraphicsApi : public IGraphicsApi
{
    public:
        VulkanGraphicsApi();

        void InitializeGraphicsApi(void* params) override;
        bool InitializeImGuiImpl() override;
        void NewFrame() override;
        void RenderDrawData() override;
        void SetRenderTarget() override;
        void HookedFunc();
        void CleanupGraphicsApi(void* params) override;

        ~VulkanGraphicsApi();
};