# KDevelop 官方 Windows 打包流程研究

## 研究目标

研究当前项目如何按照 **官方 Windows 产物风格** 进行打包，并识别：

- 官方目录是什么类型的产物
- 官方打包器对应的是哪条 Craft 流程
- 当前项目如何尽量复现同样的打包结果

## 官方参考路径

你给出的官方路径：

- `C:\Users\tanglin\Downloads\kdevelop-master-6639-windows-cl-msvc2022-x86_64`

同目录下还存在对应压缩包：

- `C:\Users\tanglin\Downloads\kdevelop-master-6639-windows-cl-msvc2022-x86_64.7z`

这说明：

- 当前目录是 **`.7z` 便携包解压后的内容**
- 它不是 NSIS 安装器解包目录
- 它也不是源码构建目录

## 官方包的目录结构特征

官方便携包解压后，顶层目录是：

- `bin`
- `etc`
- `lib`
- `libexec`
- `licenses`
- `share`

这是一个很典型的 **Craft 归档包布局**，特征包括：

- 主程序和大量依赖 DLL 放在 `bin`
- Qt 插件也被部署到 `bin` 下的多个子目录中
- 文档、locale、icons、desktop 元数据等放在 `share` 或 `bin\data`
- `lib\plugins`、`qml`、`translations` 等运行时内容都已经被整理成可分发形态

## 结论：官方参考目录对应的是哪种打包方式

从目录结构和同名 `.7z` 文件可以判断：

- **官方参考目录对应的是 Craft 的便携归档流程**
- 最接近的打包器是：
  - `PortablePackager`

不是：

- `NullsoftInstallerPackager` 产出的安装器 `.exe`
- 也不是单纯 `windeployqt` 手工拷贝流程

## 代码级证据

### 1. KDevelop 蓝图定义了打包信息

KDevelop 的 Craft 蓝图：

- `C:\CraftRoot\etc\blueprints\locations\craft-blueprints-kde\kde\kdevelop\kdevelop\kdevelop.py`

其中可以确认：

- `preArchive()` 会执行颜色主题安装脚本
- `createPackage()` 定义了：
  - 快捷方式
  - 应用图标
  - 包含 `kdevelop.exe`
  - 包含 `kdevelop-msvc.bat`

### 2. PortablePackager 的行为

Craft 中：

- `C:\CraftRoot\craft\bin\Packager\PortablePackager.py`

其特征：

- 在 `archiveDir()` 收集完整运行树
- 最后压成 `.7z`
- 保留目录结构

这与官方参考目录完全吻合。

### 3. Windows 默认安装器路径

Craft 中：

- `C:\CraftRoot\craft\bin\Packager\TypePackager.py`

在 Windows 平台如果不显式指定，默认偏向：

- `NullsoftInstallerPackager`

也就是：

- 默认更像 “生成安装器 exe”
- 但你给的官方参考目录明显不是这条产物

所以如果要复现你给出的官方 `.7z` 包，必须显式选：

- `PortablePackager`

## 官方 Windows 打包的实际逻辑链

官方风格可以概括为下面几步：

1. 用 Craft 构建 `kde/kdevelop`

Craft 会：

- 编译 KDevelop
- 安装到自己的 image/install 目录
- 拉齐 Qt、KF6、LLVM、Kate 插件等运行时依赖

2. 用 `CollectionPackagerBase` 收集整个运行树

这一步会把：

- 主程序
- Qt 运行时
- KDE Frameworks
- 插件
- QML
- 图标
- 语言包
- 许可证

统一整理进一个可分发目录。

3. 用 `PortablePackager` 压成 `.7z`

最终产物命名风格类似：

- `kdevelop-master-6639-windows-cl-msvc2022-x86_64.7z`

4. 用户解压后得到目录树

也就是你给的：

- `kdevelop-master-6639-windows-cl-msvc2022-x86_64`

## 如果要打成“官方同风格”的包，推荐流程

### 方案 A：最接近官方的 Craft 便携包流程

这是最推荐的方案。

前提：

