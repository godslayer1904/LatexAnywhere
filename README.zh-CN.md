# LatexAnywhere

[English](README.md) · **简体中文** · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

在**任何**输入框里，用 Obsidian [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) 的方式打公式。
按 `Alt+L`，用 Latex Suite 的片段边打边看实时预览，按 `Enter`，公式就插回光标原来的位置。

- 框里跑的是**原版 Latex Suite**（1.12.8），不是仿制：片段、跳位、自动分式、矩阵快捷键和 Obsidian 里完全一致（用 5220 组录制的输入与原版逐键对照）。
- 自动使用你在 Obsidian 里的 **Latex Suite 个人设置**。
- 原生程序、体积小：不带浏览器内核，弹框只要几毫秒。
- 支持 Windows（x64、ARM64）和 Linux（x86_64、aarch64），界面有 10 种语言。

## 安装

**Windows 10/11**：在 [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) 下载 `LatexAnywhere-Setup-*.exe` 运行即可。x64 和 ARM64 共用一个安装包，不需要管理员权限。

**Linux**（GTK 4.12 以上，如 Ubuntu 24.04+、Fedora 40+；已在 Ubuntu 26.04 / GNOME 50 Wayland 上测试）：

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

卸载：Linux 上运行 `bash install.sh --uninstall`；Windows 在「设置 → 应用」里卸载。

## 使用

1. 在任意输入框里（浏览器、Word、聊天软件、编辑器、终端……）按 `Alt+L`。
2. 打公式。`x^2` 自动变成 `x^{2}`，`//` 出分式，`Tab` 跳到下一个空位，框上方实时显示渲染结果。
3. 上屏：

| 按键 | 插入的内容 | 适合 |
|---|---|---|
| `Enter` | 公式本身 `x^{2}` | LaTeX、Overleaf |
| `Alt+Enter` | 带 $ 的行内公式 `$x^{2}$` | Typora、Obsidian、Markdown |
| `Ctrl+Enter` | Unicode 字符 `x²` | 微信等不显示公式的地方 |
| `Ctrl+Shift+Enter` | Word 原生公式（Windows） | Word、PowerPoint |
| `Shift+Enter` | 在框里换行 | 多行公式 |
| `Esc` 或再按 `Alt+L` | 什么都不插 | 取消 |

先选中一段文字再按 `Alt+L`，选中的内容会带进框里，改完上屏会替换原来的选区。
进框时输入法自动切成英文，离开后切回；剪贴板会恢复原样。

使用说明：托盘图标 →「使用说明」，Linux 上也可以按 `Alt+Shift+L`。紧急退出：`Win+Alt+L`（Windows）/ `Super+Alt+L`（Linux）。

## 设置

LatexAnywhere 会从你的 Obsidian 库里读取 Latex Suite 的 `data.json`。想用别的设置，把一份 Latex Suite 的 `data.json` 放到 `%APPDATA%\LatexAnywhere\settings.json`（Windows）或 `~/.config/LatexAnywhere/settings.json`（Linux）。

## Linux 说明

Wayland 不允许程序读取别的窗口、也不允许往别的窗口里打字，所以 Linux 版在底层的做法不同（GNOME 快捷键、AT-SPI、虚拟键盘、StatusNotifierItem 托盘）。Linux 上框的位置由桌面决定，不会弹在光标旁边。详见 [docs/linux-design.md](docs/linux-design.md)。

## 原型

[`prototype/`](prototype/) 是最初的 Python 快速原型（Windows，pywebview），本原生版就是照着它重写的。保留供参考，不再维护。

## 从源码构建

需要 CMake、Ninja、Node.js 22，以及 MSVC（Windows）或 GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev`（Linux）。具体步骤见 [.github/workflows/build.yml](.github/workflows/build.yml)。

## 许可

MIT。Latex Suite（MIT）、CodeMirror（MIT）、MathJax（Apache 2.0）、QuickJS-ng（MIT）等组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
