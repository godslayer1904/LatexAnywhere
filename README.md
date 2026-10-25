# LatexAnywhere

**English** · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

Type LaTeX math in **any** text box, the Obsidian [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) way.
Press `Alt+L`, type with Latex Suite snippets and a live preview, press `Enter` — the formula lands where your cursor was.

- The box runs the **original Latex Suite** (1.12.8), not a re-implementation: snippets, tabstops, auto-fraction, matrix shortcuts behave exactly as in Obsidian (checked against Latex Suite on 5,220 recorded sessions).
- Uses **your own Latex Suite settings** from Obsidian automatically.
- Native and small: no browser engine, popup in a few milliseconds.
- Windows (x64, ARM64) and Linux (x86_64, aarch64). Interface in 10 languages.

## Install

**Windows 10/11** — download `LatexAnywhere-Setup-*.exe` from [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) and run it. One installer for x64 and ARM64, no administrator rights needed.

**Linux** (GTK 4.12+, e.g. Ubuntu 24.04+, Fedora 40+; tested on Ubuntu 26.04 / GNOME 50 Wayland):

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

Uninstall: `bash install.sh --uninstall` on Linux, or *Settings → Apps* on Windows.

## Use

1. In any text box (browser, Word, chat, editor, terminal…) press `Alt+L`.
2. Type the formula. `x^2` becomes `x^{2}`, `//` makes a fraction, `Tab` jumps to the next slot. The rendered formula is shown above.
3. Insert it:

| Key | Inserts | Good for |
|---|---|---|
| `Enter` | the formula `x^{2}` | LaTeX, Overleaf |
| `Alt+Enter` | inline math `$x^{2}$` | Typora, Obsidian, Markdown |
| `Ctrl+Enter` | Unicode `x²` | chat apps without math rendering |
| `Ctrl+Shift+Enter` | native Word equation (Windows) | Word, PowerPoint |
| `Shift+Enter` | a new line in the box | multi-line formulas |
| `Esc` or `Alt+L` again | nothing | cancel |

Select text before pressing `Alt+L` to edit it in the box; inserting replaces the selection.
The box switches your input method to English and switches it back afterwards. Your clipboard is restored.

Help window: tray icon → *Help*, or `Alt+Shift+L` on Linux. Emergency quit: `Win+Alt+L` (Windows) / `Super+Alt+L` (Linux).

## Settings

LatexAnywhere reads Latex Suite's `data.json` from your Obsidian vaults. To use other settings, put a Latex Suite `data.json` at `%APPDATA%\LatexAnywhere\settings.json` (Windows) or `~/.config/LatexAnywhere/settings.json` (Linux).

## Linux notes

Wayland does not let programs read other windows or type into them, so the Linux version works differently under the hood (GNOME shortcut, AT-SPI, a virtual keyboard, StatusNotifierItem tray). On Linux the box opens where the desktop places it, not next to the cursor. Details: [docs/linux-design.md](docs/linux-design.md).

## Prototype

[`prototype/`](prototype/) holds the original Python quick prototype (Windows, pywebview) that this native version was rebuilt from. It is kept for reference and is not maintained.

## Build from source

Needs CMake, Ninja, Node.js 22, and MSVC (Windows) or GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev` (Linux). See [.github/workflows/build.yml](.github/workflows/build.yml) for the exact steps.

## License

MIT. Latex Suite (MIT), CodeMirror (MIT), MathJax (Apache 2.0), QuickJS-ng (MIT) and the other components are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
