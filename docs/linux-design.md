# Linux 版的做法（Wayland）

Windows 版能做的事，Wayland 大多禁止程序直接做（读别的窗口的选区、往别的窗口发按键、全局热键、指定窗口位置）。下面是每一项的替代办法和理由。

| 功能 | Windows | Linux | 理由 |
|---|---|---|---|
| Alt+L 唤起 | RegisterHotKey | GNOME 自定义快捷键调用 `latexanywhere --toggle`，经 D-Bus 交给正在运行的实例 | GlobalShortcuts 门户在 GNOME 48+ 会反复弹设置窗；自定义快捷键稳定、无弹窗，并且会带激活令牌，框能拿到焦点 |
| 取选区 | UI Automation，读不到再 Ctrl+C | 焦点程序经 AT-SPI 可见（GTK、终端等）就以它为准，它说没选中就是没选中；浏览器等默认不暴露的程序改读主选区（PRIMARY），与上次带进来的相同就当旧内容不用 | Chrome、Firefox 取消选中后主选区也不清空（实测）；带进来的文字在框里是选中状态，万一是旧的第一下按键就覆盖。不注入 Ctrl+C：终端里是中断进程 |
| 上屏 | 剪贴板 + Ctrl+V | 剪贴板 + 虚拟键盘发 Shift+Insert；目标是终端（AT-SPI 角色）时发 Ctrl+Shift+Insert（/dev/uinput；没有权限时用 GNOME 的 Mutter 远程桌面接口） | 与键盘布局无关。终端（VTE）的 Shift+Insert 粘贴的是主选区，Ctrl+Shift+Insert 才是剪贴板 |
| 剪贴板恢复原样 | 粘贴后写回快照 | 一直由我们持有剪贴板：粘贴完成前给公式，之后给弹框前快照的原内容；文字走 GTK 自己的字符串序列化 | Wayland 失去焦点后不能再改剪贴板，GNOME 也没有 data-control 协议。自己写字节流的话 Chrome 不来读（实测）。Chrome 会缓存读到的内容，在 Chrome 里粘贴仍是公式 |
| 不碰 PRIMARY | — | 只写剪贴板 | GTK 程序失去 PRIMARY 会取消自己的选中，「选中后替换」会变成插入（实测） |
| 进框切英文、离开切回 | IMM 只改公式框自己的 | fcitx5：控制接口开关；IBus（GNOME 默认）：记下全局引擎，切到 `xkb:us::eng`，离开时切回；中途崩溃重启后自动切回 | IBus 在 GNOME 下没有读写「中/英」状态的公开接口，切引擎是编辑器自动切换插件的通行做法，也是 GNOME 切到拉丁键盘时自己的做法 |
| 托盘 | Shell_NotifyIcon | 自己实现 StatusNotifierItem + com.canonical.dbusmenu | libayatana-appindicator-glib 导出 org.gtk.Menus，Ubuntu 顶栏扩展不认 |
| 框的位置 | 光标旁 | 由桌面决定 | Wayland 不允许程序自己定窗口位置 |
| 守护与自我监控 | 守护进程 + 内存/CPU 监控 | 同左（fork + waitpid，/proc 读内存） | — |

## 测试

两层，都在独立的会话总线、独立的运行时目录（`XDG_RUNTIME_DIR`）里起无头 GNOME Shell，不碰真实桌面（共用 `/run/user/<uid>` 时测试的无障碍总线退出会删掉真实会话的连接文件，踩过）。

- `tests/e2e-linux/apps_run.sh build/latexanywhere`：**真实软件验收**。逐个打开 GNOME 文本编辑器、gedit、Ptyxis、GNOME 终端、Chrome、Firefox，测三种上屏、选中后替换、没选中时不带入旧内容，以及 Chrome 在智能拼音开着时自动切英文。
- `tests/e2e-linux/run.sh build/latexanywhere`：功能回归（下面这段）。
- `tests/e2e-linux/help_shots.sh build/latexanywhere`：10 种语言的说明窗口截图。


`tests/e2e-linux/run.sh build/latexanywhere`：在独立的会话总线里起无头 GNOME Shell（不碰真实桌面），目标程序是一个 GTK4 输入框，驱动脚本通过 Mutter 远程桌面接口模拟真人按键，托盘用测试自己扮演的 StatusNotifierWatcher 经 dbusmenu 真实点击。覆盖欢迎窗口、各种上屏方式、选区替换、托盘与语言切换、智能拼音自动切换、剪贴板恢复、崩溃自动重启、300 次浸泡。
