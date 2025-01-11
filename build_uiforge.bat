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

@REM Set directories
set CWD=%~dp0
set SRC_DIR=%CWD%src
set BIN_DIR=%CWD%bin
set OBJ_DIR_INJECTOR=%BIN_DIR%\injector
set OBJ_DIR_BINDINGS=%BIN_DIR%\bindings
set OBJ_DIR_CORE=%BIN_DIR%\core
set LIBS_DIR=%CWD%libs

@REM Have to modify include  environment variable so all the files will link properly
set INCLUDE=%CWD%include;%CWD%include\luajit;%INCLUDE%

set CSTD=/std:c++17

@REM Graphics API linking options
set LINK_D3D11=d3d11.lib d3dcompiler.lib libs\imgui_directx11_1.91.2.lib libs\kiero_directx11.lib libs\minhook_x64.lib libs\DirectXTK.lib
set LINK_GRAPHICS=%LINK_D3D11%

@REM LuaJIT linking
set LINK_LUA=%LIBS_DIR%\lua51.lib

@REM sol_ImGui
set SOL_IMGUI_DEFINES=IMGUI_NO_DOCKING

@REM Initialize build environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

@REM Determine what to build
set BUILD_ALL=false
set BUILD_PCH=false
set BUILD_INJECTOR=false
set BUILD_CORE=false

if /I "%~1"=="" set BUILD_ALL=true
if /I "%~1"=="pch" set BUILD_PCH=true
if /I "%~1"=="injector" set BUILD_INJECTOR=true
if /I "%~1"=="core" set BUILD_CORE=true

@REM Build PCH
if "%BUILD_ALL%"=="true" set BUILD_PCH=true
if "%BUILD_PCH%"=="true" (
    echo Building
    if not exist %BIN_DIR% mkdir %BIN_DIR%
    cl /nologo /Bt+ /EHsc /Zi /Yc"pch.h" %CSTD% /D %SOL_IMGUI_DEFINES% /Fp"%BIN_DIR%\pch.pch" /Fo"%BIN_DIR%\pch.obj" /c %SRC_DIR%\pch.cpp
    @REM /nologo        : Suppresses the compiler version info in output.
    @REM /Bt            : Outputs timing information for each stage of the compilation process.
    @REM /EHsc          : Enables standard C++ exception handling.
    @REM /Yc            : Creates a precompiled header from pch.h.
    @REM %CSTD%         : Specifies the C++ standard to use (e.g., /std:c++17).
    @REM /D             : Defines preprocessing macros.
    @REM /Fp            : Specifies the precompiled header
    @REM /Fo            : Specifies the compiled object
    @REM /c             : Compiles without linking.
    if errorlevel 1 goto error
)

@REM Build Injector
if "%BUILD_ALL%"=="true" set BUILD_INJECTOR=true
if "%BUILD_INJECTOR%"=="true" (
    echo Building Injector
    if not exist %OBJ_DIR_INJECTOR% mkdir %OBJ_DIR_INJECTOR%
    cl /nologo /EHsc /Fe:%CWD%UiForge.exe %CSTD% %SRC_DIR%\injector\injector.cpp %SRC_DIR%\injector\util.cpp
    @REM /nologo      : Suppresses the compiler version info in output.
    @REM /EHsc        : Enables standard C++ exception handling.
    @REM /Fe          : Specifies the output file name for the executable.
    @REM %CSTD%       : Specifies the C++ standard to use.
    if errorlevel 1 goto error
)

@REM Build Core
if "%BUILD_ALL%"=="true" set BUILD_CORE=true
if "%BUILD_CORE%"=="true" (
    echo Building Core
    if not exist %OBJ_DIR_CORE% mkdir %OBJ_DIR_CORE%
    cl /nologo /bigobj /EHsc /Bt+ /MP /MT /Zi /LD /D %SOL_IMGUI_DEFINES% /Yu"pch.h" /Fp"%BIN_DIR%\pch.pch" /Fe:%BIN_DIR%\uiforge_core.dll %CSTD% %SRC_DIR%\core\*.cpp %BIN_DIR%\pch.obj /link %LINK_GRAPHICS% %LINK_LUA%
    @REM /nologo      : Suppresses the compiler version info in output.
    @REM /bigobj      : Enables support for larger object files.
    @REM /EHsc        : Enables standard C++ exception handling.
    @REM /MT          : Statically links the multithreaded runtime library.
    @REM /Zi          : Generates complete debugging information.
    @REM /LD          : Builds a dynamic-link library (DLL).
    @REM /D           : Defines macro(s) for preprocessing (e.g., IMGUI_NO_DOCKING).
    @REM /Yu          : Uses the precompiled header pch.h.
    @REM /Fp          : Specifies the PCH file to use.
    @REM /Fe          : Specifies the output file name for the DLL.
    @REM %CSTD%       : Specifies the C++ standard to use.
    @REM /link        : Specifies linker options.
    if errorlevel 1 goto error
)


goto cleanup

:error
echo ERROR: Build failed

:cleanup
echo Cleaning up
@REM Remember, the following command executes silently and will not display any output
if exist %BIN_DIR%\injector rmdir /S /Q %BIN_DIR%\injector

@REM Clean other build artifacts if we have any for some reason
@REM >nul redirects stdout to nul, suppressing output
@REM 2>&1 redirects stderr to stdout, which in this case will redirect to nul, thus getting rid of any terminal output for these commands
del %BIN_DIR%\*.exp >nul 2>&1
del %BIN_DIR%\*.lib >nul 2>&1
del *.obj >nul 2>&1
del *.exp >nul 2>&1
del *.lib >nul 2>&1

exit /b 0

:build_all
set BUILD_INJECTOR=true
set BUILD_CORE=true
goto build_injector