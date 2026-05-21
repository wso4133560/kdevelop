# IFFT KDevelop CK803 调试说明

本文档记录当前已验证可用的 `ifft` 工程在 KDevelop 中构建、下板和调试的流程。

## 环境路径

- 工程目录：`C:\code\ifft\ifft`
- KDevelop 启动脚本：`C:\code\kdevelop\run-kdevelop-ifft-project.bat`
- KDevelop 程序：`C:\tmp\kdevelop-native-msvc-install\bin\kdevelop.exe`
- 目标 ELF：`C:\code\ifft\ifft\Obj\ifftt.elf`
- 汇编输出：`C:\code\ifft\ifft\Lst\ifftt.asm`
- C-Sky 工具链：
  `C:\Users\tanglin\AppData\Roaming\C-Sky\ck803_toolkit_extract\compiler\CKV2ElfMinilib_V3.10.29`
- GDB：
  `C:\Users\tanglin\AppData\Roaming\C-Sky\ck803_toolkit_extract\compiler\CKV2ElfMinilib_V3.10.29\bin\csky-elfabiv2-gdb.exe`
- DebugServer 端口：`39000`

## 启动 IDE

推荐使用固定启动脚本：

```bat
C:\code\kdevelop\run-kdevelop-ifft-project.bat
```

如果需要重启 IDE：

```powershell
Stop-Process -Force -Name kdevelop
Start-Process C:\code\kdevelop\run-kdevelop-ifft-project.bat
```

正常打开后，窗口标题应类似：

```text
ifft-ck803: ifft - KDevelop
```

## 构建工程

KDevelop 中已经配置为使用完整构建脚本：

```bat
C:\Windows\System32\cmd.exe /c "C:\code\ifft\ifft\utilities\kdevelop\build_all.cmd" ifftt
```

在 IDE 里构建：

1. 左侧 `Projects` 面板选中根节点 `ifft`。
2. 右键选择 `Build`，或使用顶部菜单 `Project -> Build Selection`。
3. 构建成功后应生成：

```text
C:\code\ifft\ifft\Obj\ifftt.elf
C:\code\ifft\ifft\Lst\ifftt.asm
```

说明：

- `ifftt.elf` 是 CK803 目标 ELF，不是 Windows 可执行程序。
- 如果 THU 阶段返回 `1`，但 `ConfigPack` 头文件已存在，当前脚本会按 warning 处理并继续。
- C 编译阶段目前仍有若干 `implicit declaration` warning，但不影响当前 core build 通过。

## DebugServer

固定 DebugServer 启动脚本：

```bat
C:\Users\tanglin\AppData\Roaming\C-Sky\ck803_toolkit_extract\scripts\start_ifft_debugserver_39000.cmd
```

它会启动：

```text
DebugServerConsole.exe -port 39000 -setclk 12000k -nrstdelay 100 -rstwait 50 -ide
```

确认端口监听：

```powershell
netstat -ano | Select-String ':39000'
```

正常应看到：

```text
0.0.0.0:39000  LISTENING
```

## KDevelop 调试配置

当前 KDevelop Launch 配置名称：

```text
CK803 ifftt remote
```

关键配置：

- Executable：`C:\Windows\System32\cmd.exe`
- GDB Path：`csky-elfabiv2-gdb.exe`
- Remote GDB Config Script：
  `C:\code\ifft\ifft\utilities\kdevelop\gdb_ck803_config.gdb`
- Remote GDB Run Script：
  `C:\code\ifft\ifft\utilities\kdevelop\gdb_ck803_run.gdb`

注意：

- 不要把 `C:\code\ifft\ifft\Obj\ifftt.elf` 填到 KDevelop 的 Windows Executable 字段。
- `ifftt.elf` 是下到板子上的 CK803 程序，由 GDB 脚本加载。
- KDevelop 的 Executable 字段使用 `cmd.exe` 只是为了通过 Native Application 的本机可执行文件校验。

## GDB 脚本

`gdb_ck803_config.gdb`：

```gdb
set pagination off
set confirm off
set breakpoint pending on
set remotetimeout 10
directory C:/code/ifft/ifft
directory C:/code/ifft/ifft/src/TC_adda_mix/c
file C:/code/ifft/ifft/Obj/ifftt.elf
target remote localhost:39000
monitor reset halt
```

`gdb_ck803_run.gdb`：

```gdb
load
set $pc = __start
tbreak main
continue
```

调试启动后，GDB 会：

1. 连接 `localhost:39000`。
2. reset/halt 板子。
3. 加载 `ifftt.elf` 到板子。
4. 设置 PC 到 `__start`。
5. 临时断点停在 `main`。

## 启动调试

在 KDevelop 中：

- 推荐快捷键：`Alt+F9`
- 菜单入口：`Run` / `运行` -> `Debug Launch` / `调试启动`

调试常用快捷键：

