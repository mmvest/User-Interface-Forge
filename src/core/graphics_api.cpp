#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <wincodec.h>

#include <WICTextureLoader.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_dx12.h>
#include <plog/Log.h>

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
void*   (*IGraphicsApi::CreateTextureFromMemory)(const void* pixels, int width, int height)         = nullptr;
void    (*IGraphicsApi::ReleaseTexture)(void* texture)                                              = nullptr;
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
    IGraphicsApi::CreateTextureFromMemory   = D3D11GraphicsApi::CreateTextureFromMemory;
    IGraphicsApi::ReleaseTexture            = D3D11GraphicsApi::ReleaseTexture;
    IGraphicsApi::ShutdownImGuiImpl         = D3D11GraphicsApi::ShutdownImGuiImpl;
}

void D3D11GraphicsApi::InitializeApi(void* swap_chain)
{
    IDXGISwapChain* dxgi_swap_chain = (IDXGISwapChain*)swap_chain;
    HRESULT result = dxgi_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11_device);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get DirectX device. Error: " + std::to_string(result));
    }

    d3d11_device->GetImmediateContext(&d3d11_context);

    DXGI_SWAP_CHAIN_DESC swap_chain_description;
    result = dxgi_swap_chain->GetDesc(&swap_chain_description);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get the description of the DirectX 11 swapchain. Error: " + std::to_string(result));
    }

    target_window = swap_chain_description.OutputWindow;

    // Render-target view is acquired per-frame in UpdateRenderTarget() and released at end of Render().
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

    IDXGISwapChain* dxgi_swap_chain = (IDXGISwapChain*)swap_chain;

    DXGI_SWAP_CHAIN_DESC swap_chain_description;
    HRESULT result = dxgi_swap_chain->GetDesc(&swap_chain_description);
    if (FAILED(result))
    {
        PLOG_WARNING << "Failed to get swapchain description while updating render target. Error: " << result;
        return;
    }

    target_window = swap_chain_description.OutputWindow;

    // Hybrid lifecycle:
    // - Long-lived: device + immediate context (kept until shutdown; refreshed only if the device changes).
    // - Short-lived: swapchain/backbuffer/RTV (never retained beyond the current frame).
    if (main_render_target_view)
    {
        main_render_target_view->Release();
        main_render_target_view = nullptr;
    }

    ID3D11Device* new_device = nullptr;
    result = dxgi_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&new_device);
    if (FAILED(result) || !new_device)
    {
        PLOG_WARNING << "Failed to get DirectX device while updating render target. Error: " << result;
        return;
    }

    if (!d3d11_device)
    {
        d3d11_device = new_device;
    }
    else if (new_device != d3d11_device)
    {
        PLOG_WARNING << "D3D11 device changed; resetting cached device/context pointers.";
        d3d11_device->Release();
        d3d11_device = new_device;

        if (d3d11_context)
        {
            d3d11_context->Release();
            d3d11_context = nullptr;
        }
    }
    else
    {
        // GetDevice() AddRef's the returned pointer; drop the extra reference.
        new_device->Release();
    }

    if (!d3d11_context)
    {
        d3d11_device->GetImmediateContext(&d3d11_context);
        if (!d3d11_context)
        {
            PLOG_WARNING << "Failed to get DirectX immediate context while updating render target.";
            return;
        }
    }

    ID3D11Texture2D* back_buffer = nullptr;
    result = dxgi_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
    if (FAILED(result) || !back_buffer)
    {
        PLOG_WARNING << "Failed to get the DirectX backbuffer while updating render target. Error: " << result;
        return;
    }

    result = d3d11_device->CreateRenderTargetView(back_buffer, nullptr, &main_render_target_view);
    back_buffer->Release();
    if (FAILED(result) || !main_render_target_view)
    {
        PLOG_WARNING << "Failed to create render target view while updating render target. Error: " << result;
        return;
    }
}

