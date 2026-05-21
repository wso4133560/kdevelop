# KDevelop CK803 Integration

Preferred GUI entry:

1. Run `C:\code\ifft\ifft\utilities\kdevelop\open_in_kdevelop.cmd`.
2. Or start the current KDevelop session directly; it is now configured to reopen `ifft` automatically.

Build and debug entry points:

1. `Project > Build Configuration > Configure` runs the THU generation step.
2. `Project > Build Selection` or the build toolbar button runs the C-Sky `ifftt` build.
3. `Tools > External Scripts > CK803 - Build All ifftt` runs THU and then C-Sky.
4. `Tools > External Scripts > CK803 - Start DebugServer 39000` starts the CK803 debug server.
5. `Run > Debug Launch` starts `csky-elfabiv2-gdb` and attaches to `localhost:39000`.

Recommended debug flow:

1. Build once with `CK803 - Build All ifftt`.
2. Start `CK803 - Start DebugServer 39000`.
3. Use `Run > Debug Launch`.

Notes:

1. The launch configuration debugs `Obj\ifftt.elf`.
2. The GDB startup scripts load the ELF, connect to port `39000`, reset, load, set `PC` to `__start`, break at `main`, and continue.
3. `build_thu.cmd` defaults to `-IFFT 14`. Edit the script if your THU invocation changes.
4. `open_in_kdevelop.cmd` opens the project directory, not the `.kdev4` file as a text document.
