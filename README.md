# 右 Alt 守护 / RightAlt Guard

**让语音留在输入框，让右 Alt 专心做快捷键。**

**Keep voice input in your text field. Let Right Alt do its job.**

[中文介绍](#中文介绍) · [English](#english) · [下载 / Download](https://github.com/songyi141319/right-alt-guard/releases/latest)

## 中文介绍

你用右 Alt 启动语音输入时，软件的菜单有时也会被激活，把焦点从输入框抢走。语音已经开始，文字却无法进入原来的输入框。

**右 Alt 守护**是一个轻量的 Windows 小工具，尝试阻止这次菜单激活。它没有通过补发 Ctrl、其他按键或点击输入框来“修复”焦点，适合这些额外动作会打断语音的情况。

### 三步使用

1. 从[最新版下载页](https://github.com/songyi141319/right-alt-guard/releases/latest)下载 `RightAltGuard.exe`。
2. 放在喜欢的位置，双击。右下角托盘出现绿色 **A** 图标，保护已开启；图标可能在“显示隐藏的图标”里。
3. 回到原来的输入框，用右 Alt 开始和停止语音，检查菜单不再抢焦点、文字仍进入这个输入框。

右键或点击托盘图标，可以选择 **暂停保护 / Pause**、**恢复保护 / Resume**、**退出 / Exit**。暂停时图标变灰；退出会停止所有后台保护。重复双击不会多开。

一个下载文件，无需安装 AutoHotkey、无需运行脚本。第一次启动会自动把四个必需组件释放到 `%LOCALAPPDATA%\RightAltGuard\0.1.0`，然后在后台运行。不是完全不落地的程序；可移动你下载的主程序，运行时请保留组件目录。

不自动设置开机启动。如需开机使用，可以自行将主程序的快捷方式放入 Windows 启动文件夹。移除主程序快捷方式即可取消自动启动。

### 它做什么

- 尝试阻止单按右 Alt 打开 Windows 原生菜单。
- 对使用 `Chrome_WidgetWin_1` 窗口的 Chromium/Electron 软件增加处理，覆盖 Codex 一类自行处理 Alt 松开的菜单。
- 保留左 Alt；首次按右 Alt 前已经按住 Ctrl、Shift、左 Alt 或 Win 时，放行整组通知。
- 保留系统 Alt 按住状态及其他键的消息，不补发键盘输入，不自动点击或恢复焦点。
- 没有文本、语音或按键记录功能，没有联网请求和遥测。点击“开源主页”时才会打开浏览器。

### 已验证和限制

原保护机制已由用户在 Windows 上试用，并反馈豆包可以启动语音且菜单冲突似乎已消失。首个公开版本沿用相同保护源代码，新增单文件启动与托盘控制；打包、启停和异常清理已有自动检查。这个反馈不能代表所有电脑、软件和键盘布局都验证通过。

支持 Windows 10/11 **x64（Intel/AMD）**，同时处理普通 64 位和 32 位桌面程序。不是 32 位 Windows 或 ARM64 原生版本。普通权限不覆盖管理员程序或安全桌面。

在上述 Chromium 窗口中，右 Alt 的最终窗口通知会被隐藏。依赖这些通知的其他快捷键或输入法可能受影响，尤其是先按右 Alt 再按其他键、AltGr 及部分键盘布局。`Alt+Tab`、语音启停和文字落点请在自己的环境检查；不能保证所有组合键都兼容。发现影响时，从托盘暂停或退出。

程序使用常规 Windows 窗口消息观察组件，需要在匹配的桌面进程中加载 DLL。文件完整源代码公开，发布程序目前没有代码签名；如果安全软件阻止，请核对来源与文件指纹，不要为了运行它关闭防护。

### 卸载

从托盘选择退出，删除下载的主程序；可按需删除 `%LOCALAPPDATA%\RightAltGuard` 组件目录。没有注册服务、驱动、注册表按键映射或默认开机项。

## English

Pressing Right Alt to start voice input can also activate an application's menu and move focus away from your text field. Dictation starts, but the words no longer land where you intended.

**RightAlt Guard** is a small Windows utility that attempts to prevent that menu activation. It sends no extra Ctrl or other key, and performs no automatic focus click—useful when those actions would interrupt dictation.

### Get started

1. Download `RightAltGuard.exe` from the [latest release](https://github.com/songyi141319/right-alt-guard/releases/latest).
2. Save it anywhere and double-click. A green **A** appears in the notification area, possibly under hidden icons.
3. Return to your text field and use Right Alt to start and stop voice input. Verify that the menu stays out of the way and words enter the original field.

Click or right-click the icon for **Pause**, **Resume**, or **Exit**. The icon turns gray while paused. Exit stops both background workers. Launching twice does not create another tray controller.

One download; no installer, AutoHotkey, or startup script required. On first launch, four required components are extracted to `%LOCALAPPDATA%\RightAltGuard\0.1.0`. This is a single-file distribution, not an entirely in-memory application. You may move the downloaded launcher; retain its component folder while it runs.

Startup is manual by default. If desired, place a shortcut to the launcher in your Windows Startup folder; remove that shortcut to undo automatic startup.

### Behavior and compatibility

- Attempts to block native Windows menu activation following a bare Right Alt tap.
- Adds scoped handling for Chromium/Electron windows with class `Chrome_WidgetWin_1`, including software-managed Alt-release menus such as Codex.
- Preserves Left Alt and passes Right Alt pairs begun with Ctrl, Shift, Left Alt, or a Windows key already held.
- Leaves the operating system's Alt modifier state and other key messages unchanged. Generates no keyboard input and does not automatically refocus a window.
- Includes no text, audio, or keystroke recording, networking, or telemetry. The GitHub menu opens your browser only when selected.

The underlying protection was tried on one Windows setup with Doubao voice input, and the user reported that it appeared to resolve the menu conflict. This release reuses the same protection source and adds tested packaging and tray lifecycle handling. That report is not a guarantee for every application, keyboard layout, or input method.

Requires Windows 10/11 **x64 (Intel/AMD)**. Includes both 64-bit and 32-bit desktop components; this is not a 32-bit Windows or native ARM64 release. Normal privileges do not cover elevated applications or secure desktops.

In the targeted Chromium windows, final Right Alt window notifications are hidden. Other shortcuts or input methods depending on those notifications may be affected, especially Right-Alt-first combinations, AltGr, and some keyboard layouts. Verify voice start/stop, `Alt+Tab`, and text placement in your environment. Pause or exit if a shortcut is affected.

The utility uses ordinary Windows message hooks and loads its component DLLs into desktop processes. Source is available, but release binaries are currently unsigned. If security software blocks execution, check the source and published checksum; do not disable protection to run it.

To remove it, choose Exit, delete the downloaded launcher, and optionally delete `%LOCALAPPDATA%\RightAltGuard`. It installs no service, driver, registry key remapping, or default startup entry.

## 开发 / Development

Install Visual Studio C++ Build Tools with a Windows SDK. Run from Windows PowerShell 5.1 or newer:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The build checks both architectures and the single-file package, then produces `dist\RightAltGuard.exe` and `dist\SHA256SUMS.txt`. In a disposable Windows desktop session, `build.ps1 -DesktopTests` also exercises tray startup, pause/resume, normal exit, forced controller exit, and native menu callbacks. These desktop tests start and stop this utility's real hooks; they generate no physical keyboard input and send test menu messages only to their own windows.

Advanced commands:

```powershell
.\RightAltGuard.exe --status                       # 0 = both components and any active tray are ready
.\RightAltGuard.exe --stop                         # stop tray and protection
.\RightAltGuard.exe --extract-only "$env:TEMP\GuardPayload"  # inspect components without starting hooks
```

See [architecture](docs/ARCHITECTURE.md) and [verification](docs/VERIFICATION.md).

## 许可 / License

[MIT](LICENSE). Free to use, inspect, modify, and share. No affiliation with Microsoft, Doubao, or OpenAI.
