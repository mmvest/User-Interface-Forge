#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <string>
#include <unordered_map>
#include <vector>

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
         * @brief Creates a graphics API compatible texture from raw 32-bit RGBA pixels.
         *
         * @param pixels Pointer to width * height * 4 bytes of RGBA pixel data (row-major, no padding).
         * @param width Texture width in pixels.
         * @param height Texture height in pixels.
         * @return Pointer to the created texture resource, or nullptr on failure.
         */
        static void* (*CreateTextureFromMemory)(const void* pixels, int width, int height);

        /**
         * @brief Releases a texture previously returned by CreateTextureFromFile or CreateTextureFromMemory.
         *
         * Texture handles are API specific (a COM shader resource view for D3D11, a GPU descriptor
         * handle for D3D12), so releasing must go through the active implementation rather than a
         * blind IUnknown::Release.
         *
         * @param texture The texture handle to release. Null is ignored.
         */
        static void (*ReleaseTexture)(void* texture);

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
         * @brief Creates a texture from raw 32-bit RGBA pixels in the DirectX 11 context.
         *
         * @param pixels Pointer to width * height * 4 bytes of RGBA pixel data (row-major, no padding).
         * @param width Texture width in pixels.
         * @param height Texture height in pixels.
         * @return Pointer to the created shader resource view, or nullptr on failure.
         */
        static void* CreateTextureFromMemory(const void* pixels, int width, int height);

        /**
         * @brief Releases a D3D11 texture handle (a COM shader resource view).
         */
        static void ReleaseTexture(void* texture);

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
 * @brief Implementation for using the DirectX 12 graphics API with UiForge.
 *
 * D3D12 differs from D3D11 in two important ways for a Present hook:
 *
 * 1. Rendering requires the application's ID3D12CommandQueue, which cannot be obtained from
 *    the swap chain. The queue is captured by additionally hooking
 *    ID3D12CommandQueue::ExecuteCommandLists (see HookedExecuteCommandLists). Rendering is
 *    skipped until a direct command queue has been captured.
 *
 * 2. Texture handles handed to ImGui are D3D12_GPU_DESCRIPTOR_HANDLE values into a
 *    shader-visible SRV descriptor heap, not COM pointers. This class owns that heap and a
 *    registry mapping handles back to their resources so ReleaseTexture can clean up.
 */
class D3D12GraphicsApi : public IGraphicsApi
{
    public:
        /**
         * @brief Constructs the DirectX 12 graphics API and wires the static interface.
         *
         * @param OnGraphicsApiInvoke Callback to handle graphics API invocation.
         */
        D3D12GraphicsApi(void(*OnGraphicsApiInvoke)(void*));

        /**
         * @brief Initializes the DirectX 12 graphics API from the application's swap chain.
         *
         * Obtains the device and swap chain description, then creates the render target
         * descriptor heap, the shader-visible SRV descriptor heap, per-backbuffer command
         * allocators, the command list, and the upload fence.
         *
         * @param swap_chain The IDXGISwapChain instance from the target application.
         */
        static void InitializeApi(void* swap_chain);

        /**
         * @brief Initializes ImGui with the DirectX 12 implementation.
         *
         * @return True if initialization is successful, otherwise false.
         */
        static bool InitializeImGui();

        /**
         * @brief Prepares a new frame for rendering using DirectX 12 and the correct ImGui implementation.
         */
        static void NewFrame();

        /**
         * @brief Acquires the current backbuffer and creates its render target view for this frame.
         *
         * Backbuffer references are never held across frames (released at the end of Render()),
         * so swap chain resizes need no special handling.
         *
         * @param swap_chain The swap chain for the DirectX 12 graphics pipeline.
         */
        static void UpdateRenderTarget(void* swap_chain);

        /**
         * @brief Records and submits the ImGui draw data on the captured application command queue.
         */
        static void Render();

        /**
         * @brief Hooked implementation of the `Present` function for DXGI swap chains under D3D12.
         *
         * Skips UiForge processing until the application's direct command queue has been captured
         * by HookedExecuteCommandLists, then behaves like the D3D11 hook.
         *
         * @param swap_chain The swap chain being presented.
         * @param sync_interval Synchronization interval.
         * @param flags Flags for the `Present` call.
         * @return HRESULT indicating success or failure of the operation.
         */
        static HRESULT __stdcall HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

        /**
         * @brief Hooked implementation of ID3D12CommandQueue::ExecuteCommandLists.
         *
         * Used purely to capture the application's direct command queue, which is required for
         * submitting the ImGui command list and for texture uploads. Always forwards to the
         * original function.
         */
        static void __stdcall HookedExecuteCommandLists(ID3D12CommandQueue* command_queue, UINT num_command_lists, ID3D12CommandList* const* command_lists);

