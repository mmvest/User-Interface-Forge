@echo off

::======================================================================================================
:: File:            build_uiforge.bat
:: Description:     This script will build UiForge.dll for you as well as
::                  StartUiForge.exe.
::
:: Usage:           build_uiforge.bat <graphics_api> <architecture>
::
:: Example:         build_uiforge.bat d3d11 x86_64
::
:: Author:          mmvest (wereox)
:: Date:            2024-09-26
::
:: Requirements:    VS2022
::                  vcvars64.bat (VS2022 Command Prompt)
::                  Graphics API of choice installed/downloaded and added to your lib/includes path
::
:: Version:         0.1.0
:: Changelog:       0.1.0: Initial rough draft version.
::
:: Notes:           I plan to add 32-bit support and support for more graphics APIs in the future.
::                  I have already done a lot of the ground work to support 32-bit and other APIs
::                  but until uif_core.cpp supports it, only the DirectX 11 64-bit build will work. 
::
::======================================================================================================

set graphics_api=%1
set architecture=%2
set debug=%3

set "PRINT_USAGE="
if "%graphics_api%"=="" set PRINT_USAGE=1
if "%graphics_api%"=="help" set PRINT_USAGE=1
if defined PRINT_USAGE (
    echo:
    echo Usage: %~nx0 ^<graphics_api^> ^<architecture^>
    echo ---------------------------------------------------------------------------------------------------
    echo:
    echo    graphics_api        Which graphics api to compile for. One of d3d9, d3d10, d3d11, d3d12, vulkan, opengl.
    echo:
    echo    architecture        Which architecture to use. Either 32 or 64. Defaults to 64
    echo:
    echo    help                Display this usage message
    echo:
    pause
    exit /b
)

set CWD=%~dp0
set SRC_DIR=%CWD%src
set OUTPUT_DIR=%CWD%bin
set LIBS_DIR=%CWD%libs
set IMGUI_DIR=%LIBS_DIR%\imgui
set KIERO_DIR=%LIBS_DIR%\kiero
set MINHOOK_DIR=%KIERO_DIR%\minhook
set "INJECTOR_SRC=%SRC_DIR%\uif_injector.cpp %OUTPUT_DIR%\injector_util.obj"

REM determine whether to compile for 32 or 64-bit
if "%architecture%"=="32" (
    set VCVAR=vcvars32.bat
) else (
    set VCVAR=vcvars64.bat
)

REM determine whether to add _DEBUG preprocessor value
if "%debug%"=="-d" (
    set "DEBUG_FLAG=/D_DEBUG /Zi /Od /MDd"
) else (
    set "DEBUG_FLAG="
)


REM RUN_VCVARS can be used to open VS2022 command prompt -- either 32 or 64-bit
set "RUN_VCVARS=start %comspec% /k ^"C:\Program^ Files\Microsoft^ Visual^ Studio\2022\Community\VC\Auxiliary\Build\%VCVAR%^""

REM variables for the various linking and compiler needs
set CSTD=/std:c++17

set "IMPL=%IMGUI_DIR%\backends\imgui_impl_"
set "IMGUI_SUPPORTED_BACKENDS=%IMPL%dx*.cpp %IMPL%win32.cpp"
set "IMGUI_SRC=%IMGUI_DIR%\*.cpp "
set "IMGUI_INCLUDES=/I%IMGUI_DIR% /I%IMGUI_DIR%\backends"
set "IMGUI_OBJS=%OUTPUT_DIR%\imgui\*.obj"

set "KIERO_SRC=%KIERO_DIR%\*.cpp %MINHOOK_DIR%\src\*.c %MINHOOK_DIR%\src\hde\*.c"
set "KIERO_INCLUDES=/I%KIERO_DIR% /I%MINHOOK_DIR%\src /I%MINHOOK_DIR%\src\hde /I%MINHOOK_DIR%\include"
set "KIERO_OBJS=%OUTPUT_DIR%\kiero\*.obj"