void D3D11GraphicsApi::Render()
{
    if (!d3d11_context || !main_render_target_view)
    {
        return;
    }

    d3d11_context->OMSetRenderTargets(1, &main_render_target_view, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Release per-frame references.
    main_render_target_view->Release();
    main_render_target_view = nullptr;
}

HRESULT __stdcall D3D11GraphicsApi::HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    IGraphicsApi::OnGraphicsApiInvoke((void*)swap_chain);
    return ((D3D11GraphicsApi::Present)OriginalFunction)(swap_chain, sync_interval, flags);
}

void* D3D11GraphicsApi::CreateTextureFromFile(const std::wstring& file_path)
{
    void * out_texture_view = nullptr;
    if (!d3d11_device || !d3d11_context)
    {
        PLOG_WARNING << "CreateTextureFromFile called without an initialized device/context.";
        return nullptr;
    }

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

void* D3D11GraphicsApi::CreateTextureFromMemory(const void* pixels, int width, int height)
{
    if (!d3d11_device)
    {
        PLOG_WARNING << "CreateTextureFromMemory called without an initialized device.";
        return nullptr;
    }

    if (!pixels || width <= 0 || height <= 0)
    {
        PLOG_WARNING << "CreateTextureFromMemory called with invalid arguments (pixels=" << pixels
                     << ", width=" << width << ", height=" << height << ").";
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC texture_description = {};
    texture_description.Width               = width;
    texture_description.Height              = height;
    texture_description.MipLevels           = 1;
    texture_description.ArraySize           = 1;
    texture_description.Format              = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_description.SampleDesc.Count    = 1;
    texture_description.Usage               = D3D11_USAGE_IMMUTABLE;
    texture_description.BindFlags           = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initial_data = {};
    initial_data.pSysMem     = pixels;
    initial_data.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    HRESULT result = d3d11_device->CreateTexture2D(&texture_description, &initial_data, &texture);
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to create texture from memory. Returned HRESULT: " << result;
        return nullptr;
    }

    ID3D11ShaderResourceView* out_texture_view = nullptr;
    result = d3d11_device->CreateShaderResourceView(texture, nullptr, &out_texture_view);
    texture->Release();     // The shader resource view keeps its own reference.
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to create shader resource view for memory texture. Returned HRESULT: " << result;
        return nullptr;
    }

    PLOG_DEBUG << "Texture created from memory (" << width << "x" << height << "): " << out_texture_view;
    return out_texture_view;
}

void D3D11GraphicsApi::ReleaseTexture(void* texture)
{
    if (!texture)
    {
        return;
    }

    // D3D11 texture handles are COM shader resource views.
    ((IUnknown*)texture)->Release();
}

void D3D11GraphicsApi::ShutdownImGuiImpl()
{
    ImGui_ImplDX11_Shutdown();
}

void D3D11GraphicsApi::Cleanup(void* params)
{
    // Release DirectX resources (safe to call even if UiForge didn't finish initialization).
    if (main_render_target_view)
    {
        main_render_target_view->Release();
        main_render_target_view = nullptr;
    }

    if (d3d11_context)
    {
        d3d11_context->Release();
        d3d11_context = nullptr;
    }

    if (d3d11_device)
    {
        d3d11_device->Release();
        d3d11_device = nullptr;
    }

    initialized = false;
}   

