# %{APPNAME}

This project template follows the `D:\ifft` layout:

- `include`
- `lnk`
- `src/TC_adda_%{APPNAMELC}`
- `utilities`

The generated project is meant to be opened directly in RRISE/KDevelop and built with the
installed `riscv_toolkit` payload from the RRISE installer.

The project uses `KDevCustomMakeManager`. A wrapper `Makefile` is generated so the
KDevelop custom make plugin can recognize the project, while the main build logic
stays in `%{APPNAMELC}.mk`.

## Toolkit path

The helper scripts look for the toolkit in this order:

1. `RRISE_TOOLKIT_ROOT`
2. `C:\Program Files\RRISE\riscv_toolkit`
3. `C:\Program Files (x86)\RRISE\riscv_toolkit`

## Build entry points

- `utilities\kdevelop\build_all.cmd`
- `utilities\kdevelop\build_csky_target.cmd`
- `utilities\kdevelop\clean_csky_target.cmd`
- `utilities\kdevelop\start_ck803_debugserver_39000.cmd`
- `utilities\kdevelop\start_ck803_gdb.cmd`

The default make target and output name is `%{APPNAMELC}`.