REM Libraries to link based on the graphics API
set "LINK_D3D9=/link d3d9.lib"
set "LINK_D3D10=/link d3d10.lib d3dcompiler.lib"
set "LINK_D3D11=/link d3d11.lib d3dcompiler.lib"
set "LINK_D3D12=/link d3d12.lib d3dcompiler.lib dxgi.lib"
set "LINK_VULKAN="
set "LINK_OPENGL="

REM Variables for building the injector (StartUiForge.exe)
set "BUILD_INJECTOR_UTIL=cl /EHsc /c %SRC_DIR%\injector_util.cpp /Fo:%OUTPUT_DIR%\injector_util.obj"
set "BUILD_INJECTOR=%BUILD_INJECTOR_UTIL% && cl /EHsc %INJECTOR_SRC% /Fe:%OUTPUT_DIR%\StartUiForge.exe /Fo:%OUTPUT_DIR%\StartUiForge.obj %CSTD%"

REM Commands to compile object files for imgui and kiero and place them in their respective output directories
set "BUILD_IMGUI=cd %OUTPUT_DIR%\imgui && cl /c %IMGUI_SRC% %IMGUI_SUPPORTED_BACKENDS% %IMGUI_INCLUDES% && cd %CWD%"
set "BUILD_KIERO=cd %OUTPUT_DIR%\kiero && cl /c %KIERO_SRC% %KIERO_INCLUDES% && cd %CWD%"

REM The main command for building uif_core.dll
set "BUILD_CORE=cl /EHsc /LD %SRC_DIR%\uif_core.cpp %IMGUI_OBJS% %KIERO_OBJS% /Fe:%OUTPUT_DIR%\uif_core.dll /Fo:%OUTPUT_DIR%\uif_core.obj %CSTD% %IMGUI_INCLUDES% %KIERO_INCLUDES% %DEBUG_FLAG%"

REM Check if OUTPUT_DIR directory exists. If it does not, then make it.
if not exist %OUTPUT_DIR% (
    mkdir %OUTPUT_DIR%
)
if not exist %OUTPUT_DIR%\imgui (
    mkdir %OUTPUT_DIR%\imgui
)
if not exist %OUTPUT_DIR%\kiero (
    mkdir %OUTPUT_DIR%\kiero
)

goto %graphics_api%

REM Add 32-bit builds

:d3d9
REM TODO: Implement build for DirectX 9
:: %RUN_VCVARS% && %BUILD_INJECTOR% && %BUILD_IMGUI% && %BUILD_KIERO% && %BUILD_CORE% %LINK_D3D9%
exit /b

:d3d10
REM TODO: Implement build for DirectX 10
:: %RUN_VCVARS% && %BUILD_INJECTOR% && %BUILD_IMGUI% && %BUILD_KIERO% && %BUILD_CORE% %LINK_D3D10%
exit /b

:d3d11
%RUN_VCVARS% && %BUILD_INJECTOR% && %BUILD_IMGUI% && %BUILD_KIERO% && %BUILD_CORE% %LINK_D3D11%
exit /b

:d3d12
REM TODO: Implement build for DirectX 12
:: %RUN_VCVARS% && %BUILD_INJECTOR% && %BUILD_IMGUI% && %BUILD_KIERO% && %BUILD_CORE% %LINK_D3D12%
exit /b

:vulkan
REM TODO: Implement build for Vulkan
:: %RUN_VCVARS% && %BUILD_INJECTOR% && %BUILD_IMGUI% && %BUILD_KIERO% && %BUILD_CORE% %LINK_VULKAN%
exit /b

:opengl
REM TODO: Implement build for OpenGL
:: %RUN_VCVARS% && %BUILD_INJECTOR% && %BUILD_IMGUI% && %BUILD_KIERO% && %BUILD_CORE% %LINK_OPENGL%

:injector
%RUN_VCVARS% && %BUILD_INJECTOR% && exit /b
