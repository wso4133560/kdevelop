# RRISE Handoff

## Repository and current state

- Repository: `D:\code\kdevelop-commit\kdevelop`
- Branch: `master`
- Use `git log -1 --oneline` for the current revision.
- The debugger changes described below are committed together.
- `git diff --check` and the Release build passed before the commit.

## Current debugger work

The most recent work addresses four related RRISE debugger issues.

1. Opening the Disassemble/Registers view after a breakpoint used to crash the IDE.
   - Cause: an incremental build had compiled the disassembly view against an old `MIDebugSession` virtual-function table layout.
   - Fix: `prepareDebugging()` and `preferHardwareBreakpoints()` were moved after the original virtual interface in `midebugsession.h`. This keeps existing virtual slots stable.
   - Important: when changing this interface, perform a clean rebuild of the debugger targets. The Ninja dependency tracking in this build has previously failed to rebuild all C++ users of changed headers.

2. The Windows interrupt action needed a MI-based interrupt path.
   - `plugins/gdb/debugsession.cpp` enables `mi-async on` on Windows.
   - `MIDebugSession::interruptDebugger()` sends `-exec-interrupt` in addition to the existing signal-based wake-up path.

3. Interrupting a remote CK803 target could open an empty disassembly view.
   - Some remote GDB `*stopped` records do not contain a complete `frame` field.
   - `DisassembleWidget` now refreshes on `inferiorStopped` and evaluates `$pc` as a fallback, then disassembles around that address.

4. Source and assembly stepping are selected explicitly.
   - The Disassemble/Registers view has a checkable button labelled `保持汇编调试` or `取消汇编调试`.
   - When assembly stepping is enabled, normal Step Over / Step Into and `F10` / `F11` send `-exec-step-instruction` (`stepi`).
   - When it is disabled, the same actions retain source-level `next` / `step` behavior.
   - The setting is persisted in the `Disassemble/Registers View` config group and published through the application property `rriseInstructionSteppingEnabled`.
   - Duplicate step requests are guarded until the debugger reports a state transition.

## Build locations

- Build directory: `C:\tmp\kdevelop-native-msvc-systemqt-riscvtpl`
- Install tree: `C:\tmp\kdevelop-native-msvc-install-riscvtpl`
- Installer staging tree: `D:\tmp\kdevelop-native-installer\staging\app`
- Installer output: `D:\tmp\kdevelop-native-installer\artifacts\RRISE-Setup.exe`
- Installed application: `C:\Program Files\RRISE`

The latest installer was built on 2026-07-31 15:21:06:

- Path: `D:\tmp\kdevelop-native-installer\artifacts\RRISE-Setup.exe`
- Size: `508298871` bytes
- SHA-256: `F3F654786A0A406F5245C20160F7937A96D7A02C1BD887F2D5A33689FAF915BD`

## Build and package commands

Run these from the repository root in PowerShell.

```powershell
$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
cmd.exe /d /s /c "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && cmake --build C:\tmp\kdevelop-native-msvc-systemqt-riscvtpl --config Release --target KDevPlatformShell kdevgdb --parallel 4"
```

When a virtual interface in `midebugsession.h` changes, clean the debugger targets first:

```powershell
ninja -C C:\tmp\kdevelop-native-msvc-systemqt-riscvtpl -t clean kdevdebuggercommon kdevgdb_static kdevgdb
```

Copy the rebuilt binaries to the installer staging tree before packaging:

```powershell
$build = 'C:\tmp\kdevelop-native-msvc-systemqt-riscvtpl\bin'
$stagingApp = 'D:\tmp\kdevelop-native-installer\staging\app'
Copy-Item "$build\KDevPlatformShell.dll" "$stagingApp\bin" -Force
Copy-Item "$build\kdevgdb.dll" "$stagingApp\bin" -Force
Copy-Item "$build\kdevgdb.dll" "$stagingApp\lib\plugins\kdevplatform\66" -Force
```

The last copy is mandatory. RRISE loads `kdevgdb.dll` from
`lib\plugins\kdevplatform\66`, not from `bin`. Verify the loaded module path
with `Get-Process kdevelop` before diagnosing stale debugger behavior.

Package the installer:

```powershell
& 'C:\Program Files (x86)\NSIS\makensis.exe' `
  '/DAPP_SOURCE=D:\tmp\kdevelop-native-installer\staging' `
  '/DOUTFILE=D:\tmp\kdevelop-native-installer\artifacts\RRISE-Setup.exe' `
  'packaging\windows\kdevelop-installer.nsi'
```

NSIS compresses about 1.8 GB of staged content and normally runs for several minutes without intermediate output. Do not start a second NSIS process while one is writing `RRISE-Setup.exe`.

## Validation checklist

1. Close RRISE and install the latest `RRISE-Setup.exe` over the existing installation.
2. Connect the CK-Link board and start a normal remote debug session.
3. Stop at a breakpoint, then open Disassemble/Registers. The IDE must not crash.
4. Continue execution, click Interrupt, and verify the disassembly view is raised and shows instructions around the stopped program counter.
5. Enable `保持汇编调试`, then press `F10` or `F11`; verify exactly one assembly instruction is executed.
6. Select `取消汇编调试`, then press `F10` or `F11`; verify source-level stepping is used.

## Known constraints and troubleshooting

- Do not manually overwrite `C:\Program Files\RRISE` from a non-elevated shell; it normally fails with access denied. Use the installer after closing RRISE.
- A running `DebugServerConsole.exe` may be left from older tests. The installer has logic to stop it, but inspect it if an installation reports files in use.
- The hardware debug server listens on `localhost:39000`. If it cannot open the port, check the CK-Link cable/board connection before diagnosing the IDE.
- `D:\tmp\rrise-gdb-mi-trace.log` contains RRISE-specific GDB tracing from earlier debugging sessions. It is diagnostic output outside the repository and is not source-of-truth for current code.
- The currently installed IDE can be an older build than the staging tree. Always verify installation timestamp/hash before reproducing a reported runtime issue.

## Verification commands

Review the debugger-fix set with:

```powershell
git diff --check
git status --short
Get-Process kdevelop | ForEach-Object {
  $_.Modules | Where-Object ModuleName -eq 'kdevgdb.dll' |
    Select-Object ModuleName, FileName
}
```
