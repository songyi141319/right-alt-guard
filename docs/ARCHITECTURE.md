# Architecture / 工作原理

The public launcher embeds two workers and two hook DLLs as Win32 resources. The extracted files must exactly match the embedded bytes; a mismatched file is refused instead of executed or silently overwritten. Directory ancestors are checked and held without delete sharing while components are extracted, rejecting junctions and other reparse points before traversing them.

主程序把两个后台程序和两个菜单组件装进同一文件。释放时核对完整字节；已有文件不一致就停止，不擅自覆盖。目录逐层检查并在释放期间保持打开，阻止通过目录联接把组件写到其他位置。

The tray owns a kill-on-close job. Each worker is created suspended and atomically assigned to that job using the Windows 10 job-list process attribute, then resumed. Normal exit signals the workers' stop events and waits for release. If the controller crashes, Windows terminates both job members. A separate named control mutex serializes start/stop operations; another mutex limits the tray to one instance per session.

托盘主程序负责两个后台程序的生命周期。正常退出通知它们结束；异常退出由 Windows 自动结束其所属后台。启动和停止串行处理，重复双击不会多开。

If one worker exits unexpectedly, the two-second status timer stops the surviving worker. If cleanup cannot complete, the icon tooltip reports a partial fault and the menu offers to stop the remainder; it does not label that state as paused.

Each worker installs `WH_GETMESSAGE` and `WH_CBT` hooks for its own architecture and continuously pumps Windows messages. Per-target-thread state distinguishes a bare Right Alt tap from Left Alt, pre-held modifiers, intervening keys, changed windows, and stale releases. Native `SC_KEYMENU` requests are vetoed only after a qualifying Right Alt release.

For `Chrome_WidgetWin_1` root windows, a matched Right Alt down/repeat/up sequence begun without other held modifiers is converted to `WM_NULL` after downstream callbacks in the same hook chain see the original message. This prevents the software-menu Alt-release path. Windows modifier state is unchanged. Other root-window classes retain their original Alt notifications and use only the native menu-command veto.

This ordering does not preserve all input-method paths: the final destination window receives a null message. Right-Alt-first combinations in targeted windows can lose the initial Alt notification even when later key messages retain their context flags. Test coverage models specific menu behavior; it does not establish universal shortcut compatibility.

原消息观察链与最终窗口接收是不同的步骤：链上的后续观察者先看到原通知，目标窗口收到的是空消息。因此不能承诺所有输入法、AltGr 和组合键都保留。这一限制保留在中英文使用说明中。

The launcher does not include the temporary passive diagnostic logger used during local debugging. No user input or diagnostic log is shipped. The only browser action is the explicit GitHub menu command. Standard popup foreground ownership is requested only after the user clicks the tray, never from the keyboard hooks.

References / 依据:

- [Microsoft: SetWindowsHookEx and cross-bitness requirements](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw)
- [Microsoft: GetMsgProc message modification](https://learn.microsoft.com/en-us/windows/win32/winmsg/getmsgproc)
- [Microsoft: CBT system-command veto](https://learn.microsoft.com/en-us/windows/win32/winmsg/cbtproc)
- [Microsoft: notification area](https://learn.microsoft.com/en-us/windows/win32/shell/notification-area)
- [Electron: RootView Alt-release menu focus](https://github.com/electron/electron/blob/main/shell/browser/ui/views/root_view.cc)
