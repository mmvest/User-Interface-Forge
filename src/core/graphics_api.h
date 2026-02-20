#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <string>

enum class GraphicsApiType
{
    DirectX11,
    DirectX12,
    Vulkan,
    OpenGL
};


/**
 * @brief Abstract interface for graphics API operations.
 *
 * This class defines a generic interface for initializing, managing, and rendering
 * using different graphics APIs such as DirectX 11, DirectX 12, Vulkan, and OpenGL.
 */
class IGraphicsApi
{
    public:
        virtual ~IGraphicsApi() {}

        /**
         * @brief Initializes the graphics API with platform-specific parameters.
         *
         * @param params Platform-specific parameters for initialization (e.g., swap chain for DirectX).
         */
        static void (*InitializeGraphicsApi)(void* params);

        /**
         * @brief Initializes ImGui with the graphics API implementation.
         *
         * @return True if initialization is successful, otherwise false.
         */
        static bool (*InitializeImGuiImpl)();

        /**
         * @brief Prepares a new frame for rendering.
         */
        static void (*NewFrame)();

        /**
         * @brief Executes rendering operations for the current frame.
         */
        static void (*Render)();

        /**
         * @brief Ensures the swap chain and render target are up to date.
         * 
         * This is especially important for making sure UiForge functions
         * properly when windows swap between fullscreen and windowed mode.
         *
         * @param params Platform-specific parameters for the graphics API function.
         */
        static void (*UpdateRenderTarget)(void* params);

        /**
         * @brief This function is called whenever the graphics API's primary function (e.g., `Present` for D3D11) is invoked.
         *
         * @param params Platform-specific parameters for the graphics API function.
         */        
        static void (*OnGraphicsApiInvoke)(void* params);

        /**
         * @brief Creates a graphics API compatible texture from a file.
         *
         * @param file_path Path to the texture file.
         * @return Pointer to the created texture resource.
         */
        static void* (*CreateTextureFromFile)(const std::wstring& file_path);

        /**
         * @brief Shuts down the ImGui implementation for the graphics API.
         */
        static void (*ShutdownImGuiImpl)();

        /**
         * @brief Cleans up resources associated with the graphics API.
         *
         * @param params Platform-specific parameters for cleanup.
         */        
        virtual void Cleanup(void* params);

        static void* OriginalFunction;  // Note the naming convention on these two variables don't match my normal convention.
        static void* HookedFunction;    // This is because I am treating these variables like functions and thus use function naming convention.
        static bool  initialized;
        static HWND  target_window;
};

/**
 * @brief Implementation for using the DirectX 11 graphics API with UiForge.
 */
class D3D11GraphicsApi : public IGraphicsApi
{
    public:
        /**
         * @brief Constructs the DirectX 11 graphics API and initializes hooks.
         *
         * @param OnGraphicsApiInvoke Callback to handle graphics API invocation.
         */
        D3D11GraphicsApi(void(*OnGraphicsApiInvoke)(void*));

        /**
         * @brief Initializes the DirectX 11 graphics API by performing a series of essential steps.

        * This function takes the IDXGISwapChain instance as input, obtains the context,
        * and uses the swap chain to obtain the description, back buffer, and create the render target view.
        *
        * @param swap_chain The IDXGISwapChain instance from the target application
        */
        static void InitializeApi(void* swap_chain);

        /**
         * @brief Initializes ImGui with the DirectX 11 implementation.
         *
         * @return True if initialization is successful, otherwise false.
         */
        static bool InitializeImGui();

        /**
         * @brief Prepares a new frame for rendering using DirectX 11 and the correct ImGui implementation.
         */        
        static void NewFrame();

        /**
         * @brief Updates the swap chain and render target view if the back buffer changes.
         *
         * @param swap_chain The swap chain for the DirectX 11 graphics pipeline.
         */
        static void UpdateRenderTarget(void* swap_chain);

        /**
         * @brief Executes rendering for the current frame using DirectX 11 and ImGui.
         */
        static void Render();

        /**
         * @brief Hooked implementation of the `Present` function for DirectX 11.
         *
         * This function invokes the registered callback for graphics API invocation and
         * then calls the original `Present` function.
         *
         * @param swap_chain The swap chain for the DirectX 11 graphics pipeline.
         * @param sync_interval Synchronization interval.
         * @param flags Flags for the `Present` call.
         * @return HRESULT indicating success or failure of the operation.
         */        
        static HRESULT __stdcall HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

        /**
         * @brief Creates a texture from a file and loads it into the DirectX 11 context.
         *
         * @param file_path Path to the texture file.
         * @return Pointer to the created texture resource.
         */        
        static void* CreateTextureFromFile(const std::wstring& file_path);

        /**
         * @brief Shuts down the ImGui implementation for DirectX 11.
         */        
        static void ShutdownImGuiImpl();

        /**
         * @brief Cleans up resources associated with the DirectX 11 graphics API.
         *
         * This function releases DirectX resources such as the device, context, and render target view.
         *
         * @param params Additional cleanup parameters (unused).
         */        
        void Cleanup(void* params = nullptr) override;

        /**
         * @brief Type definition for the original Directx 11 Present function.
         */
        typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

        /**
         * @brief Destructor for the DirectX 11 graphics API.
         *
         * Calls the cleanup function to ensure all resources are released.
         */
        ~D3D11GraphicsApi() override;

    private:
        static ID3D11Device*            d3d11_device;
        static ID3D11DeviceContext*     d3d11_context;
        static ID3D11RenderTargetView*  main_render_target_view;
};


/** 
 * @brief Implementation placeholder for DirectX 12 graphics API.
 *
 * @note This class is currently unimplemented.
 */
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

/** 
 * @brief Implementation placeholder for Vulkan graphics API.
 *
 * @note This class is currently unimplemented.
 */
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
