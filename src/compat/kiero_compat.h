#pragma once

// This header is meant to be force-included (MSVC: /FI) when compiling kiero.cpp.
//
// I ran into an issue caused by `externals/kiero/kiero.h` defining its feature macros unconditionally,
// so build flags like `/D KIERO_INCLUDE_D3D11=1` don't work because the header clobbers them.
//
// So, the fix? Just undefine them here and then define the macros I want in the build script. Not the
// prettiest solution, but it does mean I don't need to change kiero source.

// Include once to trip the include-guard before kiero.cpp's own `#include "kiero.h"`.
#include "kiero.h"

#undef KIERO_INCLUDE_D3D11
#undef KIERO_USE_MINHOOK

// Define our values (0 or 1). Build should pass these as `/DUIFORGE_...=1`.
#ifndef UIFORGE_KIERO_INCLUDE_D3D11
#  define UIFORGE_KIERO_INCLUDE_D3D11 0
#endif
#ifndef UIFORGE_KIERO_USE_MINHOOK
#  define UIFORGE_KIERO_USE_MINHOOK 0
#endif

#define KIERO_INCLUDE_D3D11 UIFORGE_KIERO_INCLUDE_D3D11
#define KIERO_USE_MINHOOK   UIFORGE_KIERO_USE_MINHOOK
