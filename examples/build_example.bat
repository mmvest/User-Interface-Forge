@echo off

set "RUN_VCVARS=start %comspec% /k ^"C:\Program^ Files\Microsoft^ Visual^ Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat^""

set "IMGUI_OUTPUT_DIR=..\bin\imgui"
set "IMGUI_DIR=..\libs\imgui"
set "IMGUI_INCLUDES=/I%IMGUI_DIR% /I%IMGUI_DIR%\backends"
set "IMGUI_OBJS=%IMGUI_OUTPUT_DIR%\*.obj"

set "BUILD_EXAMPLE=cl /EHsc /LD uif_module_example.cpp /MD /Fe:..\bin\uif_module_example_d3d11_x64.dll /Fo:..\bin\uif_module_example_d3d11_x64.o %IMGUI_OBJS% %IMGUI_INCLUDES%"

%RUN_VCVARS% && %BUILD_EXAMPLE% && exit /b