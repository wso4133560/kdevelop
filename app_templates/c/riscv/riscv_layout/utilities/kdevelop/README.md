# KDevelop CK803 helpers

These scripts build and debug the generated project against the toolkit installed by RRISE.

- `build_all.cmd` runs the default C-Sky build.
- `build_csky_target.cmd` builds `%{APPNAMELC}.mk`.
- `clean_csky_target.cmd` removes generated `Obj` and `Lst` outputs.
- `start_ck803_debugserver_39000.cmd` starts the T-Head debug server on port `39000`.
- `start_ck803_gdb.cmd` starts `csky-elfabiv2-gdb` and executes the local startup scripts.
