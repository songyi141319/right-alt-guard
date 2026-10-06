# Verification / 验证边界

Release: **0.1.0**. Local desktop verification: **2026-10-05** (user's America/Los_Angeles date).

## Protection / 菜单保护

The hook/worker sources were copied unchanged from the locally tested protection. A user reported that the earlier version appeared effective with Doubao voice input. Packaging checks do not replace that voice test and do not establish compatibility on other computers.

原菜单组件和后台源代码保持不变。用户反馈原保护似乎已经奏效；单文件版本新增包装与托盘控制，不能把自动测试称为所有输入法或软件的验证。

- x64 and x86 policy checks: **20 each**, including native menu veto, Left Alt, combinations, changed windows, repeated keys, and stale state.
- x64 and x86 software-menu fixtures: **22 each**, including focus/highlight suppression, other-window preservation, pre-held modifiers, thread modifier state, and original messages in the same hook chain.
- Real desktop native menu fixtures: right Alt veto, left Alt pass, and no change to the user's foreground window. Only private hidden test windows receive fixture commands.

## Package and lifecycle / 包装与生命周期

- A lone downloaded executable extracts all four required files with the correct machine architecture.
- Reusing an identical extracted component does not rewrite it.
- A mismatched component is refused and left unchanged.
- Final and ancestor junctions are refused; no directory or component is created in the junction target.
- Tray startup, duplicate launch, Pause, Resume, normal Exit, single-worker failure/recovery, and forced controller termination are exercised on the actual Windows desktop.
- Both worker process handles are checked after Pause, Exit, and controller termination, rather than inferring cleanup only from the combined status flag.
- Build and package scripts run under Windows PowerShell 5.1.

These tests generate no physical keyboard events. Tray tests control this utility's own hidden window; menu fixtures send messages only to their own windows. Desktop tests briefly start/stop the actual protection, so use a disposable desktop session for development.

这些检查不生成真实键盘输入、不向用户输入框发送消息。它们会启停本工具的实际后台；开发测试建议放在专用桌面会话。

## Counterproof / 反例

A temporary launcher with kill-on-close disabled reproduces the orphan-worker failure in the forced-exit test. A temporary package with directory reparse rejection disabled fails the ancestor-junction test. Formal sources remain unchanged during these probes; the normal build is then tested again.

Before adding single-worker reconciliation, terminating one worker left the other alive and the new fixture failed. The controller now stops any surviving worker on its status timer; a still-unresolved partial state is labelled "Needs attention" and offers a stop action rather than claiming to be paused.

临时取消异常退出清理，检查会发现后台残留；临时取消目录联接拒绝，检查会发现组件写入联接目标。正式代码不被改成这些反例，随后重新检查正式版本。

The first tray probe also exposed a test-harness issue: PowerShell converts a null string argument to an empty string, which caused a window-title search to miss the hidden controller. The corrected helper passes a real null from C#; this was a test detection issue, not evidence that the protection itself had failed.

## Public contents / 公开内容

Only source, build scripts, tests, icon assets, license, and documentation are included in Git. Build output and test fixtures are ignored. The release contains the launcher and its SHA256 checksum. Temporary diagnostics, recorded focus metadata, local absolute paths, credentials, and unrelated workspace files are not included.

公开仓库不包含本地诊断记录、输入内容、个人绝对路径或凭据。发布程序包含正常工作所需的后台组件；目前未进行代码签名。
