# LatexAnywhere — Python quick prototype

[中文说明在下面](#中文)

This is the original quick prototype (Windows only) that the native C++ version in this repository was rebuilt from. It shows the idea with very little code: a small borderless window hosts the **original Latex Suite plugin** in a WebView2 page, `Alt+L` opens it next to the caret, and the result is pasted back into the previous text box.

It is kept for reference and is **not maintained**. For everyday use, install the native version from [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest).

## Run

Needs Windows 10/11 with the WebView2 runtime (included in Windows 11) and Python 3.10+.

```bat
pip install -r requirements.txt
run.cmd
```

`install.ps1` adds a logon task and Start-menu/desktop shortcuts. `selftest.py` opens its own test window, presses `Alt+L`, types `x^2` and reads the result back.

## Files

| File | What it is |
|---|---|
| `formula_box.py` | the whole program: hotkey, caret position, selection, the WebView2 window, pasting, input-method switching, tray, watchdog |
| `boot.pyw` | logon launcher with retries |
| `www/ime.html` | the editor page (Latex Suite + CodeMirror + MathJax preview) |
| `www/ime.bundle.js` | CodeMirror 6 bundle, built from `src/entry.js` with `npx esbuild src/entry.js --bundle --format=iife --outfile=www/ime.bundle.js` |
| `www/plugin/` | the Latex Suite plugin (MIT). Put your own `data.json` here to use your settings |
| `help.html` | the old help page |

## Known limitations

These are why the native version exists: WebView2 makes the first popup slow and memory heavy, and with some Chinese input methods the switch to English inside the box is not reliable when keys are typed very quickly after `Alt+L`.

---

## 中文

这是最初的 Python 快速原型（仅 Windows），本仓库的原生 C++ 版就是照着它重写的。代码很少：一个无边框小窗口里用 WebView2 页面跑**原版 Latex Suite 插件**，`Alt+L` 在光标旁弹出，结果粘回原来的输入框。

原型保留供参考，**不再维护**。日常使用请到 [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) 安装原生版。

运行：需要 Windows 10/11（带 WebView2，Windows 11 自带）和 Python 3.10+，然后 `pip install -r requirements.txt`，再运行 `run.cmd`。`install.ps1` 会添加登录自启任务和开始菜单、桌面快捷方式；`selftest.py` 会开一个自己的测试窗口，按 `Alt+L` 打 `x^2` 并读回结果。

已知局限（也是做原生版的原因）：WebView2 让首次弹框慢、内存占用大；某些中文输入法下，`Alt+L` 后立刻打字时框内切英文不可靠。想用自己的 Latex Suite 设置，把 `data.json` 放进 `www/plugin/`。
