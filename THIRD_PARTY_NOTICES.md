# Third-party notices

| Component | License | Use |
| --- | --- | --- |
| [Obsidian Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) 1.12.8 | MIT, (c) 2022 artisticat1 | snippet behaviour reference and default snippets |
| [CodeMirror 6](https://codemirror.net/) (@codemirror/state, language, commands; key handling and decoration code adapted from @codemirror/view) | MIT, (c) Marijn Haverbeke and others | editor state engine |
| [Lezer](https://lezer.codemirror.net/) | MIT, (c) Marijn Haverbeke and others | parser runtime |
| [valibot](https://valibot.dev/) | MIT | snippet schema validation (used by Latex Suite) |
| [QuickJS-ng](https://github.com/quickjs-ng/quickjs) 0.17.0 | MIT, (c) Fabrice Bellard, Charlie Gordon and the QuickJS-ng contributors | embedded JavaScript engine |
| [MathJax](https://www.mathjax.org/) 3.2.2 (mathjax-full) | Apache-2.0, (c) The MathJax Consortium | live preview (SVG), MathML output |
| [unicodeit](https://github.com/svenkreiss/unicodeit) 0.7.5 | MIT, (c) Sven Kreiss | Ctrl+Enter Unicode conversion |
| [Inno Setup](https://jrsoftware.org/isinfo.php) and its translation files (installer/Languages) | Inno Setup License, (c) Jordan Russell and Martijn Laan | Windows installer |

## Python prototype (`prototype/`)

The prototype bundles the Latex Suite plugin (MIT, above), CodeMirror 6 and Lezer (MIT, above, in `www/ime.bundle.js`, built from `src/entry.js`) and MathJax 3 with its fonts (Apache-2.0, in `www/obsidian/lib/mathjax`).
It is run with [pywebview](https://pywebview.flowrl.com/) (BSD-3-Clause), [comtypes](https://github.com/enthought/comtypes) (MIT) and [psutil](https://github.com/giampaolo/psutil) (BSD-3-Clause), which are installed from PyPI and not included here.
It does not include any part of Obsidian itself; Obsidian's colours used by the formula box are re-declared in `www/ime.html`.
