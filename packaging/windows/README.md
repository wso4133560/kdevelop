# Windows installer packaging

This directory contains the custom Windows installer flow for the native MSVC + system Qt build.

## Goal

Produce a single installer exe that:

- installs the prepared KDevelop runtime tree,
- bundles Qt/KF/Craft runtime dependencies so users do not configure Qt environment variables manually,
- provides a selectable CP210x USB-to-UART driver component,
- installs `pics\清科芯擎LOGO-中英文组合-黑.svg` and uses a generated `pics\rrise-logo.ico` for the installer, shortcuts, and uninstall entry,
- creates shortcuts that start `KDevelop.exe`, a launcher that sets process-local runtime paths before starting `bin\kdevelop.exe`.

## Prerequisites

- Native install tree: `C:\tmp\kdevelop-native-msvc-install`
- Qt: `C:\Qt\6.11.0\msvc2022_64`
- Craft runtime DLL source: `C:\CraftRoot\bin`
- RISC-V toolkit directory: `D:\code\kdevelop-commit\riscv_toolkit`
- CP210x driver zip in the repository root: `CP210x_Windows_Drivers.zip`
- Visual Studio C++ build tools, for compiling `KDevelop.exe` launcher
- NSIS 3.x, for producing the final installer exe

## Build staging and installer

```powershell
.\packaging\windows\package-native-msvc-systemqt.ps1
```

Override the toolkit source directory if needed:

```powershell
.\packaging\windows\package-native-msvc-systemqt.ps1 -RiscvToolkitDir D:\path\to\riscv_toolkit
```

Default output:

- staging tree: `D:\tmp\kdevelop-native-installer\staging`
- installer exe: `D:\tmp\kdevelop-native-installer\artifacts\RRISE-Setup.exe`

If NSIS is not installed yet, prepare and inspect staging only:

```powershell
.\packaging\windows\package-native-msvc-systemqt.ps1 -SkipNsis
```

Quick prerequisite check:

```powershell
.\packaging\windows\package-native-msvc-systemqt.ps1 -ValidateOnly
```

## Runtime layout

The packaged app root contains `KDevelop.exe`. This is not the real KDevelop binary; it is a small launcher. It starts `bin\kdevelop.exe` with process-local values for:

- `PATH`
- `KDEDIRS`
- `XDG_DATA_DIRS`
- `XDG_CONFIG_DIRS`
- `QT_PLUGIN_PATH`
- `QML2_IMPORT_PATH`
- `QT_QUICK_CONTROLS_STYLE_PATH`

These variables are not written globally to the user machine.

The packaged app root also contains `riscv_toolkit\`, so templates can reference a stable installed path such as `C:\Program Files\RRISE\riscv_toolkit`.

## CP210x driver option

The NSIS script exposes `CP210x USB-to-UART driver` as an optional component. When selected, the installer extracts the CP210x driver files and runs the x64 or x86 Silicon Labs installer according to the target OS.
