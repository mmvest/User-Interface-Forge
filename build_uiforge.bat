@echo off
setlocal EnableDelayedExpansion

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
set OBJ_DIR_FTXUI=%BIN_DIR%\ftxui
set OBJ_DIR_EXTERNALS=%BIN_DIR%\externals
set EXTERNALS_DIR=%CWD%externals

@REM External dependency roots
set DIRECTXTK_DIR=%EXTERNALS_DIR%\DirectXTK
set FTXUI_DIR=%EXTERNALS_DIR%\FTXUI
set IMGUI_DIR=%EXTERNALS_DIR%\imgui
set INJECTTOOLS_DIR=%EXTERNALS_DIR%\InjectTools
set KIERO_DIR=%EXTERNALS_DIR%\kiero
set LUAJIT_SRC_DIR=%EXTERNALS_DIR%\LuaJIT\src
set MINHOOK_DIR=%EXTERNALS_DIR%\minhook
set PLOG_INCLUDE_DIR=%EXTERNALS_DIR%\plog\include
set SCL_INCLUDE_DIR=%EXTERNALS_DIR%\simple-config-library\include
set SOL2_INCLUDE_DIR=%EXTERNALS_DIR%\sol2\include
set SOL2_IMGUI_DIR=%EXTERNALS_DIR%\sol2_ImGui_Bindings

set FTXUI_SRC_DIR=%FTXUI_DIR%\src\ftxui
set FTXUI_INCLUDE_DIR=%FTXUI_DIR%\include
set FTXUI_LIB=%OBJ_DIR_FTXUI%\FTXUI.lib
set LUAJIT_LIB=%LUAJIT_SRC_DIR%\lua51.lib

@REM Common project include root (so "core\\util.h" resolves to src\\core\\util.h).
set PROJ_INCLUDE_DIR=%SRC_DIR%

set CSTD=/std:c++17
set RUNTIME=/MT

@REM Graphics API linking options
set LINK_D3D11=d3d11.lib dxgi.lib d3dcompiler.lib
set LINK_IMGUI_WIN32=user32.lib gdi32.lib imm32.lib
set LINK_WIC=ole32.lib windowscodecs.lib
set LINK_GRAPHICS=%LINK_D3D11% %LINK_IMGUI_WIN32% %LINK_WIC%

@REM Injector linking
set LINK_FTXUI="%FTXUI_LIB%"

@REM sol_ImGui
set SOL_IMGUI_DEFINES=IMGUI_NO_DOCKING

@REM Manual cleanup mode (no build).
if /I "%~1"=="cleanup" goto cleanup

@REM Initialize build environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

