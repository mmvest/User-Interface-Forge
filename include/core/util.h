#pragma once
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
    void ErrorMessageBox(const char* err_msg);

    /**
     * @brief Displays an informational message in a message box.
     *
     * This function opens a modal message box with the specified informational message.
     *
     * @param info_msg The informational message to display.
     */
    void InfoMessageBox(const char* info_msg);

    /**
     * @brief Processes custom input actions for UiForge.
     *
     * This function monitors specific keyboard input (e.g., `VK_END`) to trigger
     * custom actions, such as cleanup operations.
     */
    void ProcessCustomInputs();
}
