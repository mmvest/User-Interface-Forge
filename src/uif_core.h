#pragma once
#include <d3d11.h>
#include <dxgi.h>

void CreateTestWindow();
bool AllocateDebugConsole();
bool LoadUiMods();
void ExecuteUiMods();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HRESULT __stdcall hooked_d3d11_present_func(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
DWORD WINAPI CoreMain(LPVOID lpParam);