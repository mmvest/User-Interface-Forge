#pragma once
#include <unordered_map>
unsigned long GetProcessIdByName(const wchar_t* process_name);
size_t GetSizeOfWStringBytes(std::wstring str);
size_t GetTotalSizeBytesOfWStrings(std::vector<std::wstring> str_vec);
std::wstring ConcatenateWStrings(std::vector<std::wstring> str_vec);

// ===================== BITNESS STRUCTS ===================== 
enum class Bitness
{
    x86,
    x86_64,
    UNKNOWN
};
extern std::unordered_map<std::wstring, Bitness> bitness_map;
Bitness GetBitnessFromWString(const std::wstring& str);

//  ===================== GRAPHICS API STRUCTS =====================
enum class GraphicsAPI
{
    DIRECTX9,
    DIRECTX10,
    DIRECTX11,
    DIRECTX12,
    VULKAN,
    UNKNOWN
};
extern std::unordered_map<std::wstring, GraphicsAPI> api_map;
GraphicsAPI GetApiFromWString(const std::wstring& str);

// ===================== USER OPTIONS -- USED FOR ARGUMENT PARSING =====================
struct UserOptions
{
    std::wstring target_process_name;
    Bitness bitness = Bitness::UNKNOWN;
    GraphicsAPI api = GraphicsAPI::UNKNOWN;
    std::vector<std::wstring> modules;
};


