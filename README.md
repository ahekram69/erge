# USB Camera Control

Windows 优先、macOS 次优先的 USB 摄像头预览与参数控制软件。

每次推送代码后，GitHub Actions 会自动生成带版本号和构建编号的 Windows
安装包、Windows 免安装包和 macOS DMG 测试包。
软件窗口标题和 EXE 文件名也会显示版本号，便于区分新旧版本。

## 当前原型

- 自动发现和热插拔刷新摄像头
- 选择摄像头
- 实时视频预览
- 选择设备提供的分辨率和最大帧率
- 曝光补偿与变焦（仅在设备和系统后端支持时启用）
- Windows/macOS 共用 Qt 6 代码

## Windows 构建

1. 安装 Visual Studio 2022，并勾选“使用 C++ 的桌面开发”。
2. 安装 Qt 6.5 或更新的 Qt 6 MSVC 64-bit 套件。
3. 用 Qt Creator 打开项目根目录的 `CMakeLists.txt`。
4. 选择 Desktop Qt 6 MSVC 64-bit Kit，构建并运行。

命令行构建示例：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
cmake --build build
```

## macOS 测试版

在仓库的 **Actions → macOS Build** 页面下载 DMG。当前测试版采用临时签名，
首次打开时如果 macOS 阻止运行，请在“系统设置 → 隐私与安全性”中选择“仍要打开”。
正式对外发布前需要使用 Apple Developer ID 完成签名与公证。

## 计划

1. Windows 原生 UVC 控制层：亮度、对比度、饱和度、锐度、曝光、白平衡、对焦、变焦、增益和背光补偿。
2. 参数范围自动探测、自动/手动模式、恢复默认值和预设保存。
3. 截图、录像、镜像、翻转、旋转及裁剪。
4. Windows 虚拟摄像头输出。
5. macOS UVC 控制和 CoreMediaIO Camera Extension。

## 说明

不同摄像头支持的参数、范围和步长不同。界面应根据设备查询结果动态启用控制项，而不是假定所有设备都支持同一套参数。
