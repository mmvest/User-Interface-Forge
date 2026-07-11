<p align="center">
  <img src="media/uiforge_icon_gold.png" alt="UiForge icon" width="358" height="353"/>
</p>
<h1 align="center">User Interface Forge</h1>
<p align="center">
  <a href="https://github.com/mmvest/user-interface-forge/blob/master/LICENSE">
    <img src="https://img.shields.io/github/license/mmvest/user-interface-forge.svg?style=flat-square"/>
  </a>
  <br>
A framework for injecting and managing custom UI elements in Windows Processes.
</p>

## Overview

User Interface Forge (UiForge) is a tool for injecting and managing custom UI elements in Windows GUI applications rendered with DirectX 11 or DirectX 12. The framework provides a means whereby users can create their own UI elements using the [ImGui](https://github.com/ocornut/imgui) library and then have their UI elements rendered in the application. Since your code will be running within the address space of the target process, you also have access to that address space, meaning you can expose variables and addresses within the process that you can then access, display, or manipulate via your ImGui UI. This tool is ideal for developers or hobbyists who want to extend or modify the behavior of existing software, such as games or graphical applications.

UiForge has three main parts:
1. **[ImGui Lua Bindings (sol_ImGui.h)](https://github.com/mmvest/sol2_ImGui_Bindings)** - These bindings provide access to ImGui's functionality in Lua so you can write and create your own UI elements in Lua instead of C/C++. I have unofficially dubbed these Lua scripts as "ForgeScripts".

1. **The Core (uiforge_core.dll)** - This is responsible for loading, running, and managing all of the Lua scripts. It is also responsible for hooking the correct graphics api functions so that the custom UI can be displayed within the context of the target application.

1. **The Injector (UiForge.exe)** - This is responsible for getting the `uiforge_core.dll` into the target application and starting it.

UiForge supports 64-bit **DirectX 11** and **DirectX 12**, and can auto-detect which of the two a target process is using. Vulkan and OpenGL are planned but not yet implemented.

<div align="center">

⚠️ **WARNING: Use this code at your own risk. Only place trusted, non-malicious scripts in the scripts directory. This code will load any scripts found there, including potentially harmful ones. Intended for educational purposes and UI modding of retro/classic games or personal development projects. The injector may be marked as malware by Windows Defender since this uses a VERY common injection technique. I also recommend NOT using this while any form of anti-cheat is running, as you may get caught/banned. This is not meant to circumvent anti-cheat.** ⚠️

</div>

## Requirements
- **[Windows OS](https://www.microsoft.com/software-download/windows11)**: This tool is designed for Windows environments only. So far the tool has only been tested on Windows 11 but I suspect it should work on 64-bit Windows 7 and 10 as well.

- **[Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)**: Must have `vcvars64.bat` and `cl` for compiling the code.

- **[DirectX SDK](https://learn.microsoft.com/en-us/windows/win32/directx-sdk--august-2009-)**: Included with the Windows SDK.


## Building

1. **Clone the Repository** (dependencies are vendored as git submodules under `externals/`):
    ```bash
    git clone --recurse-submodules https://github.com/mmvest/User-Interface-Forge.git
    cd user-interface-forge
    ```
    If you already cloned without submodules, run:
    ```bash
    git submodule update --init --recursive
    ```

2. **Build**:

    To build, use the [build_uiforge.bat](build_uiforge.bat) script found in the root of the project:
    ```bash
    build_uiforge.bat
    ```

    This builds all components and places them in the `bin` directory (the core DLL is `bin\uiforge_core.dll`). The main executable, **UiForge.exe**, is placed in the project root.

    The build script also accepts individual targets:
    ```bash
    build_uiforge.bat injector        :: Just the injector (UiForge.exe) and FTXUI
    build_uiforge.bat core            :: Just the core DLL and its dependencies
    build_uiforge.bat testd3d11       :: The D3D11 test window (see Testing/Demo)
    build_uiforge.bat ftxui           :: Just the FTXUI static library
    build_uiforge.bat cleanup         :: Remove build artifacts, no build
    build_uiforge.bat create-package  :: Package an already-built UiForge into a release zip, no build
    ```

You are now all set to use UiForge!

### Creating a release package

Once UiForge is built, `create-package` bundles the runtime components into a zip under a `releases` directory. This does **not** build anything, so build UiForge first.
```bash
build_uiforge.bat create-package          :: Produces releases\UiForge.zip
build_uiforge.bat create-package V1.0.0   :: Produces releases\UiForge-V1.0.0.zip
```
An optional version string (second argument) is appended to the file name. Without a version string, the package is just named `UiForge.zip`.

> **Note:** Just another friendly reminder that anti-virus software may flag `UiForge.exe` as a virus. This is because the code uses a very classic and simple process injection technique -- and it is not very sneaky about it either. Go ahead and look over the code though -- it is not malicious.

## How to Use UiForge

Run UiForge from the command line, targeting a process by name or by PID:
```bash
UiForge.exe -n <target_application_name>
UiForge.exe -p <pid>
UiForge.exe -h | -help | --help
```

UiForge launches a small terminal UI (powered by [FTXUI](https://github.com/ArthurSonzogni/FTXUI)) that scans for matching processes and then behaves based on how many it finds:

- **No matches** - shows a message and exits.
- **One match** - injects into it automatically.
- **Two or more matches** - shows an interactive selector so you can pick **one or more** processes to inject into in a single run.

Name matching (`-n`) is fuzzy and ranked: the target and process names are normalized (lowercased, `.exe` stripped) and ranked as exact match, then substring match, then a bounded edit-distance match to catch small typos. Every match is shown in the selector.

In the selector:
- `↑` / `↓` - move
- `Space` - toggle selection of the highlighted process
- `Enter` / `Inject` button - inject into all selected processes
- `Esc` - cancel
- `PageUp` / `PageDown` - scroll the live log panel

Before injecting, UiForge checks whether the core DLL is already loaded in the target to avoid a double-injection.

As long as the application is actively rendering frames with a supported graphics API, you should see the UiForge icon appear in the top-left corner of the window. Click it to open the Settings window.

To stop UiForge, click the **Eject** button in the Settings window, or press `Ctrl+Alt+Shift+End`. A dialog box will pop up confirming that UiForge has cleaned up. Note that NOT closing this dialog and then attempting to re-inject may cause UiForge to fail. (Keybinds are currently fixed in code; configurable keybinds are planned.)

### Expected file structure

UiForge expects the following structure. The core DLL path and the script/module/resource directories are configurable through the `config` file; the `profiles` directory is always created inside the scripts directory.
```
project-root/
├── UiForge.exe            # The injector / launcher
├── config                 # Configuration file for UiForge
├── bin/
│  └── uiforge_core.dll    # Core DLL injected into the target
└── scripts/               # Lua scripts loaded and run in the target
   ├── modules/            # Shared libraries / modules (ships with imgui and uiforge type-hint stubs)
   ├── resources/          # Shared images and other resources for UiForge and scripts
   ├── profiles/           # Saved profiles (created on demand); see "Profiles" below
   ├── my_script.lua       # A loose script
   └── my_package/         # A script package (see "Script packages" below)
      ├── my_package.lua   # The package's entry script
      ├── modules/         # Optional modules private to this package
      └── resources/       # Optional resources private to this package
```

### Script packages

Besides loose `.lua` files, a script can be shipped as a self-contained directory dropped into `scripts\`. This lets a script bundle its own modules and images so users don't have to place files into the shared `modules` and `resources` directories by hand.

A subdirectory of `scripts\` is treated as a script package when it contains an entry script directly inside it, named (in order of preference):

1. `<directory name>.lua` (e.g. `my_package\my_package.lua`)
2. `main.lua`
3. `init.lua`

Inside a package:

- An optional `modules\` folder is prepended to `package.path` while the package's script runs, so its `require()` calls resolve local modules first and fall back to the shared `scripts\modules` directory.
- An optional `resources\` folder is checked first when the script loads resources by relative path (e.g. `UiForge.LoadTexture`), falling back to the shared `scripts\resources` directory.

The shared `modules`, `resources`, and `profiles` directories are never treated as packages. Loose scripts continue to work exactly as before, and profiles identify a packaged script by its entry script's file name, so two packages (or a package and a loose script) must not use the same script file name.

## Lua Script Integration and Features

You build your UI in Lua-based scripts (ForgeScripts). How it works:

- **Script loading**: Scripts in the `scripts` directory (loose `.lua` files and script packages) are executed every frame in the target application, whether or not they use the ImGui bindings.

- **Isolated script environments**: Each script runs in its own Lua environment, so scripts don't clobber each other's globals. Shared globals like `ImGui` and `UiForge` still fall through and remain accessible.

- **Rendering**: UI elements are rendered within the target application's graphics API render loop (D3D11 or D3D12). ImGui context and frame setup are handled for you; a script just calls `ImGui.Begin()`, `ImGui.End()`, and whatever goes in between.

- **LuaJIT**: Scripts run on LuaJIT, which is Lua 5.1 compatible. See the [LuaJIT documentation](https://luajit.org/) for details.

- **ImGui bindings**: The Lua ImGui bindings (powered by [Sol2](https://github.com/ThePhD/sol2)) are exposed as globals, so scripts don't need to require or load anything to use them.

- **Type hints**: An `imgui.lua` file in `scripts\modules\imgui` provides type hints for most supported functions. It is not exhaustive, but it is a useful reference.

- **Custom modules**: Add libraries or Lua modules to `scripts\modules` to `require` them from your scripts, or bundle them inside a script package's own `modules` folder. An embedded `serpent` serializer is available via `require("serpent")`.

- **Example script**: The [`uiforge_example`](scripts/uiforge_example) package is a single tour of UiForge's features. ImGui widgets, drawing, window flags, packaged modules and resources, fonts, audio, and a bouncing balls demo wired into the Settings/Save/Load callbacks.

- **Static linking**: Third-party dependencies (ImGui, Kiero, LuaJIT, etc.) are statically linked into UiForge, so there are no extra DLLs to manage. Dependencies are typically kept as git submodules under the [`externals`](externals) folder.

- **Configuration**: Options are set in the `config` file (see [Configuration](#configuration)).

### The Settings window

Clicking the UiForge icon opens the Settings window, which lets you:

- **Enable/disable** individual scripts via checkboxes.
- **Select** a script to view its own settings UI (Settings tab) or its **Debug** stats (file size, read/hash time, average load/execute time, execution count).
- **Refresh** the script list to pick up newly added script files and script packages.
- **Hot-Reload** the selected script or all scripts if none are selected.
- Manage **Profiles** via the File menu (see below).
- **Eject** UiForge.

You can also click-n-drag the UiForge Icon to move it around.

### The Lua API

Exposed under the global `UiForge` table:

| Symbol | Description |
|--------|-------------|
| `UiForge.scripts_path` / `modules_path` / `resources_path` / `profiles_path` | Absolute paths to the corresponding directories. |
| `UiForge.LoadTexture(path)` | Loads an image into a texture handle usable with `ImGui.Image`. Relative paths resolve against the calling package's `resources` folder first (if any), then the shared resources directory. |
| `UiForge.CreateTextureFromMemory(rgba, width, height)` | Creates a texture from raw 32-bit RGBA pixel bytes (pass a Lua string, e.g. via `ffi.string(buf, len)`). |
| `UiForge.ReleaseTexture(handle)` | Releases a texture created by the above. |
| `UiForge.LoadFont(path[, size_px])` | Loads a `.ttf`/`.otf` font and returns an `ImFont` usable with `ImGui.PushFont`. Relative paths resolve like `LoadTexture`. On any failure (missing file, bad font) it returns the default font, so `PushFont` is always safe. Repeat loads of the same path and size return the same font. |
| `UiForge.LoadSound(path)` | Loads an `.mp3` or `.wav` file and returns a sound handle, or `nil` when the file is missing or cannot be opened. Relative paths resolve like `LoadTexture`. Repeat loads of the same file return the same handle. |
| `UiForge.PlaySound(handle[, options])` | Plays a loaded sound from the beginning. `options` is a table supporting `volume` (0.0 to 1.0, default 1.0) and `loop` (default false). |
| `UiForge.StopSound(handle)` | Stops a playing sound. |
| `UiForge.IsSoundPlaying(handle)` | Returns whether the sound is currently playing. |
| `UiForge.SetSoundVolume(handle, volume)` | Adjusts a sound's volume (0.0 to 1.0), including while it is playing. |
| `UiForge.ReleaseSound(handle)` | Releases a loaded sound. All sounds are released automatically on eject. |
| `UiForge.RegisterCallback(type, fn)` | Registers a callback for the current script (see below). |
| `UiForge.CallbackType` | Table of callback type constants: `Settings`, `DisableScript`, `Save`, `Load`, `OnEject`. |

### Script callbacks

A script registers callbacks with `UiForge.RegisterCallback(UiForge.CallbackType.<Type>, function ... end)`:

- **`Settings`** - Renders the script's own settings UI inside the UiForge Settings panel (Settings tab) when the script is selected.
- **`DisableScript`** - Teardown/cleanup; runs once when the script transitions from enabled to disabled.
- **`Save`** - Returns a plain-data table (or `nil` to skip). The returned table is captured into the profile being saved. Non-table return values are ignored with a warning.
- **`Load`** - Receives the table previously produced by this script's `Save` callback when a profile is applied.
- **`OnEject`** - Last-chance cleanup for every script (enabled or not), run right before the core unloads.

## Profiles

Instead of saving individual scripts, UiForge saves **profiles**. A profile captures:

1. Which scripts are enabled.
2. Each enabled script's state, as returned by its `Save` callback.
3. The ImGui window layout (positions, sizes, and collapsed state).

Profiles are stored as `scripts\profiles\<name>.profile.lua`. Profile names must be valid file names. Currently profile names are capped at 64 characters.

**Applying a profile** (File > Select Profile) enables exactly the scripts the profile lists, disables all others, then restores each script's saved state (via its `Load` callback) and the saved window layout.

**Saving a profile** (File > Save Profile...) opens a dialog, prefilled with the current profile name, to capture the current setup under a name of your choosing.

**Preferred profile per process** (File > Set Profile As Preferred): you can mark the current profile as preferred for the current target executable. Preferences are stored per process (keyed by the lowercased executable name) in `scripts\profiles\preferred_profiles.lua`, and the preferred profile is applied automatically the next time UiForge is injected into that same executable. Because preferences are per-process, a profile made for one game will not be auto-loaded into a different process.

## Configuration

Options are set in the `config` file as `KEY=value` pairs.

| Key | Description |
|-----|-------------|
| `CORE_DLL` | Path to the core DLL to inject (relative to `UiForge.exe`, or an absolute path). Default `bin\uiforge_core.dll`. |
| `FORGE_BIN_DIR` | Bin directory. |
| `FORGE_SCRIPT_DIR` | Scripts directory (relative to the config file). |
| `FORGE_MODULES_DIR` | Modules directory (relative to the scripts directory). |
| `FORGE_RESOURCES_DIR` | Resources directory (relative to the scripts directory). |
| `RELOAD_ON_SAVE` | `1` enables automatic reloading of scripts when their file changes; `0` disables. |
| `RELOAD_ON_SAVE_POLL_MS` | How often (ms) to poll script timestamps when reload-on-save is enabled. Default 2500. |
| `SETTINGS_ICON_FILE` | Settings icon image file (in the resources directory). |
| `SETTINGS_ICON_SIZE_X` / `SETTINGS_ICON_SIZE_Y` | Settings icon size in pixels. |
| `GRAPHICS_API` | `auto` (default), `d3d11`, or `d3d12`. `auto` detects the API from the DLLs loaded in the target (preferring D3D12 when both are present). |
| `MAX_LOG_SIZE_BYTES` | Maximum size of a single rolling log file. |
| `MAX_LOG_FILES` | Number of rolling log files to keep. |
| `INJECTOR_LOG_FILE_NAME` | Injector log file name. Default `inject_log.txt`. |
| `LOG_FILE_NAME` | Core log file name. Default `forge_log.txt`. |
| `LOGGING_LEVEL` | `0` none, `1` fatal, `2` error, `3` warning, `4` info, `5` debug, `6` verbose. |

> The `profiles` directory is not configurable; it is always `<scripts directory>\profiles`.

### Logging

UiForge writes two rolling logs, both governed by `LOGGING_LEVEL`, `MAX_LOG_SIZE_BYTES`, and `MAX_LOG_FILES`:

- **Injector log** (`INJECTOR_LOG_FILE_NAME`, default `inject_log.txt`) is written to the injector's working directory. It records the injection process: argument parsing, the process scan and match results, and the per-process open/inject/verify/run/wait steps and any errors. The same output is mirrored into the injector's on-screen log panel.
- **Core log** (`LOG_FILE_NAME`, default `forge_log.txt`) is written next to the `config` file. It records the in-process lifecycle: configuration values, kiero initialization and graphics-API selection, hook binding, ImGui/graphics initialization, script loading/execution/reload, profile save/apply, callback errors, and cleanup on eject.

## Testing/Demo
If you would like to test UiForge out, you can build the test application by running:
```
.\build_uiforge.bat testd3d11
```
This produces `test_d3d11_window.exe` in the `bin` directory. Run it, then start UiForge targeting the test window:
```
.\UiForge.exe -n test_d3d11_window.exe
```
And there you go! Now you can mess around with UiForge in the test app.

## Examples: UiForge in Action

Examples are incoming -- currently working on a project that will be using this!

## Contributing

Contributions are welcome! I am very new to C++ (all my professional experience is in C) so I am open to suggestions, advice, or criticism that will help me improve. Currently the code is a hodgepodge of what I would call "C"-isms and "C++"-isms. I'd like to clean it up as I learn more about C++ and OOP.

Please open an issue or submit a pull request for any features or fixes you'd like to see.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE.txt) file for details.

## Acknowledgements

- **[Dear ImGui](https://github.com/ocornut/imgui)** - The graphics library
- **[Kiero Library](https://github.com/Rebzzel/kiero)** - Graphics API hooking library
- **[MinHook](https://github.com/TsudaKageyu/minhook)** - Used by Kiero for hooking
- **[Sol2](https://github.com/ThePhD/sol2)** - C++/Lua binding library
- **[LuaJIT](https://luajit.org/)** - Just-in-time Lua interpreter
- **[DirectXTK](https://github.com/microsoft/DirectXTK)** - WIC texture loading (D3D11)
- **[FTXUI](https://github.com/ArthurSonzogni/FTXUI)** - Terminal UI for the injector
- **[plog](https://github.com/SergiusTheBest/plog)** - Logging framework
- **[SCL](https://github.com/WizardCarter/simple-config-library)** - config library
- **[serpent](https://github.com/pkulchenko/serpent)** - Lua serializer used for profiles
- **[sol2_ImGui-Bindings](https://github.com/Fesmaster/sol2_ImGui_Bindings)** - Original bindings I pulled from. I have forked the repo and made changes. The changes can be found at https://github.com/mmvest/sol2_ImGui_Bindings.

## Disclaimer

This tool is for educational and developmental purposes only. Use at your own risk and ensure you have the right to modify the target application. I am not responsible for how users choose to utilize this tool. This includes, but is not limited to, any consequences of:

- Being banned from games, platforms, or services.
- Running malicious Lua scripts using UiForge.

By using this tool, you accept full responsibility for your actions and any outcomes that may result.
