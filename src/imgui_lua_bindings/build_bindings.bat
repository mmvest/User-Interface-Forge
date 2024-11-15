@echo off
REM Set include path
set INCLUDE_PATH="..\..\include"

REM Set output name
set MODULE_NAME=imgui_lua_bindings
set WRAPPER_FILE=%MODULE_NAME%.cpp
set OUTPUT_DLL=..\..\bin\%MODULE_NAME%.dll
set OUTPUT_LIB=..\..\libs\%MODULE_NAME%.lib

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo Compiling DLL with MSVC...
cl /LD /I%INCLUDE_PATH% %WRAPPER_FILE% /link "..\..\libs\lua.lib" "..\..\libs\imgui_directx11_1.91.2.lib" /out:%OUTPUT_DLL% /IMPLIB:%OUTPUT_LIB%
if %errorlevel% neq 0 (
    echo MSVC compilation failed.
    goto cleanup
)

echo Build successful! DLL created: %OUTPUT_DLL%

:cleanup
echo Cleaning up...
del ..\..\libs\*.exp
del *.obj
exit /b %errorlevel%