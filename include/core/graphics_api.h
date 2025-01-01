#pragma once

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>
#include "directx\WICTextureLoader.h"

#include "core\ui_manager.h"

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

        
        static void (*InitializeGraphicsApi)(void* params);
        static bool (*InitializeImGuiImpl)();
        static void (*NewFrame)();
        static void (*Render)();
        static void (*OnGraphicsApiInvoke)(void* params);
        static void* (*CreateTextureFromFile)(const std::wstring& file_path);
        static void (*ShutdownImGuiImpl)();
        virtual void Cleanup(void* params);

        static void* OriginalFunction;  // Note the naming convention on the following two variables don't match my normal convention.
        static void* HookedFunction;    // This is because I am treating these variables like functions and thus use function naming convention.
        static bool  initialized;
        static HWND  target_window;
};

class D3D11GraphicsApi : public IGraphicsApi
{
    public:
        D3D11GraphicsApi();

        static void InitializeApi(void* swap_chain);
        static bool InitializeImGui();
        static void NewFrame();
        static void Render();
        static HRESULT __stdcall HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);
        static void* CreateTextureFromFile(const std::wstring& file_path);
        static void ShutdownImGuiImpl();
        void Cleanup(void* params = nullptr) override;
        typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

        ~D3D11GraphicsApi() override;

    private:
        static ID3D11Device*            d3d11_device;
        static ID3D11DeviceContext*     d3d11_context;
        static ID3D11RenderTargetView*  main_render_target_view;
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
        // void Cleanup(void* params) override;

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
        // void Cleanup(void* params) override;

        ~VulkanGraphicsApi();
};