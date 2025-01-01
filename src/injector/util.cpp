#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include "injector\util.h"
#include "plog\Log.h"

unsigned long GetProcessIdByName(const wchar_t* process_name)
/**
 * @brief Retrieves the process ID (PID) of a running process by its name.
 *
 * This function searches through the currently running processes and matches
 * their names with the specified name. If a match is found, the corresponding
 * process ID is returned. If no matching process is found, the function
 * returns zero.
 *
 * @param process_name A pointer to a wide character string (wchar_t*) representing
 *                     the name of the process to search for. The name SHOULD 
 *                     include the file extension (e.g., "notepad.exe" instead of "notepad").
 *
 * @return The process ID (PID) of the matching process if found; otherwise, returns
 *         zero indicating that no matching process was found.
 */

{
    unsigned long process_id = 0;
    PROCESSENTRY32W process_entry = { 0 };
    HANDLE process_list_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (process_list_snapshot == INVALID_HANDLE_VALUE)
    {
        PLOG_FATAL << "[!] Failed to get process snapshot. Error: " << GetLastError();
        goto cleanup;
    }

    process_entry.dwSize = sizeof(PROCESSENTRY32W);
    if (!Process32FirstW(process_list_snapshot, &process_entry))
    {
        PLOG_FATAL << "[!] Failed to get first process from snapshot. Error: " << GetLastError();
        goto cleanup;
    }

    do {
        BOOL process_name_matches = !wcsncmp(process_name, process_entry.szExeFile, MAX_PATH);
        if (process_name_matches) {
            process_id = process_entry.th32ProcessID;
            break;
        }
    } while (Process32NextW(process_list_snapshot, &process_entry));

    cleanup:
    if(process_list_snapshot != INVALID_HANDLE_VALUE && process_list_snapshot) CloseHandle(process_list_snapshot);
    return process_id;
}

size_t GetSizeOfWStringBytes(const std::wstring& str)
/**
 * @brief Calculates the size in bytes of a wide string (std::wstring).
 * 
 * This function takes a wide string as input and returns the total number of bytes
 * required to store the string, including the null terminator. The size is calculated
 * by multiplying the length of the wide string by the size of a wide character (wchar_t).
 *
 * @param str The wide string for which the size in bytes is to be calculated.
 * @return size_t The total size in bytes of the wide string, including the null terminator.
 */
{
    return ((str.size() + 1) * sizeof(wchar_t)); // add one for the null terminator
}

size_t GetTotalSizeBytesOfWStrings(std::vector<std::wstring>& str_vec)
/**
 * @brief Calculates the total size in bytes of a vector of wide strings (std::vector<std::wstring>).
 * 
 * This function takes a vector of wide strings as input and computes the total number of bytes 
 * required to store all the strings in the vector, including the null terminators for each string. 
 * The size for each string is calculated by multiplying its length by the size of a wide character 
 * (wchar_t), and the results are summed to obtain the total size.
 *
 * @param str_vec The vector of wide strings for which the total size in bytes is to be calculated.
 * @return size_t The total size in bytes of all the wide strings in the vector, including null terminators.
 */
{
    size_t total = 0;
    for (const std::wstring& str : str_vec)
    {
        total += GetSizeOfWStringBytes(str); 
    }
    return total;
}

std::wstring ConcatenateWStrings(std::vector<std::wstring>& str_vec)
/**
 * @brief Concatenates a vector of wide strings (std::vector<std::wstring>) into a single wide string.
 * 
 * This function takes a vector of wide strings as input and combines them into a single wide string. 
 * Each string in the vector is appended in the order they appear, resulting in a continuous wide 
 * string including the null terminators. The resulting string includes all the characters 
 * from the input wide strings.
 *
 * @param str_vec The vector of wide strings to be concatenated.
 * @return std::wstring The concatenated wide string formed by appending all the strings from the vector.
 */
{
    std::wstring result;

    for (const std::wstring& str : str_vec) {
        PLOG_DEBUG << L"Concatenating: " << str;
        result += str;
        result.push_back(L'\0'); // Be sure to include the null bytes!
    }

    return result;
}