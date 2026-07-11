/**
 * @file uiforge_imconfig.h
 * @brief UiForge ImGui compile-time config, injected via IMGUI_USER_CONFIG.
 *
 * Routes IM_ASSERT to a logged C++ exception instead of the CRT assert dialog.
 * UiForge lives inside someone else's process, so an abort() takes the host
 * application down with it. The exception is caught at the frame boundary
 * (OnGraphicsApiInvoke) or the per-script boundary (RunScripts), which disables
 * the offending script and keeps the host alive.
 */
#pragma once

/**
 * @brief Logs the failed ImGui assertion and throws std::runtime_error.
 *
 * Defined in core.cpp. Never returns.
 *
 * @param expression The stringified expression that failed.
 * @param file Source file of the assertion.
 * @param line Source line of the assertion.
 */
[[noreturn]] void UiForgeImGuiAssertFail(const char* expression, const char* file, int line);

#define IM_ASSERT(_EXPR) ((_EXPR) ? (void)0 : UiForgeImGuiAssertFail(#_EXPR, __FILE__, __LINE__))
