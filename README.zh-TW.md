# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · **繁體中文** · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

在**任何**輸入框裡，用 Obsidian [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) 的方式打公式。
按 `Alt+L`，用 Latex Suite 的片段邊打邊看即時預覽，按 `Enter`，公式就插回游標原本的位置。

- 框裡執行的是**原版 Latex Suite**（1.12.8），不是仿製：片段、跳位、自動分式、矩陣快捷鍵與 Obsidian 完全一致（以 5220 組錄製的輸入與原版逐鍵比對）。
- 自動使用你在 Obsidian 裡的 **Latex Suite 個人設定**。
- 原生程式、體積小：不帶瀏覽器核心，彈出只要幾毫秒。
- 支援 Windows（x64、ARM64）與 Linux（x86_64、aarch64），介面有 10 種語言。

## 安裝

**Windows 10/11**：在 [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) 下載 `LatexAnywhere-Setup-*.exe` 執行即可。x64 與 ARM64 共用一個安裝程式，不需要系統管理員權限。

**Linux**（GTK 4.12 以上，如 Ubuntu 24.04+、Fedora 40+；已在 Ubuntu 26.04 / GNOME 50 Wayland 上測試）：

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

解除安裝：Linux 執行 `bash install.sh --uninstall`；Windows 在「設定 → 應用程式」中解除安裝。

## 使用

1. 在任何輸入框裡（瀏覽器、Word、聊天軟體、編輯器、終端機……）按 `Alt+L`。
2. 打公式。`x^2` 自動變成 `x^{2}`，`//` 產生分式，`Tab` 跳到下一個空位，框上方即時顯示渲染結果。
3. 輸出：

| 按鍵 | 插入的內容 | 適合 |
|---|---|---|
| `Enter` | 公式本身 `x^{2}` | LaTeX、Overleaf |
| `Alt+Enter` | 帶 $ 的行內公式 `$x^{2}$` | Typora、Obsidian、Markdown |
| `Ctrl+Enter` | Unicode 字元 `x²` | 不顯示公式的聊天軟體 |
| `Ctrl+Shift+Enter` | Word 原生方程式（Windows） | Word、PowerPoint |
| `Shift+Enter` | 在框裡換行 | 多行公式 |
| `Esc` 或再按 `Alt+L` | 什麼都不插入 | 取消 |

先選取一段文字再按 `Alt+L`，選取的內容會帶進框裡，改完輸出會取代原本的選取範圍。
進框時輸入法自動切成英文，離開後切回；剪貼簿會恢復原狀。

使用說明：系統匣圖示 →「使用說明」，Linux 上也可以按 `Alt+Shift+L`。緊急結束：`Win+Alt+L`（Windows）/ `Super+Alt+L`（Linux）。

## 設定

LatexAnywhere 會從你的 Obsidian 儲存庫讀取 Latex Suite 的 `data.json`。若要使用其他設定，把一份 Latex Suite 的 `data.json` 放到 `%APPDATA%\LatexAnywhere\settings.json`（Windows）或 `~/.config/LatexAnywhere/settings.json`（Linux）。

## Linux 說明

Wayland 不允許程式讀取其他視窗、也不允許往其他視窗輸入文字，所以 Linux 版底層的做法不同（GNOME 快捷鍵、AT-SPI、虛擬鍵盤、StatusNotifierItem 系統匣）。Linux 上框的位置由桌面決定，不會出現在游標旁邊。詳見 [docs/linux-design.md](docs/linux-design.md)。

## 原型

[`prototype/`](prototype/) 是最初的 Python 快速原型（Windows，pywebview），本原生版就是照著它重寫的。保留供參考，不再維護。

## 從原始碼建置

需要 CMake、Ninja、Node.js 22，以及 MSVC（Windows）或 GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev`（Linux）。具體步驟見 [.github/workflows/build.yml](.github/workflows/build.yml)。

## 授權

MIT。Latex Suite（MIT）、CodeMirror（MIT）、MathJax（Apache 2.0）、QuickJS-ng（MIT）等元件見 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