        /**
         * @brief Creates a texture from a file in the DirectX 12 context.
         *
         * The image is decoded to 32-bit RGBA with WIC and uploaded through CreateTextureFromMemory.
         *
         * @param file_path Path to the texture file.
         * @return An opaque texture handle (a GPU descriptor handle) usable with ImGui.Image, or nullptr on failure.
         */
        static void* CreateTextureFromFile(const std::wstring& file_path);

        /**
         * @brief Creates a texture from raw 32-bit RGBA pixels in the DirectX 12 context.
         *
         * Uploads the pixels to a default-heap texture via a temporary upload buffer, waits for
         * the copy on the captured command queue, and allocates an SRV descriptor for it.
         *
         * @param pixels Pointer to width * height * 4 bytes of RGBA pixel data (row-major, no padding).
         * @param width Texture width in pixels.
         * @param height Texture height in pixels.
         * @return An opaque texture handle (a GPU descriptor handle) usable with ImGui.Image, or nullptr on failure.
         */
        static void* CreateTextureFromMemory(const void* pixels, int width, int height);

        /**
         * @brief Releases a D3D12 texture handle (frees its SRV descriptor and underlying resource).
         */
        static void ReleaseTexture(void* texture);

        /**
         * @brief Shuts down the ImGui implementation for DirectX 12.
         */
        static void ShutdownImGuiImpl();

        /**
         * @brief Cleans up all DirectX 12 resources owned by this implementation.
         *
         * @param params Additional cleanup parameters (unused).
         */
        void Cleanup(void* params = nullptr) override;

        /**
         * @brief Type definition for the original DXGI Present function.
         */
        typedef HRESULT(__stdcall* Present) (IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags);

        /**
         * @brief Type definition for the original ID3D12CommandQueue::ExecuteCommandLists function.
         */
        typedef void(__stdcall* ExecuteCommandLists) (ID3D12CommandQueue* command_queue, UINT num_command_lists, ID3D12CommandList* const* command_lists);

        /**
         * @brief Kiero method-table hook slot and trampoline for ExecuteCommandLists.
         *
         * Present uses the IGraphicsApi OriginalFunction/HookedFunction pair; this extra pair is
         * needed because D3D12 requires two hooks.
         */
        static void* OriginalExecuteCommandLists;

        /**
         * @brief Destructor for the DirectX 12 graphics API.
         */
        ~D3D12GraphicsApi() override;

    private:
        /**
         * @brief Tracks an SRV-backed texture so ReleaseTexture can free its descriptor and resource.
         */
        struct TextureRecord
        {
            ID3D12Resource* resource;
            UINT            heap_index;
        };

        /**
         * @brief Allocates a slot in the shader-visible SRV descriptor heap.
         *
         * @return The slot index, or UINT_MAX when the heap is exhausted.
         */
        static UINT AllocateSrvSlot();

        /**
         * @brief Returns the CPU/GPU descriptor handles for an SRV heap slot.
         */
        static D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(UINT slot);
        static D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(UINT slot);

        /**
         * @brief SRV descriptor allocation callbacks handed to the ImGui DX12 backend.
         */
        static void ImGuiSrvAlloc(struct ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle);
        static void ImGuiSrvFree(struct ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);

        /**
         * @brief Submits a closed command list on the captured queue and blocks until the GPU finishes it.
         *
         * Used for texture uploads, which must complete before the upload buffer is destroyed.
         *
         * @return true when the submission and wait succeeded.
         */
        static bool ExecuteAndWait(ID3D12CommandList* command_list);

        static ID3D12Device*                        d3d12_device;
        static ID3D12CommandQueue*                  d3d12_command_queue;        // Captured from the app via HookedExecuteCommandLists
        static ID3D12GraphicsCommandList*           d3d12_command_list;
        static std::vector<ID3D12CommandAllocator*> command_allocators;         // One per swap chain buffer
        static ID3D12DescriptorHeap*                rtv_descriptor_heap;
        static ID3D12DescriptorHeap*                srv_descriptor_heap;        // Shader visible, shared by ImGui and user textures
        static UINT                                 rtv_descriptor_size;
        static UINT                                 srv_descriptor_size;
        static std::vector<bool>                    srv_slot_used;              // Free list for srv_descriptor_heap slots
        static std::unordered_map<UINT64, TextureRecord> texture_registry;      // GPU descriptor ptr -> owning resource
        static DXGI_FORMAT                          rtv_format;                 // Swap chain buffer format, needed by ImGui init
        static UINT                                 buffer_count;
        static ID3D12Resource*                      current_back_buffer;        // Held only between UpdateRenderTarget and Render
        static UINT                                 current_buffer_index;
        static ID3D12Fence*                         upload_fence;
        static UINT64                               upload_fence_value;
        static HANDLE                               upload_fence_event;
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
