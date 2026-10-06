# v0.1.0 · 右 Alt 守护 / RightAlt Guard

让语音留在输入框，让右 Alt 专心做快捷键。

首个公开版本：一个文件，双击开启；右下角绿色 A 图标提供暂停、恢复和退出。退出或主程序异常结束时，后台也会停止。没有默认开机项、没有额外按键、没有文本或录音采集。

**下载 `RightAltGuard.exe` 即可。** `SHA256SUMS.txt` 用于核对文件。首次运行会自动释放所需组件到个人应用目录。

支持 Windows 10/11 x64（Intel/AMD），包含两种桌面程序位数的保护。普通权限不覆盖管理员程序。部分输入法、AltGr 或先右 Alt 后其他键的组合可能受影响；请实测语音启停和原输入框落字，受影响时从托盘暂停或退出。

原保护由用户在豆包语音场景试用并反馈似乎有效；本版本保持其源代码，完成了打包、菜单模型、托盘启停、异常清理和路径保护检查。未承诺所有软件兼容。

---

Keep voice input in your text field. Let Right Alt do its job.

First public release: one file, double-click to start. The green A tray icon offers Pause, Resume, and Exit. Background workers stop on normal exit or controller termination. No automatic startup, generated keystrokes, text collection, or audio recording.

**Download `RightAltGuard.exe`.** Use `SHA256SUMS.txt` to check the file. Required components are extracted to your local application data directory on first launch.

Requires Windows 10/11 x64 (Intel/AMD); includes workers for both desktop architectures. Normal privileges do not cover elevated applications. Some input methods, AltGr, or Right-Alt-first combinations may be affected. Verify voice start/stop and text placement; pause or exit if needed.

The underlying protection was reported apparently effective in one Doubao voice setup. This release preserves that source and verifies packaging, menu models, tray lifecycle, crash cleanup, and extraction paths. It is not a universal compatibility guarantee.

MIT License · Source available · Unsigned Windows binary
