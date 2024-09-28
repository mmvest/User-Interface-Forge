<h1 align="center">User Interface Forge</h1>
<p align="center">
  <a href="https://github.com/mmvest/user-interface-forge/blob/master/LICENSE">
    <img src="https://img.shields.io/github/license/mmvest/user-interface-forge.svg?style=flat-square"/>
  </a>
  <br>
A framework for injecting and managing custom UI elements in Windows Processes.
</p>

## Overview

User Interface Forge (UIF) is a tool for injecting and managing custom UI elements in any Windows GUI applications using DirectX 11. The framework provides a means whereby users can create their own UI elements using the [ImGui](https://github.com/ocornut/imgui) library and then have their UI elements rendered in the application. Since your code will be running within the address space of the target process, you also have access to that address space meaning you can expose variables and addresses within the process that you can then access, display, or manipulate via your ImGui UI. This tool is ideal for developers or hobbyists who want to extend or modify the behavior of existing software, such as games or graphical applications.

<div align="center">

⚠️ **WARNING: Use this code at your own risk. Only place trusted, non-malicious DLLs in the uif_mods directory. This code will load any DLLs found there, including potentially harmful ones. Intended for educational purposes and UI modding of retro/classic games or personal development projects.** ⚠️

</div>

## Requirements
- **[Windows OS](https://www.microsoft.com/software-download/windows11)**: This tool is designed for Windows environments only. So far the tool has only been tested on Windows 11 but I suspect it should work on 64-bit Windows 7 and 10 as well.


- **[Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)**: Must have `vcvars64.bat` and `cl` for compiling the code.

- **[DirectX SDK](https://learn.microsoft.com/en-us/windows/win32/directx-sdk--august-2009-)**: Included with the Windows SDK


## Building

1. **Clone the Repository**:
    ```bash
    git clone https://github.com/mmvest/User-Interface-Forge.git
    cd user-interface-forge
    ```

2. **Build**:

    To build, use the [build_uiforge.bat](build_uiforge.bat) script found in the root of the project. You can invoke the build like so:
    ```bash
    build_uiforge.bat <graphics_api> <architecture>
    ```

    ### **Currently the code only supports 64-bit DirectX11 applications.**
    
    So for now the only valid build command is:
    ```bash
    build_uiforge.bat d3d11 64
    ```

    This will build all the components of the application and place them in the `bin` directory. The main executable will be called **StartUiForge.exe**.

> **Note:** Anti-Virus Software may flag `StartUiForge.exe` as a virus. This is because the code uses a very classic and simple process injection technique -- and it is not very sneaky about it either. Go ahead and look over the code though -- its not malicious.

## Setup

After building the binaries, place the `uif_core.dll` inside the directory where the target application executable resides. 

Next, create a directory called `uif_mods` and place it in the same directory as the target application executable as well.

Now you are all set to use UiForge.

## Custom Modules

The main feature of UI Forge is its ability to load custom modules and run them every frame that is rendered in the target application. You can create your custom modules and place them in the `uif_mods` directory. These modules must be DLLs and must export the function:
```c
extern "C" __declspec(dllexport) void ShowUiMod(ImGuiContext* mod_context)
```
The `uif_core.dll` will load every DLL file located in the mods directory and then retrieve a pointer to this function so it can be called during the core's main loop.

A [template](uif_module_template.cpp) and an [example](uif_module_example.cpp) of what a custom module might look like can be found in the project.

Although this is INTENDED for UI elements, it could also just as easily be used as a launch point for other hacks since all DLLs loaded will have access to the virtual address space of the target process. You can technically put whatever you want in the `ShowUiMod` function and Ui Forge core will run it.

## Usage

To use UI Forge, open your target application and wait for the application GUI to initialize. Once the application has started and the GUI starts presenting, run `StartUiForge.exe` passing in the desired configuration as arguments. As an example
```bash
StartUiForge.exe --help  # Display help text
StartUiForge.exe pcsx2-qt.exe 64 d3d11
```
This will inject `uif_core.dll` into the target application, in this case the Playstation 2 emulator called `pcsx2`.

If you want to inject custom DLLs along with the core DLL into the target application, you may pass them as additional arguments like so:
```bash
StartUiForge.exe pcsx2-qt.exe 64 d3d11 mylib_01.dll mylib_02.dll ... mylib_N.dll
```
A separate thread will be spun up for each DLL.

Once injected, the `uif_core.dll` will load all custom UI modules located in the `uif_mods` directory, find the function called `ShowUiMod` that MUST be exported by the DLLs, and hook the DirectX11 `Present()` function which will start the framework's loop. At this point, `uif_core.dll` will run indefinitely until the application closes, running your custom UI code each frame.

## Examples: UI Forge in Action

Examples are incoming -- currently working on a project that will be using this!

## Roadmap
Below are some features that I want to implement. The current implementation is what I would call a rough draft or proof of concept to show that it can be done and to get my other project off the ground. 
- Implement a control panel for `uif_core` that allows the user to enable/disable modules, load/unload modules, get debug information (such as amount of time elapsed to run each module and debug logs), etc.
- Implement a config file that will allow the user to change configuration options such as the name of the mods folder or the name of the core dll to be injected.
- Implement an GUI for `StartUiForge.exe` so it does not have to be just a CLI application
- Add support for DirectX 9, 10, and 12 as well as vulkan (and potentially openGL).
- Add 32-bit support
- Potentially make Linux compatible version
- Add instructions for building the example code

## Contributing

Contributions are welcome! I am very new to C++ (all my professional experience is in C) so I am open to suggestions, advice, or criticism that will help me improve. Currently the code is a hodgepodge of what I would call "C"-isms and "C++"-isms. I'd like to clean it up as I learn more about C++ and OOP. 

Please open an issue or submit a pull request for any features or fixes you'd like to see.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE.txt) file for details.

## Acknowledgements

- **[Dear ImGui](https://github.com/ocornut/imgui)** For providing a flexible and easy-to-use graphical interface.
- **[Kiero Library](https://github.com/Rebzzel/kiero)**: For easy hooking into various graphics APIs.
- **[MinHook](https://github.com/TsudaKageyu/minhook)** For providing the tools that lets Kiero work!

## Disclaimer

This tool is for educational and developmental purposes only. Use at your own risk and ensure you have the right to modify the target application.