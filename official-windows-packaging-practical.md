# KDevelop 官方风格 Windows 打包实操记录

## 目标

基于当前本地源码：

- `C:\code\kdevelop`

研究并实际跑通一条尽量接近官方 Windows 便携包的打包流程，产出：

- 官方风格 `.7z` 便携包

## 官方参考对象

你给出的官方参考目录：

- `C:\Users\tanglin\Downloads\kdevelop-master-6639-windows-cl-msvc2022-x86_64`

同目录下存在压缩包：

- `C:\Users\tanglin\Downloads\kdevelop-master-6639-windows-cl-msvc2022-x86_64.7z`

由此可知，官方参考对象本质上是：

- **Craft 便携包 `.7z` 解压后的目录树**

不是：

- NSIS 安装器解包目录
- 单纯 `windeployqt` 部署目录

## 结论

最接近官方参考包的流程是：

- `Craft 构建`
- `PortablePackager 打包`
- 输出 `.7z`

对应的打包器是：

- `PortablePackager`

## 关键代码依据

### 1. KDevelop 蓝图

文件：

- `C:\CraftRoot\etc\blueprints\locations\craft-blueprints-kde\kde\kdevelop\kdevelop\kdevelop.py`

可确认：

- `createPackage()` 中定义了：
  - `kdevelop.exe`
  - `kdevelop-msvc.bat`
  - 快捷方式
  - 图标

### 2. Craft 便携包打包器

文件：

- `C:\CraftRoot\craft\bin\Packager\PortablePackager.py`

其行为是：

- 先把完整运行树收集到 `archiveDir`
- 再压缩成 `.7z`

### 3. Craft 默认 Windows 安装器打包器

文件：

- `C:\CraftRoot\craft\bin\Packager\TypePackager.py`

默认 Windows 更偏向：

- `NullsoftInstallerPackager`

但这不是你给的官方参考目录对应的流程。

## 本机环境

- Craft 根目录：`C:\CraftRoot`
- Python：`C:\Users\tanglin\AppData\Local\Programs\Python\Python313\python.exe`
- 源码目录：`C:\code\kdevelop`
- ABI：`windows-cl-msvc2022-x86_64`

## 实际执行的打包命令

```bat
C:\Users\tanglin\AppData\Local\Programs\Python\Python313\python.exe C:\CraftRoot\craft-tmp\bin\craft.py ^
  --options kde/kdevelop.srcDir=C:\code\kdevelop ^
  --options kde/kdevelop.buildTests=False ^
  --options [Packager]PackageType=PortablePackager ^
  --package kde/kdevelop
```

## 实际产物

主产物：

- `C:\CraftRoot\tmp\kdevelop-master-windows-cl-msvc2022-x86_64.7z`

校验文件：

- `C:\CraftRoot\tmp\kdevelop-master-windows-cl-msvc2022-x86_64.7z.sha256`

清单文件：

- `C:\CraftRoot\tmp\manifest.json`
- `C:\CraftRoot\tmp\manifest-20260519T025708.json`

文件大小：

- `701129241` 字节，约 `701 MB`

## 同批生成的附加包

此次打包时，Craft 还顺带输出了这些包：

- `C:\CraftRoot\tmp\kdev-php-master-windows-cl-msvc2022-x86_64.7z`
- `C:\CraftRoot\tmp\kdev-python-master-windows-cl-msvc2022-x86_64.7z`
- `C:\CraftRoot\tmp\kdev-ruby-master-master-windows-cl-msvc2022-x86_64.7z`

## 产物目录特征

官方参考目录的顶层结构是：

- `bin`
- `etc`
- `lib`
- `libexec`
- `licenses`
- `share`

这正是 Craft 便携包解压后的典型布局。

## 当前结论

当前项目已经可以通过 Craft 的官方风格流程打出便携包：

- 类型：`.7z`
- 风格：与官方下载目录风格一致
- 流程：`Craft + PortablePackager`

## 如果要继续

下一步可选方向：

1. 将本次产物与官方参考目录做逐项差异对比
2. 研究如何生成官方 Windows 安装器 `.exe`
3. 固化成一份“一键打包脚本”