- 使用 Craft 作为构建与收集依赖的中心
- 最终选择 `PortablePackager`

推荐命令思路：

```bat
C:\Users\tanglin\AppData\Local\Programs\Python\Python313\python.exe C:\CraftRoot\craft-tmp\bin\craft.py ^
  --options kde/kdevelop.srcDir=C:\code\kdevelop ^
  --options kde/kdevelop.buildTests=False ^
  kde/kdevelop
```

然后打包：

```bat
C:\Users\tanglin\AppData\Local\Programs\Python\Python313\python.exe C:\CraftRoot\craft-tmp\bin\craft.py ^
  --options [Packager]PackageType=PortablePackager ^
  kde/kdevelop --package
```

说明：

- 第一条命令负责构建和安装
- 第二条命令负责按便携归档方式收集并输出 `.7z`

### 方案 B：生成官方 Windows 安装器

如果你要的是安装器 `.exe`，不是 `.7z` 便携包，则走：

- `NullsoftInstallerPackager`

命令思路：

```bat
C:\Users\tanglin\AppData\Local\Programs\Python\Python313\python.exe C:\CraftRoot\craft-tmp\bin\craft.py ^
  --options [Packager]PackageType=NullsoftInstallerPackager ^
  kde/kdevelop --package
```

说明：

- 这条会更接近“官方安装器 exe”流程
- 但它不对应你给出的参考目录

## 当前项目和官方参考包的差异

你现在有两条构建线：

### 1. Craft 线

- 运行产物：`C:\CraftRoot\bin\kdevelop.exe`
- 优点：
  - 依赖链闭合
  - 最容易直接走 Craft 官方打包流程
- 缺点：
  - 不是“纯系统 Qt”

### 2. 原生系统 Qt 线

- 运行产物：`C:\tmp\kdevelop-native-msvc-install\bin\kdevelop.exe`
- 优点：
  - 真正基于系统 Qt 6.11
- 缺点：
  - 为了消除加载器错误，已经手工替换了一批 KF6 DLL
  - 它当前不是 Craft 默认认知的标准打包输入

结论：

- 如果目标是“尽量复现官方 Windows `.7z` 包”，应优先以 **Craft 线** 为打包基础
- 如果目标是“把当前系统 Qt 原生运行树打成一个自定义便携包”，也可以做，但那是 **定制打包**，不完全等同于官方流水线

## 如何判断打包是否成功

一个成功的官方风格便携包，应满足：

- 顶层有 `bin / etc / lib / libexec / licenses / share`
- `bin\kdevelop.exe` 可以直接启动
- 插件目录齐全
- Qt 插件目录齐全
- `No Plugins Found` 不出现
- 解压后不依赖源码目录

## 你当前最推荐的打包路线

如果你的目标是：

### “做出和官方 `kdevelop-master-6639-windows-cl-msvc2022-x86_64.7z` 同风格的包”

推荐：

- 使用 **Craft 构建 + PortablePackager 打包**

### “把现在这个已经修过加载器问题的系统 Qt 原生版本也打成一个包”

推荐：

- 先把 `C:\tmp\kdevelop-native-msvc-install` 视为最终运行树
- 再单独做归档或安装器封装
- 这条更像“自定义分发流程”

## 实用建议

最稳的顺序是：

1. 先用 Craft 线打出一份官方风格 `.7z`
2. 用你的官方参考目录对比：
   - 顶层结构
   - `bin` 下 DLL/插件数量
   - `share`、`licenses`、`libexec` 内容
3. 再决定是否要把原生系统 Qt 线也做成第二套包

## 最终结论

你给出的官方 Windows 参考包：

- 本质上是 **Craft PortablePackager 产出的 `.7z` 便携包解压目录**

不是：

- NSIS 安装器目录
- 单纯 `windeployqt` 部署目录

因此，如果要尽量按官方流程打包当前项目，核心路线应当是：

- `Craft 构建`
- `PortablePackager 打包`
- 产出 `.7z`
- 解压后得到与你提供的官方目录风格一致的运行树