D3D11GraphicsApi::~D3D11GraphicsApi()
{
    Cleanup(nullptr);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                           D3D12GraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

ID3D12Device*                        D3D12GraphicsApi::d3d12_device              = nullptr;
ID3D12CommandQueue*                  D3D12GraphicsApi::d3d12_command_queue       = nullptr;
ID3D12GraphicsCommandList*           D3D12GraphicsApi::d3d12_command_list        = nullptr;
std::vector<ID3D12CommandAllocator*> D3D12GraphicsApi::command_allocators;
ID3D12DescriptorHeap*                D3D12GraphicsApi::rtv_descriptor_heap       = nullptr;
ID3D12DescriptorHeap*                D3D12GraphicsApi::srv_descriptor_heap       = nullptr;
UINT                                 D3D12GraphicsApi::rtv_descriptor_size       = 0;
UINT                                 D3D12GraphicsApi::srv_descriptor_size       = 0;
std::vector<bool>                    D3D12GraphicsApi::srv_slot_used;
std::unordered_map<UINT64, D3D12GraphicsApi::TextureRecord> D3D12GraphicsApi::texture_registry;
DXGI_FORMAT                          D3D12GraphicsApi::rtv_format                = DXGI_FORMAT_R8G8B8A8_UNORM;
UINT                                 D3D12GraphicsApi::buffer_count              = 0;
ID3D12Resource*                      D3D12GraphicsApi::current_back_buffer       = nullptr;
UINT                                 D3D12GraphicsApi::current_buffer_index      = 0;
ID3D12Fence*                         D3D12GraphicsApi::upload_fence              = nullptr;
UINT64                               D3D12GraphicsApi::upload_fence_value        = 0;
HANDLE                               D3D12GraphicsApi::upload_fence_event        = nullptr;
void*                                D3D12GraphicsApi::OriginalExecuteCommandLists = nullptr;

// Size of the shader-visible SRV heap shared by the ImGui backend (fonts, internal textures)
// and user textures created via CreateTextureFromFile/CreateTextureFromMemory.
static const UINT UIFORGE_D3D12_SRV_HEAP_CAPACITY = 256;

D3D12GraphicsApi::D3D12GraphicsApi(void(*OnGraphicsApiInvoke)(void*) = nullptr)
{
    IGraphicsApi::OnGraphicsApiInvoke       = OnGraphicsApiInvoke;
    IGraphicsApi::InitializeGraphicsApi     = D3D12GraphicsApi::InitializeApi;
    IGraphicsApi::InitializeImGuiImpl       = D3D12GraphicsApi::InitializeImGui;
    IGraphicsApi::NewFrame                  = D3D12GraphicsApi::NewFrame;
    IGraphicsApi::UpdateRenderTarget        = D3D12GraphicsApi::UpdateRenderTarget;
    IGraphicsApi::Render                    = D3D12GraphicsApi::Render;
    IGraphicsApi::HookedFunction            = D3D12GraphicsApi::HookedPresent;
    IGraphicsApi::CreateTextureFromFile     = D3D12GraphicsApi::CreateTextureFromFile;
    IGraphicsApi::CreateTextureFromMemory   = D3D12GraphicsApi::CreateTextureFromMemory;
    IGraphicsApi::ReleaseTexture            = D3D12GraphicsApi::ReleaseTexture;
    IGraphicsApi::ShutdownImGuiImpl         = D3D12GraphicsApi::ShutdownImGuiImpl;
}

void __stdcall D3D12GraphicsApi::HookedExecuteCommandLists(ID3D12CommandQueue* command_queue, UINT num_command_lists, ID3D12CommandList* const* command_lists)
{
    // ImGui rendering and texture uploads must be submitted on the application's own direct
    // queue; the swap chain cannot provide it. Capture the first direct queue we see.
    if (!d3d12_command_queue && command_queue && command_queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        command_queue->AddRef();
        d3d12_command_queue = command_queue;
        PLOG_INFO << "Captured application D3D12 direct command queue: " << command_queue;
    }

    ((D3D12GraphicsApi::ExecuteCommandLists)OriginalExecuteCommandLists)(command_queue, num_command_lists, command_lists);
}

HRESULT __stdcall D3D12GraphicsApi::HookedPresent(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    // Nothing can be rendered or uploaded until the app's command queue has been captured
    // by HookedExecuteCommandLists, so skip UiForge processing until then.
    if (d3d12_command_queue)
    {
        IGraphicsApi::OnGraphicsApiInvoke((void*)swap_chain);
    }

    return ((D3D12GraphicsApi::Present)OriginalFunction)(swap_chain, sync_interval, flags);
}

void D3D12GraphicsApi::InitializeApi(void* swap_chain)
{
    IDXGISwapChain* dxgi_swap_chain = (IDXGISwapChain*)swap_chain;

    HRESULT result = dxgi_swap_chain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12_device);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get DirectX 12 device from the swap chain. Error: " + std::to_string(result));
    }

    DXGI_SWAP_CHAIN_DESC swap_chain_description;
    result = dxgi_swap_chain->GetDesc(&swap_chain_description);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to get the description of the DirectX 12 swapchain. Error: " + std::to_string(result));
    }

    target_window = swap_chain_description.OutputWindow;
    buffer_count  = swap_chain_description.BufferCount;
    rtv_format    = swap_chain_description.BufferDesc.Format;

    // Render target view heap -- one slot per swap chain buffer, rewritten every frame.
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_description = {};
    rtv_heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_description.NumDescriptors = buffer_count;
    result = d3d12_device->CreateDescriptorHeap(&rtv_heap_description, __uuidof(ID3D12DescriptorHeap), (void**)&rtv_descriptor_heap);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to create the DirectX 12 RTV descriptor heap. Error: " + std::to_string(result));
    }
    rtv_descriptor_size = d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Shader-visible SRV heap shared by ImGui and user textures.
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_description = {};
    srv_heap_description.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_description.NumDescriptors = UIFORGE_D3D12_SRV_HEAP_CAPACITY;
    srv_heap_description.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    result = d3d12_device->CreateDescriptorHeap(&srv_heap_description, __uuidof(ID3D12DescriptorHeap), (void**)&srv_descriptor_heap);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to create the DirectX 12 SRV descriptor heap. Error: " + std::to_string(result));
    }
    srv_descriptor_size = d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    srv_slot_used.assign(UIFORGE_D3D12_SRV_HEAP_CAPACITY, false);

    // One command allocator per swap chain buffer. Present pacing guarantees the GPU is done
    // with a buffer's previous command list by the time the same buffer index comes around again.
    command_allocators.resize(buffer_count, nullptr);
    for (UINT idx = 0; idx < buffer_count; idx++)
    {
        result = d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&command_allocators[idx]);
        if (FAILED(result))
        {
            throw std::runtime_error("Failed to create a DirectX 12 command allocator. Error: " + std::to_string(result));
        }
    }

    result = d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&d3d12_command_list);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to create the DirectX 12 command list. Error: " + std::to_string(result));
    }
    d3d12_command_list->Close();    // Created open; Render() resets it at the start of each frame.

    // Fence used to block until one-shot texture upload command lists finish on the GPU.
    result = d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&upload_fence);
    if (FAILED(result))
    {
        throw std::runtime_error("Failed to create the DirectX 12 upload fence. Error: " + std::to_string(result));
    }

    upload_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!upload_fence_event)
    {
        throw std::runtime_error("Failed to create the DirectX 12 upload fence event. Error: " + std::to_string(GetLastError()));
    }
}

