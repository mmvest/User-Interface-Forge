@echo off
setlocal

::======================================================================================================
:: File:            build_uiforge.bat
:: Description:     Builds UiForge, including the injector, Lua bindings, and core, with all graphics APIs.
::                  Places UiForge.exe in the working directory.
::
:: Usage:           build_uiforge.bat
::
:: Example:         build_uiforge.bat
::
:: Author:          mmvest (wereox)
:: Date:            2024-09-26
::
:: Notes:           Only DirectX 11 is fully supported at this time.
:: Requirements:    Visual Studio 2022 (vcvars64.bat), DirectX 11 SDK, LuaJIT
::
::======================================================================================================

:: Set directories
set CWD=%~dp0
set SRC_DIR=%CWD%src
set BIN_DIR=%CWD%bin
set OBJ_DIR_INJECTOR=%BIN_DIR%\injector
set OBJ_DIR_BINDINGS=%BIN_DIR%\bindings
set OBJ_DIR_CORE=%BIN_DIR%\core
set LIBS_DIR=%CWD%libs

:: Have to modify include  environment variable so all the files will link properly
set INCLUDE=%CWD%include;%CWD%include\luajit;%INCLUDE%

set CSTD=/std:c++17

:: Graphics API linking options
set LINK_D3D11=d3d11.lib d3dcompiler.lib libs\imgui_directx11_1.91.2.lib libs\kiero_directx11.lib libs\minhook_x64.lib
set LINK_DIRECTINPUT=dinput8.lib dxguid.lib
set LINK_GRAPHICS=%LINK_D3D11%

:: LuaJIT linking
set LINK_LUA=%LIBS_DIR%\lua51.lib

:: SWIG
:: set SWIG_DEFINES=STATIC_LINKED
:: ..\..\tools\swigwin-4.3.0\swig.exe -c++ -lua -D_WIN32 -DIMGUI_DISABLE_OBSOLETE_FUNCTIONS -no-old-metatable-bindings -o include\luajit\imgui_bindings.hpp src\core\imgui.i

:: sol_ImGui
set SOL_IMGUI_DEFINES=IMGUI_NO_DOCKING

:: Initialize build environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

:: Build the injector
if not exist %OBJ_DIR_INJECTOR% mkdir %OBJ_DIR_INJECTOR%
cl /nologo /EHsc /Fe:%CWD%UiForge.exe %CSTD% %SRC_DIR%\injector\uif_injector.cpp %SRC_DIR%\injector\injector_util.cpp
if errorlevel 1 goto error

:: Build the core
cl /nologo /EHsc /Zi /LD /D %SOL_IMGUI_DEFINES% /Fe:%BIN_DIR%\uif_core.dll %CSTD% %SRC_DIR%\core\*.cpp /link %LINK_GRAPHICS% %LINK_LUA% %LINK_DIRECTINPUT%
if errorlevel 1 goto error  

goto cleanup

:error
echo ERROR: Build failed.

:cleanup
echo Cleaning up
:: Remember, the following command executes silently and will not display any output
if exist %BIN_DIR%\injector rmdir /S /Q %BIN_DIR%\injector

:: Clean other build artifacts if we have any for some reason
:: >nul redirects stdout to nul, suppressing output
:: 2>&1 redirects stderr to stdout, which in this case will redirect to nul, thus getting rid of any terminal output for these commands
del %BIN_DIR%\*.exp >nul 2>&1
del %BIN_DIR%\*.lib >nul 2>&1
del *.obj >nul 2>&1

exit /b 0