@echo off
setlocal

::======================================================================================================
:: File:            build_uiforge.bat
:: Description:     Builds UiForge, including the injector, Lua bindings, and core, with all graphics APIs.
::                  Simplified to support 64-bit builds. Places UiForge.exe in the working directory.
::
:: Usage:           build_uiforge.bat
::
:: Example:         build_uiforge.bat
::
:: Author:          mmvest (wereox)
:: Date:            2024-09-26
::
::
:: Version:         0.4.1 
:: Changelog:       0.4.1   - Changed the build so that its much more straightforward                           (2024-11-25)
::                  0.4.0   - Removed 32-bit "support", only builds for 64-bit                                  (2024-11-24)
::                          - Renamed StartUiForge.exe to UiForge.exe
::                          - Simplified logic and arguments for maintainability
::                          - Removed debug flag support
::                  0.3.1   - Support for building with Lua and other small changes
::                  0.3.0   - Removed options for DirectX9 and DirectX10 -- I don't plan to support them        (2024-11-11)
::                          - Swapped Architecture and Graphics API argument positions so that it will be       (2024-11-11)
::                            consitent with the injector argument list.
::                          - Removed an option to build a test font resource                                   (2024-11-11)
::                  0.2.0:  - Cleaned up script by removing unnecessary bloat in variables and build commands   (2024-10-02)
::                          - No longer build imgui, kiero, or minhook -- libraries are included instead        (2024-10-02)
::                          - Changed to accomodate new directory layout                                        (2024-10-02)
::                          - Now includes build for fonts resourcce                                            (2024-10-02)
::                  0.1.0:  - Initial rough draft version.                                                      (2024-09-26)
::
:: Notes:           Only DirectX 11 is fully supported at this time.
:: Requirements:    Visual Studio 2022 (vcvars64.bat), DirectX 11 SDK, Lua 
::
::======================================================================================================

:: Set directories
set CWD=%~dp0
set SRC_DIR=%CWD%src
set BIN_DIR=%CWD%bin
set OBJ_DIR_INJECTOR=%BIN_DIR%\injector
set OBJ_DIR_BINDINGS=%BIN_DIR%\bindings
set OBJ_DIR_CORE=%BIN_DIR%\core
set INCLUDE_DIR=%CWD%include
set LIBS_DIR=%CWD%libs

set CSTD=/std:c++17

:: Graphics API linking options
set LINK_D3D11=d3d11.lib d3dcompiler.lib libs\imgui_directx11_1.91.2.lib libs\kiero_directx11.lib libs\minhook_x64.lib
set LINK_GRAPHICS=%LINK_D3D11%

:: Initialize build environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo Building injector utility
if not exist %OBJ_DIR_INJECTOR% mkdir %OBJ_DIR_INJECTOR%
cl /EHsc /c /I %INCLUDE_DIR% /Fo:%OBJ_DIR_INJECTOR%\injector_util.obj %CSTD% %SRC_DIR%\injector\injector_util.cpp
if errorlevel 1 goto error

echo Building injector (UiForge.exe)
cl /EHsc /I %INCLUDE_DIR% /Fe:%CWD%UiForge.exe /Fo:%OBJ_DIR_INJECTOR%\injector.obj %CSTD% %SRC_DIR%\injector\uif_injector.cpp %OBJ_DIR_INJECTOR%\injector_util.obj
if errorlevel 1 goto error

echo Building Lua bindings
if not exist %OBJ_DIR_BINDINGS% mkdir %OBJ_DIR_BINDINGS%
cl /EHsc /LD /I %INCLUDE_DIR% /Fo:%OBJ_DIR_BINDINGS%\imgui_lua_bindings.obj /Fe:%BIN_DIR%\imgui_lua_bindings.dll %CSTD% %SRC_DIR%\imgui_lua_bindings\imgui_lua_bindings.cpp /link %LIBS_DIR%\lua.lib %LIBS_DIR%\imgui_directx11_1.91.2.lib
if errorlevel 1 goto error

echo Building core
cl /EHsc /LD /I %INCLUDE_DIR% /Fe:%BIN_DIR%\uif_core.dll %CSTD% %SRC_DIR%\core\*.cpp /link %LINK_GRAPHICS% %LIBS_DIR%\lua.lib
if errorlevel 1 goto error  

goto cleanup

:error
echo ERROR: Build failed.

:cleanup
echo Cleaning up
:: Remember, the following commands quietly execute and will not display any output
if exist %BIN_DIR%\bindings rmdir /S /Q %BIN_DIR%\bindings
if exist %BIN_DIR%\core rmdir /S /Q %BIN_DIR%\core
if exist %BIN_DIR%\injector rmdir /S /Q %BIN_DIR%\injector

:: Clean other build artifacts if we have any for some reason
:: >nul redirects stdout to nul, suppressing output
:: 2>&1 redirects stderr to stdout, which in this case will redirect to nul, thus getting rid of any terminal output for these commands
del %BIN_DIR%\*.exp >nul 2>&1
del %BIN_DIR%\*.lib >nul 2>&1
del *.obj >nul 2>&1

exit /b 0