- `Ctrl+Alt+B`：在当前行切换断点
- `F10`：Step Over
- `F11`：Step Into
- `F12`：Step Out

推荐先在这些源码位置打断点：

```text
C:\code\ifft\ifft\src\TC_adda_mix\c\th01A_cgra1_main.c:39
C:\code\ifft\ifft\src\TC_adda_mix\c\th01A_cgra1_16k_time_test.c:111
```

## 断点说明

如果点击源码左侧行号导致代码折叠，说明点到了 folding gutter，不是 breakpoint gutter。

建议：

- 使用 `Ctrl+Alt+B` 在当前光标行打断点。
- 断点应打在 `.c` 源文件的有效代码行上。
- 不要打在空行、注释行、`.asm` 输出文件或报告文件里。
- 如果源码左侧没有断点栏，在 KDevelop 中检查：
  `View -> Borders -> Show Icon Border`

## 已处理的问题

### 工程无法加载

工程文件：

```text
C:\code\ifft\ifft\ifft.kdev4
```

已配置为：

```ini
[Project]
Name=ifft
Manager=KDevCustomBuildSystem
```

同时修复了 KDevelop 插件路径和 session 配置，当前工程可以正常打开。

### 构建后处理脚本误报

此前 `ifftt.modify.bat` 被 CK803 构建环境中的 `sh` 执行，出现：

```text
@echo: command not found
SET: command not found
错误: 指定的目录不存在。
```

已将相关 `.modify.bat` 脚本改为同时兼容 `cmd` 和 `sh`，构建核心输出已通过。

### ELF 不是可执行程序

`C:\code\ifft\ifft\Obj\ifftt.elf` 是 CK803 目标程序，不是 Windows 程序。

KDevelop Native Application 的 Executable 字段不能直接填它；当前使用 `cmd.exe` 作为占位，真正的 ELF 由 GDB `file/load` 命令处理。

### GDB 版本误判

C-Sky GDB 输出：

```text
GNU gdb (T-HEAD C-SKY Tools V3.10.29 Minilibc abiv2) 7.12
```

KDevelop 原逻辑会先匹配到 `V3.10.29`，误判 GDB 版本低于 `7.0.0`。

已修复 KDevelop GDB 插件版本解析逻辑，改为取 `GNU gdb` 行中的最后一个版本号，即 `7.12`。

## 手动下板调试脚本

如果不通过 IDE，也可以使用固定脚本手动连接和下板：

```bat
C:\Users\tanglin\AppData\Roaming\C-Sky\ck803_toolkit_extract\scripts\start_ifft_debugserver_39000.cmd
C:\Users\tanglin\AppData\Roaming\C-Sky\ck803_toolkit_extract\scripts\run_ifftt_debugserver_39000.cmd
```

`run_ifftt_debugserver_39000.cmd` 会使用当前工程的：

```text
C:\code\ifft\ifft\Obj\ifftt.elf
```

并通过 `csky-elfabiv2-gdb.exe` 连接 `localhost:39000`。

## 快速检查清单

- KDevelop 已通过 `run-kdevelop-ifft-project.bat` 启动。
- 工程根节点 `ifft` 已出现在左侧 Projects 面板。
- `build_all.cmd ifftt` 构建后生成 `Obj\ifftt.elf`。
- DebugServer 在 `39000` 端口监听。
- Launch 配置选择 `CK803 ifftt remote`。
- GDB Path 指向 `csky-elfabiv2-gdb.exe`。
- Remote GDB 脚本指向 `gdb_ck803_config.gdb` 和 `gdb_ck803_run.gdb`。
- 使用 `Alt+F9` 启动调试。
- 首次进入后应停在 `main`。

## 串口监视窗口

IDE 已增加底部工具窗口：

```text
Serial Port
```

功能：

- 自动扫描 Windows 串口设备。
- 端口下拉框优先显示设备完整名称，例如 `USB-SERIAL CH340 (COM3)`。
- 如果设备没有友好名称，会回退显示 `COM3` 这类端口号。
- 端口输入框仍可手动填写，例如 `COM3`、`COM10`。
- 支持选择常用波特率，默认 `115200`。
- `Open` 打开串口。
- `Close` 关闭串口。
- `Clear` 清空当前串口输出。
- 串口打印会实时显示在 IDE 底部窗口栏。

使用步骤：

1. 打开 KDevelop 后，在底部工具栏找到 `Serial Port`。
2. 点击 `Refresh` 扫描串口。
3. 选择板子的串口号。
4. 确认波特率，通常先用 `115200`。
5. 点击 `Open`。
6. 板子输出会显示在窗口下方文本区域。
7. 调试结束后点击 `Close` 释放串口。

说明：

- 如果下拉框没有列出端口，可以直接手动输入端口号。
- 如果打开失败，通常是串口被其它工具占用，先关闭其它串口工具后再打开。
- 当前串口窗口只做接收显示，不做发送命令。