@REM Basic dependency sanity checks (externals/ should already be populated by submodules)
if not exist "%EXTERNALS_DIR%\" (
    echo ERROR: Missing "%EXTERNALS_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%IMGUI_DIR%\imgui.h" (
    echo ERROR: Missing ImGui sources under "%IMGUI_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%KIERO_DIR%\kiero.cpp" (
    echo ERROR: Missing kiero sources under "%KIERO_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%MINHOOK_DIR%\src\hook.c" (
    echo ERROR: Missing MinHook sources under "%MINHOOK_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%DIRECTXTK_DIR%\Src\WICTextureLoader.cpp" (
    echo ERROR: Missing DirectXTK sources under "%DIRECTXTK_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%LUAJIT_SRC_DIR%\msvcbuild.bat" (
    echo ERROR: Missing LuaJIT build scripts under "%LUAJIT_SRC_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%PLOG_INCLUDE_DIR%\plog\Log.h" (
    echo ERROR: Missing plog headers under "%PLOG_INCLUDE_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%SCL_INCLUDE_DIR%\SCL\SCL.hpp" (
    echo ERROR: Missing simple-config-library headers under "%SCL_INCLUDE_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%SOL2_INCLUDE_DIR%\sol\sol.hpp" (
    echo ERROR: Missing sol2 headers under "%SOL2_INCLUDE_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%SOL2_IMGUI_DIR%\sol_ImGui.h" (
    echo ERROR: Missing sol2_ImGui_Bindings headers under "%SOL2_IMGUI_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)
if not exist "%INJECTTOOLS_DIR%\inject_tools.h" (
    echo ERROR: Missing InjectTools headers under "%INJECTTOOLS_DIR%".
    echo        Run: git submodule update --init --recursive
    goto error
)

@REM Determine what to build
set BUILD_ALL=false
set BUILD_INJECTOR=false
set BUILD_CORE=false
set BUILD_TESTD3D11=false
set BUILD_FTXUI=false
set BUILD_FAILED=false

if /I "%~1"=="" set BUILD_ALL=true
if /I "%~1"=="injector" set BUILD_INJECTOR=true
if /I "%~1"=="core" set BUILD_CORE=true
if /I "%~1"=="testd3d11" set BUILD_TESTD3D11=true
if /I "%~1"=="ftxui" set BUILD_FTXUI=true

@REM Build FTXUI static library
if "%BUILD_ALL%"=="true" set BUILD_FTXUI=true
if "%BUILD_INJECTOR%"=="true" set BUILD_FTXUI=true
if "%BUILD_FTXUI%"=="true" (
    echo Building FTXUI
    @REM Keep objects in separate subfolders to avoid name collisions.
    if not exist "%OBJ_DIR_FTXUI%\screen" mkdir "%OBJ_DIR_FTXUI%\screen"
    if not exist "%OBJ_DIR_FTXUI%\dom" mkdir "%OBJ_DIR_FTXUI%\dom"
    if not exist "%OBJ_DIR_FTXUI%\component" mkdir "%OBJ_DIR_FTXUI%\component"

    set "FTXUI_SCREEN_SOURCES="
    for %%F in ("%FTXUI_SRC_DIR%\screen\*.cpp") do (
        echo %%~nxF | findstr /I "_test.cpp fuzzer.cpp" >nul
        if errorlevel 1 set "FTXUI_SCREEN_SOURCES=!FTXUI_SCREEN_SOURCES! "%%~fF""
    )
    set "FTXUI_DOM_SOURCES="
    for %%F in ("%FTXUI_SRC_DIR%\dom\*.cpp") do (
        echo %%~nxF | findstr /I "_test.cpp fuzzer.cpp" >nul
        if errorlevel 1 set "FTXUI_DOM_SOURCES=!FTXUI_DOM_SOURCES! "%%~fF""
    )
    set "FTXUI_COMPONENT_SOURCES="
    for %%F in ("%FTXUI_SRC_DIR%\component\*.cpp") do (
        echo %%~nxF | findstr /I "_test.cpp fuzzer.cpp" >nul
        if errorlevel 1 set "FTXUI_COMPONENT_SOURCES=!FTXUI_COMPONENT_SOURCES! "%%~fF""
    )

    if "!FTXUI_SCREEN_SOURCES!"=="" (
        echo ERROR: No FTXUI screen source files were found.
        goto error
    )
    if "!FTXUI_DOM_SOURCES!"=="" (
        echo ERROR: No FTXUI dom source files were found.
        goto error
    )
    if "!FTXUI_COMPONENT_SOURCES!"=="" (
        echo ERROR: No FTXUI component source files were found.
        goto error
    )

    cl /nologo /EHsc %RUNTIME% /DUNICODE /D_UNICODE /c %CSTD% /I"%FTXUI_INCLUDE_DIR%" /I"%FTXUI_DIR%\src" /Fo"%OBJ_DIR_FTXUI%\screen\\" !FTXUI_SCREEN_SOURCES!
    if errorlevel 1 goto error
    cl /nologo /EHsc %RUNTIME% /DUNICODE /D_UNICODE /c %CSTD% /I"%FTXUI_INCLUDE_DIR%" /I"%FTXUI_DIR%\src" /Fo"%OBJ_DIR_FTXUI%\dom\\" !FTXUI_DOM_SOURCES!
    if errorlevel 1 goto error
    cl /nologo /EHsc %RUNTIME% /DUNICODE /D_UNICODE /c %CSTD% /I"%FTXUI_INCLUDE_DIR%" /I"%FTXUI_DIR%\src" /Fo"%OBJ_DIR_FTXUI%\component\\" !FTXUI_COMPONENT_SOURCES!
    if errorlevel 1 goto error

    lib /nologo /OUT:"%FTXUI_LIB%" "%OBJ_DIR_FTXUI%\screen\*.obj" "%OBJ_DIR_FTXUI%\dom\*.obj" "%OBJ_DIR_FTXUI%\component\*.obj"
    if errorlevel 1 goto error
)

@REM Build Injector
if "%BUILD_ALL%"=="true" set BUILD_INJECTOR=true
if "%BUILD_INJECTOR%"=="true" (
    echo Building Injector
    if not exist %OBJ_DIR_INJECTOR% mkdir %OBJ_DIR_INJECTOR%
    cl /nologo /EHsc %RUNTIME% /DUNICODE /D_UNICODE %CSTD% ^
        /I"%PROJ_INCLUDE_DIR%" ^
        /I"%FTXUI_INCLUDE_DIR%" /I"%FTXUI_DIR%\src" ^
        /I"%PLOG_INCLUDE_DIR%" /I"%SCL_INCLUDE_DIR%" /I"%INJECTTOOLS_DIR%" ^
        /Fo"%OBJ_DIR_INJECTOR%\\" /Fe:"%CWD%UiForge.exe" ^
        "%SRC_DIR%\injector\injector.cpp" ^
        /link %LINK_FTXUI%
    @REM /nologo      : Suppresses the compiler version info in output.
    @REM /EHsc        : Enables standard C++ exception handling.
    @REM %RUNTIME%    : Runtime library setting (must match third-party objects).
    @REM /Fe          : Specifies the output file name for the executable.
    @REM %CSTD%       : Specifies the C++ standard to use.
    if errorlevel 1 goto error
)

@REM Build Core
if "%BUILD_ALL%"=="true" set BUILD_CORE=true
if "%BUILD_CORE%"=="true" (
    echo Building Third-Party Dependencies

    if not exist "%OBJ_DIR_EXTERNALS%\imgui" mkdir "%OBJ_DIR_EXTERNALS%\imgui"
    if not exist "%OBJ_DIR_EXTERNALS%\kiero" mkdir "%OBJ_DIR_EXTERNALS%\kiero"
    if not exist "%OBJ_DIR_EXTERNALS%\minhook" mkdir "%OBJ_DIR_EXTERNALS%\minhook"
    if not exist "%OBJ_DIR_EXTERNALS%\directxtk" mkdir "%OBJ_DIR_EXTERNALS%\directxtk"

    @REM ImGui
    cl /nologo /c /EHsc %RUNTIME% /Zi %CSTD% /D %SOL_IMGUI_DEFINES% /D IMGUI_DISABLE_OBSOLETE_KEYIO ^
        /I"%IMGUI_DIR%" /I"%IMGUI_DIR%\misc\cpp" ^
        /Fo"%OBJ_DIR_EXTERNALS%\imgui\\" ^
        "%IMGUI_DIR%\imgui.cpp" "%IMGUI_DIR%\imgui_draw.cpp" "%IMGUI_DIR%\imgui_tables.cpp" "%IMGUI_DIR%\imgui_widgets.cpp" ^
        "%IMGUI_DIR%\misc\cpp\imgui_stdlib.cpp" ^
        "%IMGUI_DIR%\backends\imgui_impl_win32.cpp" "%IMGUI_DIR%\backends\imgui_impl_dx11.cpp"
    if errorlevel 1 goto error

    @REM MinHook
    cl /nologo /c %RUNTIME% /Zi ^
        /I"%MINHOOK_DIR%\include" /I"%MINHOOK_DIR%\src" /I"%MINHOOK_DIR%\src\hde" ^
        /Fo"%OBJ_DIR_EXTERNALS%\minhook\\" ^
        "%MINHOOK_DIR%\src\buffer.c" "%MINHOOK_DIR%\src\hook.c" "%MINHOOK_DIR%\src\trampoline.c" "%MINHOOK_DIR%\src\hde\hde64.c"
    if errorlevel 1 goto error

    @REM kiero
    cl /nologo /c /EHsc %RUNTIME% /Zi %CSTD% /DUIFORGE_KIERO_INCLUDE_D3D11=1 /DUIFORGE_KIERO_USE_MINHOOK=1 ^
        /FI"%SRC_DIR%\compat\kiero_compat.h" ^
        /I"%KIERO_DIR%" /I"%EXTERNALS_DIR%" ^
        /Fo"%OBJ_DIR_EXTERNALS%\kiero\\" ^
        "%KIERO_DIR%\kiero.cpp"
    if errorlevel 1 goto error

    @REM DirectXTK
    cl /nologo /c /EHsc %RUNTIME% /Zi %CSTD% ^
        /I"%DIRECTXTK_DIR%\Inc" /I"%DIRECTXTK_DIR%\Src" ^
        /Fo"%OBJ_DIR_EXTERNALS%\directxtk\\" ^
        "%DIRECTXTK_DIR%\Src\WICTextureLoader.cpp"
    if errorlevel 1 goto error

    @REM LuaJIT
    echo Building LuaJIT
    pushd "%LUAJIT_SRC_DIR%"
    call msvcbuild.bat static
    if errorlevel 1 (popd & goto error)
    popd
    if not exist "%LUAJIT_LIB%" (
        echo ERROR: LuaJIT build succeeded, but "%LUAJIT_LIB%" was not found.
        goto error
    )

    echo Building Core
    if not exist %OBJ_DIR_CORE% mkdir %OBJ_DIR_CORE%
    cl /nologo /bigobj /EHsc /Bt+ /MP %RUNTIME% /Zi /LD /D %SOL_IMGUI_DEFINES% /D IMGUI_DISABLE_OBSOLETE_KEYIO %CSTD% ^
        /I"%PROJ_INCLUDE_DIR%" ^
        /I"%IMGUI_DIR%" /I"%IMGUI_DIR%\misc\cpp" ^
        /I"%KIERO_DIR%" /I"%EXTERNALS_DIR%" ^
        /I"%PLOG_INCLUDE_DIR%" /I"%SCL_INCLUDE_DIR%" ^
        /I"%SOL2_INCLUDE_DIR%" /I"%SOL2_INCLUDE_DIR%\sol" /I"%SOL2_IMGUI_DIR%" ^
        /I"%LUAJIT_SRC_DIR%" ^
        /I"%DIRECTXTK_DIR%\Inc" /I"%DIRECTXTK_DIR%\Src" ^
        /Fo"%OBJ_DIR_CORE%\\" /Fe:"%BIN_DIR%\uiforge_core.dll" ^
        %SRC_DIR%\core\*.cpp ^
        "%OBJ_DIR_EXTERNALS%\imgui\*.obj" "%OBJ_DIR_EXTERNALS%\minhook\*.obj" "%OBJ_DIR_EXTERNALS%\kiero\*.obj" "%OBJ_DIR_EXTERNALS%\directxtk\*.obj" ^
        /link %LINK_GRAPHICS% "%LUAJIT_LIB%"
    @REM /nologo      : Suppresses the compiler version info in output.
    @REM /bigobj      : Enables support for larger object files.
    @REM /EHsc        : Enables standard C++ exception handling.
    @REM %RUNTIME%    : Runtime library setting.
    @REM /Zi          : Generates complete debugging information.
    @REM /LD          : Builds a dynamic-link library (DLL).
    @REM /D           : Defines macro(s) for preprocessing (e.g., IMGUI_NO_DOCKING).
    @REM /Fe          : Specifies the output file name for the DLL.
    @REM %CSTD%       : Specifies the C++ standard to use.
    @REM /link        : Specifies linker options.
    if errorlevel 1 goto error
)

@REM Build D3D11 Test Window
if "%BUILD_TESTD3D11%"=="true" (
    echo Building D3D11 Test Window
    if not exist %BIN_DIR% mkdir %BIN_DIR%
    cl /nologo /EHsc /Fe:%BIN_DIR%\test_d3d11_window.exe %CSTD% %SRC_DIR%\test\test_d3d11_window.cpp /DUNICODE /D_UNICODE /link user32.lib gdi32.lib d3d11.lib dxgi.lib
    @REM /nologo      : Suppresses the compiler version info in output.
    @REM /EHsc        : Enables standard C++ exception handling.
    @REM /Fe          : Specifies the output file name for the executable.
    @REM %CSTD%       : Specifies the C++ standard to use.
    @REM /DUNICODE    : Enables Unicode Win32 APIs.
    @REM /D_UNICODE   : Enables Unicode C/C++ runtime mappings.
    @REM /link        : Specifies linker options.
    @REM user32.lib   : Win32 windowing and message handling.
    @REM gdi32.lib    : GDI support (required by Win32 window class helpers).
    @REM d3d11.lib    : Direct3D 11 runtime.
    @REM dxgi.lib     : DXGI swap chain and fullscreen control.

    if errorlevel 1 goto error
)


goto cleanup

:error
echo ERROR: Build failed
set BUILD_FAILED=true

:cleanup
echo Cleaning up
@REM Remove root-level artifacts from this build.
pushd "%CWD%" >nul 2>&1
if not errorlevel 1 (
    del /F /Q "*.obj" "*.pdb" "*.ilk" >nul 2>&1
    popd >nul
)

@REM Remove ALL subdirectories under bin.
if exist "%BIN_DIR%" (
    for /d %%D in ("%BIN_DIR%\*") do rd /S /Q "%%~fD"
) >nul 2>&1

if "%BUILD_FAILED%"=="true" (
    exit /b 1
)
exit /b 0

:build_all
set BUILD_INJECTOR=true
set BUILD_CORE=true
goto build_injector
