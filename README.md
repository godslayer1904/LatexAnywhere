# LatexAnywhere

Type LaTeX math in **any** text box, the Obsidian Latex Suite way.
Press `Alt+L`, type with Latex Suite snippets and a live preview, press `Enter` — the formula lands where your cursor was.

> Work in progress. Native C++ rewrite of a Python prototype. Windows (x64, ARM64) and Linux (x86_64, aarch64).

## Install

**Windows** — download `LatexAnywhere-Setup-*.exe` from [Releases](https://github.com/godslayer1904/LatexAnywhere/releases). One installer for x64 and ARM64.

**Linux** (GTK4 desktop; tested on Ubuntu 26.04 / GNOME 50, Wayland) — one line, picks x86_64 or aarch64 by itself:

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

Uninstall: `bash install.sh --uninstall`. How the Linux version works around Wayland's limits is in [docs/linux-design.md](docs/linux-design.md).

## Goals

- Every Latex Suite feature, behaving exactly like the original (checked by differential tests against Latex Suite 1.12.8).
- Never lose what you typed: failed insertion keeps the box open, the clipboard and input method are always restored.
- Fast and small: popup P99 ≤ 50 ms, idle memory ≤ 15 MB, no browser engine.

## License

MIT. Third-party components are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