UINT D3D12GraphicsApi::AllocateSrvSlot()
{
    for (UINT slot = 0; slot < (UINT)srv_slot_used.size(); slot++)
    {
        if (!srv_slot_used[slot])
        {
            srv_slot_used[slot] = true;
            return slot;
        }
    }

    PLOG_ERROR << "D3D12 SRV descriptor heap is exhausted (" << srv_slot_used.size() << " slots).";
    return UINT_MAX;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12GraphicsApi::GetSrvCpuHandle(UINT slot)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = srv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (SIZE_T)slot * srv_descriptor_size;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12GraphicsApi::GetSrvGpuHandle(UINT slot)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = srv_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (UINT64)slot * srv_descriptor_size;
    return handle;
}

void D3D12GraphicsApi::ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
{
    const UINT slot = AllocateSrvSlot();
    if (slot == UINT_MAX)
    {
        out_cpu_handle->ptr = 0;
        out_gpu_handle->ptr = 0;
        return;
    }

    *out_cpu_handle = GetSrvCpuHandle(slot);
    *out_gpu_handle = GetSrvGpuHandle(slot);
}

void D3D12GraphicsApi::ImGuiSrvFree(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
{
    if (!srv_descriptor_heap || !srv_descriptor_size)
    {
        return;
    }

    const UINT64 heap_start = srv_descriptor_heap->GetGPUDescriptorHandleForHeapStart().ptr;
    const UINT64 slot = (gpu_handle.ptr - heap_start) / srv_descriptor_size;
    if (slot < srv_slot_used.size())
    {
        srv_slot_used[(size_t)slot] = false;
    }
}

bool D3D12GraphicsApi::InitializeImGui()
{
    ImGui_ImplDX12_InitInfo init_info;
    init_info.Device                = d3d12_device;
    init_info.CommandQueue          = d3d12_command_queue;
    init_info.NumFramesInFlight     = (int)buffer_count;
    init_info.RTVFormat             = rtv_format;
    init_info.DSVFormat             = DXGI_FORMAT_UNKNOWN;
    init_info.SrvDescriptorHeap     = srv_descriptor_heap;
    init_info.SrvDescriptorAllocFn  = D3D12GraphicsApi::ImGuiSrvAlloc;
    init_info.SrvDescriptorFreeFn   = D3D12GraphicsApi::ImGuiSrvFree;

    return ImGui_ImplDX12_Init(&init_info);
}

void D3D12GraphicsApi::NewFrame()
{
    ImGui_ImplDX12_NewFrame();
}

void D3D12GraphicsApi::UpdateRenderTarget(void* swap_chain)
{
    if (!swap_chain || !d3d12_device)
    {
        return;
    }

    // Backbuffer references are per-frame only: acquired here, released at the end of
    // Render(). Never holding them across a Present means swap chain resizes can't
    // deadlock on outstanding buffer references.
    if (current_back_buffer)
    {
        current_back_buffer->Release();
        current_back_buffer = nullptr;
    }

    IDXGISwapChain* dxgi_swap_chain = (IDXGISwapChain*)swap_chain;

    DXGI_SWAP_CHAIN_DESC swap_chain_description;
    HRESULT result = dxgi_swap_chain->GetDesc(&swap_chain_description);
    if (FAILED(result))
    {
        PLOG_WARNING << "Failed to get swapchain description while updating render target. Error: " << result;
        return;
    }
    target_window = swap_chain_description.OutputWindow;

    // The current backbuffer index requires IDXGISwapChain3.
    IDXGISwapChain3* swap_chain3 = nullptr;
    result = dxgi_swap_chain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swap_chain3);
    if (FAILED(result) || !swap_chain3)
    {
        PLOG_WARNING << "Failed to query IDXGISwapChain3 while updating render target. Error: " << result;
        return;
    }

    current_buffer_index = swap_chain3->GetCurrentBackBufferIndex();
    result = swap_chain3->GetBuffer(current_buffer_index, __uuidof(ID3D12Resource), (void**)&current_back_buffer);
    swap_chain3->Release();
    if (FAILED(result) || !current_back_buffer)
    {
        PLOG_WARNING << "Failed to get the DirectX 12 backbuffer while updating render target. Error: " << result;
        current_back_buffer = nullptr;
        return;
    }

    if (current_buffer_index >= buffer_count)
    {
        // Swap chain was recreated with more buffers than we allocated for; play it safe.
        PLOG_WARNING << "Backbuffer index " << current_buffer_index << " exceeds allocated buffer count " << buffer_count << "; skipping frame.";
        current_back_buffer->Release();
        current_back_buffer = nullptr;
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += (SIZE_T)current_buffer_index * rtv_descriptor_size;
    d3d12_device->CreateRenderTargetView(current_back_buffer, nullptr, rtv_handle);
}

void D3D12GraphicsApi::Render()
{
    if (!d3d12_command_queue || !d3d12_command_list || !current_back_buffer)
    {
        if (current_back_buffer)
        {
            current_back_buffer->Release();
            current_back_buffer = nullptr;
        }
        return;
    }

    ID3D12CommandAllocator* frame_allocator = command_allocators[current_buffer_index];
    frame_allocator->Reset();
    d3d12_command_list->Reset(frame_allocator, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = current_back_buffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    d3d12_command_list->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += (SIZE_T)current_buffer_index * rtv_descriptor_size;
    d3d12_command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
    d3d12_command_list->SetDescriptorHeaps(1, &srv_descriptor_heap);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12_command_list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    d3d12_command_list->ResourceBarrier(1, &barrier);
    d3d12_command_list->Close();

    ID3D12CommandList* command_lists[] = { d3d12_command_list };
    d3d12_command_queue->ExecuteCommandLists(1, command_lists);

    // Release the per-frame backbuffer reference.
    current_back_buffer->Release();
    current_back_buffer = nullptr;
}

bool D3D12GraphicsApi::ExecuteAndWait(ID3D12CommandList* command_list)
{
    if (!d3d12_command_queue || !upload_fence || !upload_fence_event)
    {
        return false;
    }

    ID3D12CommandList* command_lists[] = { command_list };
    d3d12_command_queue->ExecuteCommandLists(1, command_lists);

    const UINT64 signal_value = ++upload_fence_value;
    if (FAILED(d3d12_command_queue->Signal(upload_fence, signal_value)))
    {
        PLOG_ERROR << "Failed to signal the D3D12 upload fence.";
        return false;
    }

    if (upload_fence->GetCompletedValue() < signal_value)
    {
        if (FAILED(upload_fence->SetEventOnCompletion(signal_value, upload_fence_event)))
        {
            PLOG_ERROR << "Failed to arm the D3D12 upload fence event.";
            return false;
        }
        WaitForSingleObject(upload_fence_event, 5000);
    }

    return upload_fence->GetCompletedValue() >= signal_value;
}

void* D3D12GraphicsApi::CreateTextureFromMemory(const void* pixels, int width, int height)
{
    if (!d3d12_device || !d3d12_command_queue)
    {
        PLOG_WARNING << "CreateTextureFromMemory called without an initialized device/command queue.";
        return nullptr;
    }

    if (!pixels || width <= 0 || height <= 0)
    {
        PLOG_WARNING << "CreateTextureFromMemory called with invalid arguments (pixels=" << pixels
                     << ", width=" << width << ", height=" << height << ").";
        return nullptr;
    }

    // Destination texture in GPU-local memory.
    D3D12_HEAP_PROPERTIES default_heap = {};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texture_description = {};
    texture_description.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_description.Width            = width;
    texture_description.Height           = height;
    texture_description.DepthOrArraySize = 1;
    texture_description.MipLevels        = 1;
    texture_description.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_description.SampleDesc.Count = 1;

    ID3D12Resource* texture = nullptr;
    HRESULT result = d3d12_device->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &texture_description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), (void**)&texture);
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to create D3D12 texture resource. Returned HRESULT: " << result;
        return nullptr;
    }

    // Upload buffer. Texture copies require rows padded to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT.
    const UINT source_pitch  = (UINT)width * 4;
    const UINT aligned_pitch = (source_pitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const UINT64 upload_size = (UINT64)aligned_pitch * height;

    D3D12_HEAP_PROPERTIES upload_heap = {};
    upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC upload_description = {};
    upload_description.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_description.Width            = upload_size;
    upload_description.Height           = 1;
    upload_description.DepthOrArraySize = 1;
    upload_description.MipLevels        = 1;
    upload_description.SampleDesc.Count = 1;
    upload_description.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource* upload_buffer = nullptr;
    result = d3d12_device->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &upload_description,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&upload_buffer);
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to create D3D12 upload buffer. Returned HRESULT: " << result;
        texture->Release();
        return nullptr;
    }

    // Copy pixel rows into the (padded) upload buffer.
    void* mapped = nullptr;
    result = upload_buffer->Map(0, nullptr, &mapped);
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to map D3D12 upload buffer. Returned HRESULT: " << result;
        upload_buffer->Release();
        texture->Release();
        return nullptr;
    }
    for (int row = 0; row < height; row++)
    {
        memcpy((uint8_t*)mapped + (SIZE_T)row * aligned_pitch, (const uint8_t*)pixels + (SIZE_T)row * source_pitch, source_pitch);
    }
    upload_buffer->Unmap(0, nullptr);

    // Record the copy on a one-shot command list and wait for it, so the upload buffer can
    // be released immediately. Texture creation is rare enough that blocking is fine.
    ID3D12CommandAllocator* upload_allocator = nullptr;
    ID3D12GraphicsCommandList* upload_command_list = nullptr;
    result = d3d12_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&upload_allocator);
    if (SUCCEEDED(result))
    {
        result = d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, upload_allocator, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&upload_command_list);
    }
    if (FAILED(result))
    {
        PLOG_ERROR << "Failed to create D3D12 upload command list. Returned HRESULT: " << result;
        if (upload_allocator) upload_allocator->Release();
        upload_buffer->Release();
        texture->Release();
        return nullptr;
    }

    D3D12_TEXTURE_COPY_LOCATION copy_destination = {};
    copy_destination.pResource        = texture;
    copy_destination.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copy_destination.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION copy_source = {};
    copy_source.pResource                          = upload_buffer;
    copy_source.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copy_source.PlacedFootprint.Footprint.Format   = DXGI_FORMAT_R8G8B8A8_UNORM;
    copy_source.PlacedFootprint.Footprint.Width    = width;
    copy_source.PlacedFootprint.Footprint.Height   = height;
    copy_source.PlacedFootprint.Footprint.Depth    = 1;
    copy_source.PlacedFootprint.Footprint.RowPitch = aligned_pitch;

    upload_command_list->CopyTextureRegion(&copy_destination, 0, 0, 0, &copy_source, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = texture;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    upload_command_list->ResourceBarrier(1, &barrier);
    upload_command_list->Close();

    const bool upload_ok = ExecuteAndWait(upload_command_list);
    upload_command_list->Release();
    upload_allocator->Release();
    upload_buffer->Release();
    if (!upload_ok)
    {
        PLOG_ERROR << "D3D12 texture upload did not complete.";
        texture->Release();
        return nullptr;
    }

    // Allocate an SRV slot for the texture and hand back the GPU descriptor handle,
    // which is what the ImGui DX12 backend uses as its texture identifier.
    const UINT slot = AllocateSrvSlot();
    if (slot == UINT_MAX)
    {
        texture->Release();
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_description = {};
    srv_description.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_description.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_description.Texture2D.MipLevels     = 1;
    d3d12_device->CreateShaderResourceView(texture, &srv_description, GetSrvCpuHandle(slot));

    const D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = GetSrvGpuHandle(slot);
    texture_registry[gpu_handle.ptr] = TextureRecord{ texture, slot };

    PLOG_DEBUG << "D3D12 texture created from memory (" << width << "x" << height << "), SRV slot " << slot;
    return (void*)gpu_handle.ptr;
}

void* D3D12GraphicsApi::CreateTextureFromFile(const std::wstring& file_path)
{
    // WICTextureLoader is D3D11-only, so decode the image with WIC directly and feed the
    // RGBA pixels through the shared upload path.
    PLOG_DEBUG << "Creating D3D12 texture from file";

    const HRESULT co_init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool co_initialized = SUCCEEDED(co_init);    // RPC_E_CHANGED_MODE means COM was already up in another mode; keep going.

    void* texture_handle = nullptr;
    IWICImagingFactory* wic_factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    do
    {
        HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_factory));
        if (FAILED(result)) { PLOG_ERROR << "Failed to create WIC imaging factory. HRESULT: " << result; break; }

        result = wic_factory->CreateDecoderFromFilename(file_path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(result)) { PLOG_ERROR << "Failed to decode image file. HRESULT: " << result; break; }

        result = decoder->GetFrame(0, &frame);
        if (FAILED(result)) { PLOG_ERROR << "Failed to get image frame. HRESULT: " << result; break; }

        result = wic_factory->CreateFormatConverter(&converter);
        if (FAILED(result)) { PLOG_ERROR << "Failed to create WIC format converter. HRESULT: " << result; break; }

        result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(result)) { PLOG_ERROR << "Failed to convert image to 32bpp RGBA. HRESULT: " << result; break; }

        UINT width = 0;
        UINT height = 0;
        result = converter->GetSize(&width, &height);
        if (FAILED(result) || !width || !height) { PLOG_ERROR << "Failed to get image size. HRESULT: " << result; break; }

        std::vector<uint8_t> pixels((size_t)width * height * 4);
        result = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
        if (FAILED(result)) { PLOG_ERROR << "Failed to copy decoded pixels. HRESULT: " << result; break; }

        texture_handle = CreateTextureFromMemory(pixels.data(), (int)width, (int)height);
    } while (false);

    if (converter)   converter->Release();
    if (frame)       frame->Release();
    if (decoder)     decoder->Release();
    if (wic_factory) wic_factory->Release();
    if (co_initialized) CoUninitialize();

    return texture_handle;
}

