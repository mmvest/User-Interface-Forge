#pragma once

#include <vector>
#include <string>

unsigned long GetProcessIdByName(const wchar_t* process_name);
size_t GetSizeOfWStringBytes(std::wstring str);
size_t GetTotalSizeBytesOfWStrings(std::vector<std::wstring> str_vec);
std::wstring ConcatenateWStrings(std::vector<std::wstring> str_vec);

// ===================== USER OPTIONS -- USED FOR ARGUMENT PARSING =====================
struct UserOptions
{
    std::wstring target_process_name;
    std::vector<std::wstring> modules;
};


