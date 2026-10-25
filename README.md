# LatexAnywhere

Type LaTeX math in **any** text box, the Obsidian Latex Suite way.
Press `Alt+L`, type with Latex Suite snippets and a live preview, press `Enter` — the formula lands where your cursor was.

> Work in progress. Native C++ rewrite of a Python prototype. Windows first (x64 and ARM64), Linux next.

## Goals

- Every Latex Suite feature, behaving exactly like the original (checked by differential tests against Latex Suite 1.12.8).
- Never lose what you typed: failed insertion keeps the box open, the clipboard and input method are always restored.
- Fast and small: popup P99 ≤ 50 ms, idle memory ≤ 15 MB, no browser engine.

## License

MIT. Third-party components are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
