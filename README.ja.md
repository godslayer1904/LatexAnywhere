# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · **日本語** · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

**どの**入力欄でも、Obsidian の [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) と同じ方法で数式を入力できます。
`Alt+L` を押し、Latex Suite のスニペットとリアルタイムプレビューで入力して `Enter` を押すと、数式が元のカーソル位置に挿入されます。

- ボックスの中で動くのは**本物の Latex Suite**（1.12.8）です。再実装ではないので、スニペット・タブストップ・自動分数・行列のショートカットは Obsidian とまったく同じに動きます（記録した 5,220 件の入力をオリジナルとキー単位で照合済み）。
- Obsidian の **Latex Suite の個人設定**を自動で使います。
- ネイティブで軽量：ブラウザエンジンを使わず、数ミリ秒で開きます。
- Windows（x64・ARM64）と Linux（x86_64・aarch64）に対応。画面は 10 言語。

## インストール

**Windows 10/11**：[Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) から `LatexAnywhere-Setup-*.exe` をダウンロードして実行します。x64 と ARM64 共通のインストーラーで、管理者権限は不要です。

**Linux**（GTK 4.12 以上。例：Ubuntu 24.04+、Fedora 40+。Ubuntu 26.04 / GNOME 50 Wayland で動作確認済み）：

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

アンインストール：Linux は `bash install.sh --uninstall`、Windows は「設定 → アプリ」から。

## 使い方

1. どの入力欄でも（ブラウザ、Word、チャット、エディタ、ターミナル…）`Alt+L` を押します。
2. 数式を入力します。`x^2` は自動で `x^{2}` に、`//` で分数、`Tab` で次の入力位置へ。上にレンダリング結果が表示されます。
3. 挿入：

| キー | 挿入される内容 | 向いている用途 |
|---|---|---|
| `Enter` | 数式そのもの `x^{2}` | LaTeX、Overleaf |
| `Alt+Enter` | $ 付きのインライン数式 `$x^{2}$` | Typora、Obsidian、Markdown |
| `Ctrl+Enter` | Unicode 文字 `x²` | 数式を表示できないチャット |
| `Ctrl+Shift+Enter` | Word のネイティブ数式（Windows） | Word、PowerPoint |
| `Shift+Enter` | ボックス内で改行 | 複数行の数式 |
| `Esc` またはもう一度 `Alt+L` | 何も挿入しない | キャンセル |

文字を選択してから `Alt+L` を押すと、選択部分がボックスに入り、挿入すると選択範囲が置き換わります。
ボックスを開くと入力方式は自動で英字に切り替わり、閉じると元に戻ります。挿入した内容はクリップボードにも残るので、うまく入らなかったときは自分でもう一度貼り付けられます。

使い方の表示：トレイアイコン →「使い方」、Linux では `Alt+Shift+L` でも開けます。緊急終了：`Win+Alt+L`（Windows）/ `Super+Alt+L`（Linux）。

## 設定

LatexAnywhere は Obsidian の保管庫から Latex Suite の `data.json` を読み込みます。別の設定を使うには、Latex Suite の `data.json` を `%APPDATA%\LatexAnywhere\settings.json`（Windows）または `~/.config/LatexAnywhere/settings.json`（Linux）に置いてください。

## Linux について

Wayland では他のウィンドウの読み取りや文字の送り込みが許可されないため、Linux 版は内部の仕組みが異なります（GNOME のショートカット、AT-SPI、仮想キーボード、StatusNotifierItem トレイ）。Linux ではボックスの位置はデスクトップが決め、カーソルの横には出ません。詳しくは [docs/linux-design.md](docs/linux-design.md) を参照してください。

## プロトタイプ

[`prototype/`](prototype/) には、このネイティブ版の元になった最初の Python 試作版（Windows、pywebview）があります。参考用に残しており、保守はしていません。

## ソースからのビルド

CMake、Ninja、Node.js 22 と、MSVC（Windows）または GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev`（Linux）が必要です。手順は [.github/workflows/build.yml](.github/workflows/build.yml) を参照してください。

## ライセンス

MIT。Latex Suite（MIT）、CodeMirror（MIT）、MathJax（Apache 2.0）、QuickJS-ng（MIT）などのコンポーネントは [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) に記載しています。
