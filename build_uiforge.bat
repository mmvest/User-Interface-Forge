@echo off
setlocal
::======================================================================================================
:: File:            build_uiforge.bat
:: Description:     This script will build UiForge.dll for you as well as
::                  StartUiForge.exe.
::
:: Usage:           build_uiforge.bat <architecture> <graphics_api>
::
:: Example:         build_uiforge.bat d3d11 64
::
:: Author:          mmvest (wereox)
:: Date:            2024-09-26
::
:: Requirements:    VS2022
::                  vcvars64.bat (VS2022 Command Prompt)
::                  Graphics API of choice installed/downloaded and added to your lib/includes path
::
:: Version:         0.3.0 
:: Changelog:       0.3.0   - Removed options for DirectX9 and DirectX10 -- I don't plan to support them        (2024-11-11)
::                          - Swapped Architecture and Graphics API argument positions so that it will be       (2024-11-11)
::                            consitent with the injector argument list.
::                          - Removed an option to build a test font resource                                   (2024-11-11)
::                  0.2.0:  - Cleaned up script by removing unnecessary bloat in variables and build commands   (2024-10-02)
::                          - No longer build imgui, kiero, or minhook -- libraries are included instead        (2024-10-02)
::                          - Changed to accomodate new directory layout                                        (2024-10-02)
::                          - Now includes build for fonts resourcce                                            (2024-10-02)
::                  0.1.0:  - Initial rough draft version.                                                      (2024-09-26)
::
:: Notes:           I plan to add 32-bit support and support for more graphics APIs in the future.
::                  I have already done a lot of the ground work to support 32-bit and other APIs
::                  but until uif_core.cpp supports it, only the DirectX 11 64-bit build will work. 
::
::======================================================================================================

set architecture=%1
set graphics_api=%2
set debug=%3

set "PRINT_USAGE="
if "%graphics_api%"=="" set PRINT_USAGE=1
if "%graphics_api%"=="help" set PRINT_USAGE=1
if defined PRINT_USAGE (
    echo:
    echo Usage: %~nx0 ^<graphics_api^> ^<architecture^>
    echo ---------------------------------------------------------------------------------------------------
    echo:
    echo    graphics_api        Which graphics api to compile for. One of d3d11, d3d12, vulkan, opengl.
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
set BIN_DIR=%CWD%bin
set LIBS_DIR=%CWD%libs
set INCLUDE_DIR=%CWD%include

REM determine whether to compile for 32 or 64-bit
if "%architecture%"=="32" (
    set VCVAR=vcvars32.bat
) else (
    set VCVAR=vcvars64.bat
)

REM determine whether to add _DEBUG preprocessor value
if "%debug%"=="-d" (
    set "DEBUG_FLAG=/D_DEBUGPRINT"
) else (
    set "DEBUG_FLAG="
)


REM RUN_VCVARS can be used to open VS2022 command prompt -- either 32 or 64-bit
set RUN_VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\%VCVAR%"

REM variables for the various linking and compiler needs
set CSTD=/std:c++17

REM Libraries to link based on the graphics API
set LINK_IMGUI=libs\imgui_directx11_1.91.2.lib
set LINK_D3D11=d3d11.lib d3dcompiler.lib libs\imgui_directx11_1.91.2.lib libs\kiero_directx11.lib libs\minhook_x64.lib
set LINK_D3D12="d3d12.lib d3dcompiler.lib dxgi.lib"
set LINK_VULKAN=""
set LINK_OPENGL=""

REM Source files for each build
set INJECTOR_UTIL_SRC="%SRC_DIR%\injector\injector_util.cpp"
set INJECTOR_SRC=%SRC_DIR%\injector\uif_injector.cpp %BIN_DIR%\injector_util.obj
set CORE_SRC=%SRC_DIR%\core\uif_core.cpp %BIN_DIR%\*.obj
set UI_MGR_SRC=%SRC_DIR%\core\ui_manager.cpp
set CORE_UTIL_SRC=%SRC_DIR%\core\core_utils.cpp
set SCRIPT_MGR_SRC=%SRC_DIR%\core\forgescript_manager.cpp
set GRAPHICS_API_SRC=%SRC_DIR%\core\graphics_api.cpp

REM Variables for building the injector (StartUiForge.exe)

set BUILD_INJECTOR="cl /EHsc /I %INCLUDE_DIR% /Fe:%BIN_DIR%\StartUiForge.exe /Fo:%BIN_DIR%\StartUiForge.obj %CSTD% %INJECTOR_SRC%"

REM Check if BIN_DIR directory exists. If it does not, then make it.
if not exist %BIN_DIR% (
    mkdir %BIN_DIR%
)

REM vcvars initializes the vs2022 terminal
call %RUN_VCVARS%
goto %graphics_api%

REM Add 32-bit builds when I get the chance

:d3d11
echo Building Core Utils...
cl /c /Fo:%BIN_DIR%\core_utils.obj %CORE_UTIL_SRC%
echo(

echo Building Script Manager...
cl /c /Fo:%BIN_DIR%\forgescript_manager.obj %SCRIPT_MGR_SRC%
echo(

echo Building UI Manager...
cl /c /Fo:%BIN_DIR%\ui_manager.obj %UI_MGR_SRC%
echo(

echo Building Graphics Api Manager...
cl /EHsc /c /Fo:%BIN_DIR%\graphics_api.obj %GRAPHICS_API_SRC%
echo(

echo Building Core...
cl /EHsc /LD /I %INCLUDE_DIR% /I /Fe:%BIN_DIR%\uif_core.dll %DEBUG_FLAG% %CSTD% %CORE_SRC% /link %LINK_D3D11%
move /Y uif_core.dll .\bin\uif_core.dll
echo(
goto cleanup

:d3d12
REM TODO: Implement build for DirectX 12
exit /b

:vulkan
REM TODO: Implement build for Vulkan
exit /b

:opengl
REM TODO: Implement build for OpenGL

:injector
echo Building injector utility...
cl /EHsc /c /I %INCLUDE_DIR% /Fo:%BIN_DIR%\injector_util.obj %INJECTOR_UTIL_SRC%

echo Building injector ^(StartUiForge.exe^)
cl /EHsc /I %INCLUDE_DIR% /Fe:%BIN_DIR%\StartUiForge.exe /Fo:%BIN_DIR%\StartUiForge.obj %CSTD% %INJECTOR_SRC%

goto cleanup

:cleanup
echo Cleaning up
del %BIN_DIR%\*.obj
del *.obj

echo Build Complete!
exit /b

endlocal