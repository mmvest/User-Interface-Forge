#pragma once
#include <Windows.h>

/**
 * @brief One-time environment inventories written to the core log.
 *
 * The core log records what UiForge did, but not what it attached itself to. These dumps
 * record the swapchain, the process windows, and the loaded modules so a user's log file
 * alone is enough to tell which surface we render into, which window owns our input, and
 * what else is hooking the process.
 */
namespace CoreDiagnostics
{
    /**
     * @brief Logs every module loaded in the current process with its address range and path.
     *
     * Also calls out the graphics runtimes specifically and warns when one is loaded from
     * outside the system directory, which means a proxy DLL owns the present path.
     */
    void LogModuleInventory();

    /**
     * @brief Logs every enumerable window owned by this process, tagged against our own state.
     *
     * Entries are tagged with the swapchain output window, the window ImGui was initialized
     * against, the foreground window, and whether we subclassed it, so the log shows directly
     * which window we render into and whether our input gate can ever match.
     *
     * @param reason Short description of what triggered the dump, recorded in the header line.
     */
    void LogWindowInventory(const char* reason);

    /**
     * @brief Records a presented swapchain, logging its details the first time it is seen.
     *
     * Called on every present, so this stays a pointer comparison and a counter after the
     * first few frames. Also emits a periodic heartbeat so a log can distinguish "the hook
     * fired once and stopped" from "we are rendering but nothing is visible".
     *
     * @param swap_chain The swapchain passed to the present hook.
     */
    void LogSwapChainIfNew(void* swap_chain);

    /**
     * @brief Logs the details of a single swapchain, whether or not it has been seen before.
     *
     * @param swap_chain The swapchain to describe.
     * @param reason     Short description of what triggered the dump.
     */
    void LogSwapChainInfo(void* swap_chain, const char* reason);

    /**
     * @brief Records a change of render target window and dumps the window inventory again.
     *
     * The first few changes are logged in full. After that only a running count is reported,
     * because two swapchains alternating would otherwise flood the log at frame rate.
     *
     * @param new_window The window we just switched to.
     */
    void OnTargetWindowChanged(HWND new_window);
}
