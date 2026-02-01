#pragma once
#include <Windows.h>
#include <string>

namespace CoreUtils
{
    /**
     * @brief Displays an error message in a message box.
     *
     * This function opens a modal message box with the specified error message and
     * an error icon.
     *
     * @param err_msg The error message to display.
     */
    void ErrorMessageBox(std::string err_msg);

    /**
     * @brief Displays an informational message in a message box.
     *
     * This function opens a modal message box with the specified informational message.
     *
     * @param info_msg The informational message to display.
     */
    void InfoMessageBox(std::string info_msg);

    /**
     * @brief Processes custom input actions for UiForge.
     *
     * This function monitors specific keyboard input to trigger custom actions, such as cleanup operations.
     */
    void ProcessCustomInputs(HWND target_window);
}