void D3D12GraphicsApi::ReleaseTexture(void* texture)
{
    if (!texture)
    {
        return;
    }

    // D3D12 texture handles are GPU descriptor handle values, not COM pointers.
    auto record = texture_registry.find((UINT64)texture);
    if (record == texture_registry.end())
    {
        PLOG_WARNING << "ReleaseTexture called with an unknown D3D12 texture handle: " << texture;
        return;
    }

    record->second.resource->Release();
    if (record->second.heap_index < srv_slot_used.size())
    {
        srv_slot_used[record->second.heap_index] = false;
    }
    texture_registry.erase(record);
}

void D3D12GraphicsApi::ShutdownImGuiImpl()
{
    ImGui_ImplDX12_Shutdown();
}

void D3D12GraphicsApi::Cleanup(void* params)
{
    // Release any user textures still alive.
    for (auto& entry : texture_registry)
    {
        entry.second.resource->Release();
    }
    texture_registry.clear();
    srv_slot_used.clear();

    if (current_back_buffer)
    {
        current_back_buffer->Release();
        current_back_buffer = nullptr;
    }

    if (d3d12_command_list)
    {
        d3d12_command_list->Release();
        d3d12_command_list = nullptr;
    }

    for (ID3D12CommandAllocator* allocator : command_allocators)
    {
        if (allocator)
        {
            allocator->Release();
        }
    }
    command_allocators.clear();

    if (rtv_descriptor_heap)
    {
        rtv_descriptor_heap->Release();
        rtv_descriptor_heap = nullptr;
    }

    if (srv_descriptor_heap)
    {
        srv_descriptor_heap->Release();
        srv_descriptor_heap = nullptr;
    }

    if (upload_fence)
    {
        upload_fence->Release();
        upload_fence = nullptr;
    }

    if (upload_fence_event)
    {
        CloseHandle(upload_fence_event);
        upload_fence_event = nullptr;
    }

    if (d3d12_command_queue)
    {
        d3d12_command_queue->Release();     // Balances the AddRef taken when the queue was captured.
        d3d12_command_queue = nullptr;
    }

    if (d3d12_device)
    {
        d3d12_device->Release();
        d3d12_device = nullptr;
    }

    initialized = false;
}

D3D12GraphicsApi::~D3D12GraphicsApi()
{
    Cleanup(nullptr);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          VulkanGraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                          OpenGLGraphicsApi Class                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝
