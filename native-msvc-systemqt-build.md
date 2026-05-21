# KDevelop 原生 MSVC + 系统 Qt 编译说明

## 目标

使用系统安装的 Visual C++ 工具链和系统 Qt 安装目录，编译当前 `KDevelop` 源码，并产出可正常启动的 GUI 程序。

## 源码目录

- 源码路径：`C:\code\kdevelop`

## 工具链与依赖

- 编译器：Visual Studio 2022 MSVC x64
- 系统 Qt：`C:\Qt\6.11.0\msvc2022_64`
- CMake：`C:\Qt\Tools\CMake_64\bin\cmake.exe`
- 生成器：`Ninja`
- 编译并行度：`16`

## 原生构建目录

- 构建目录：`C:\tmp\kdevelop-native-msvc-systemqt`
- 安装目录：`C:\tmp\kdevelop-native-msvc-install`

## 原生配置命令

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" ^
  -S C:\code\kdevelop ^
  -B C:\tmp\kdevelop-native-msvc-systemqt ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DBUILD_DOC=OFF ^
  -DBUILD_TESTING=OFF ^
  -DCMAKE_INSTALL_PREFIX=C:\tmp\kdevelop-native-msvc-install ^
  -DCMAKE_PREFIX_PATH="C:\Qt\6.11.0\msvc2022_64;C:\CraftRoot"
```

## 原生编译命令

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build C:\tmp\kdevelop-native-msvc-systemqt --parallel 16
```

## 安装命令

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --install C:\tmp\kdevelop-native-msvc-systemqt
```

## 补装的系统 Qt 模块

最初系统 Qt 安装不完整，无法满足 KDevelop 的配置需求。通过 `C:\Qt\MaintenanceTool.exe` 补装了以下模块：

- `Qt5Compat`
- `Qt WebEngine`
- `Qt Help`
- `Qt WebChannel`
- `Qt Positioning`
- `Qt Speech`

## 运行时兼容性问题

最开始的原生构建虽然可以编译通过，但启动时会触发加载器错误 `0xC0000139`。

根因如下：

- 原生 `kdevelop.exe` 是按系统 Qt 6.11 编译的。
- `C:\CraftRoot\bin` 里的一批 `KF6` 运行时 DLL 是按另一套 Qt 运行时特性编出来的。
- 这些 DLL 中有若干会导入 `qResourceFeatureZstd`，而系统 `Qt6Core.dll` 不提供该导出。

## 已重编并替换的 KF6 DLL

以下 DLL 已经按系统 Qt 6.11 重新编译，并同步到原生运行目录中：

- `KF6WidgetsAddons.dll`
- `KF6SonnetCore.dll`
- `KF6SonnetUi.dll`
- `KF6SyntaxHighlighting.dll`
- `KF6TextEditor.dll`
- `KF6BreezeIcons.dll`
- `KF6IconThemes.dll`
- `KF6ColorScheme.dll`
- `KF6XmlGui.dll`

这些 DLL 当前位于：

- `C:\tmp\kdevelop-native-msvc-install\bin`

## Breeze Icons 的特殊处理

`breeze-icons` 在 Windows 上构建失败，不是 ABI 问题，而是资源别名生成器会把重复 SVG 内容当成致命错误。

处理方式：

- 将源码复制到：`C:\tmp\src-kf6-systemqt\breeze-icons-6.26.0`
- 修改 `tools\qrcAlias.cpp`
- 将“遇到重复资源立即 `qFatal` 退出”改成“输出 warning 并跳过重复项”

## 最终可运行的 GUI 程序

- 原生安装后的 GUI 程序：
  - `C:\tmp\kdevelop-native-msvc-install\bin\kdevelop.exe`

## 最终运行验证

最终安装目录下的可执行文件已经可以正常启动 GUI 窗口：

- 观察到的窗口标题：`KDevelop`
- 进程状态：`Responding = True`

## 推荐启动方式

建议通过 install tree 启动脚本运行，这样会自动设置运行时所需环境变量：

- `C:\code\kdevelop\run-kdevelop-native-install.bat`

该脚本会设置：

- `PATH`
- `XDG_DATA_DIRS`
- `XDG_CONFIG_DIRS`
- `QT_PLUGIN_PATH`
- `QML2_IMPORT_PATH`
- `QT_QUICK_CONTROLS_STYLE_PATH`

## 一键复现步骤

如果要在当前机器上尽量复现本次结果，建议按下面顺序执行：

1. 准备系统 Qt 组件

确保 `C:\Qt\6.11.0\msvc2022_64` 已安装，并补齐这些模块：

- `Qt5Compat`
- `Qt WebEngine`
- `Qt Help`
- `Qt WebChannel`
- `Qt Positioning`
- `Qt Speech`

2. 配置原生构建目录

执行“原生配置命令”，生成：

- `C:\tmp\kdevelop-native-msvc-systemqt`

3. 全量并行编译

执行“原生编译命令”，并行度使用：

- `--parallel 16`

4. 安装到 install tree

执行“安装命令”，生成：

- `C:\tmp\kdevelop-native-msvc-install`

5. 处理运行时兼容 DLL

如果仍有 `0xC0000139` 加载器错误，则按本文中的方式重编这些 `KF6` 框架并同步到安装目录：

- `KF6WidgetsAddons.dll`
- `KF6SonnetCore.dll`
- `KF6SonnetUi.dll`
- `KF6SyntaxHighlighting.dll`
- `KF6TextEditor.dll`
- `KF6BreezeIcons.dll`
- `KF6IconThemes.dll`
- `KF6ColorScheme.dll`
- `KF6XmlGui.dll`

6. 从 install tree 启动

不要优先从 build tree 启动，建议直接运行：

- `C:\code\kdevelop\run-kdevelop-native-install.bat`

这样插件、QML、图标、数据目录路径都会指向安装目录布局。

## 过程中创建的辅助脚本

- `C:\code\kdevelop\run-kdevelop-native-msvc-systemqt.bat`
- `C:\code\kdevelop\run-kdevelop-native-install.bat`
- `C:\code\kdevelop\build_sonnet_systemqt.bat`
- `C:\code\kdevelop\build_syntax_highlighting_systemqt.bat`
- `C:\code\kdevelop\build_ktexteditor_systemqt.bat`
- `C:\code\kdevelop\build_breeze_icons_systemqt.bat`
- `C:\code\kdevelop\build_kiconthemes_systemqt.bat`
- `C:\code\kdevelop\build_kcolorscheme_systemqt.bat`
- `C:\code\kdevelop\build_kxmlgui_systemqt.bat`
- `C:\code\kdevelop\sync_systemqt_kf6_to_native_install.bat`

## 故障排查

### 1. 提示缺少 Qt6 组件

常见现象：

- 缺少 `Qt6Core5Compat`
- 缺少 `Qt6WebEngineWidgets`
- 缺少 `Qt6Help`
- 缺少 `Qt6WebChannel`
- 缺少 `Qt6Positioning`

处理办法：

- 使用 `C:\Qt\MaintenanceTool.exe` 补装对应模块
- 补装完成后确认这些目录存在：
  - `C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6Core5Compat`
  - `C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6WebEngineWidgets`
  - `C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6Help`
  - `C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6WebChannel`
  - `C:\Qt\6.11.0\msvc2022_64\lib\cmake\Qt6Positioning`

### 2. 启动时报 `0xC0000139`

含义：

- 某个 DLL 依赖的导出符号在当前运行时里不存在

本次实际案例：

- `qResourceFeatureZstd`

处理办法：

- 不要混用“系统 Qt 6.11”与“旧 Craft Qt 特性构建的 KF6 DLL”
- 用系统 Qt 6.11 重新编译对应的 `KF6` 框架 DLL
- 再把新 DLL 复制到：
  - `C:\tmp\kdevelop-native-msvc-install\bin`

### 3. 出现 `No Plugins Found - KDevelop`

含义：

- 主程序已经启动，但插件搜索路径不对，或者你是从 build tree 直接起的

处理办法：

- 优先从 install tree 启动，而不是 build tree
- 使用：
  - `C:\code\kdevelop\run-kdevelop-native-install.bat`
- 确认以下路径存在：
  - `C:\tmp\kdevelop-native-msvc-install\lib\plugins\kdevplatform\66`
  - `C:\tmp\kdevelop-native-msvc-install\bin\data`

### 4. `breeze-icons` 构建失败

常见现象：

- 资源生成时报大量 duplicate icon / duplicate alias

处理办法：

- 使用文中记录的方法修改：
  - `tools\qrcAlias.cpp`
- 把重复资源从 fatal 改成 warning-and-skip
- 必要时再清理与 `-symbolic.svg` 对应的普通 SVG

### 5. 程序能启动但无界面

先区分两种情况：

- 进程直接退出
- 进程存在但没有主窗口

建议检查：

- `Get-Process kdevelop`
- Windows Application 事件日志
- `cdb + loader snaps`

### 6. 运行时路径混乱

推荐运行顺序：

1. `C:\tmp\kdevelop-native-msvc-install\bin`
2. `C:\Qt\6.11.0\msvc2022_64\bin`
3. `C:\CraftRoot\bin`

如果顺序错误，容易再次捡到不兼容的旧 `KF6`/`Qt6` DLL。

## 最终结果

- 原生配置：成功
- 原生编译：成功
- 原生安装：成功
- 致命加载器错误：已解决
- 最终 GUI：可正常启动
