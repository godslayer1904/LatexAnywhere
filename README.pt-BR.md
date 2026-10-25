# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · **Português (Brasil)**

Digite fórmulas LaTeX em **qualquer** campo de texto, do jeito do [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) para Obsidian.
Pressione `Alt+L`, digite com os snippets do Latex Suite e uma pré-visualização ao vivo, pressione `Enter` e a fórmula vai para onde estava o cursor.

- A caixa executa **o Latex Suite original** (1.12.8), não uma reimplementação: snippets, tabstops, frações automáticas e atalhos de matrizes funcionam exatamente como no Obsidian (conferido tecla a tecla com o original em 5.220 sessões gravadas).
- Usa automaticamente **as suas próprias configurações do Latex Suite** do Obsidian.
- Nativo e leve: sem motor de navegador, abre em poucos milissegundos.
- Windows (x64, ARM64) e Linux (x86_64, aarch64). Interface em 10 idiomas.

## Instalação

**Windows 10/11**: baixe `LatexAnywhere-Setup-*.exe` em [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) e execute. Um só instalador para x64 e ARM64, sem precisar de administrador.

**Linux** (GTK 4.12+, por exemplo Ubuntu 24.04+, Fedora 40+; testado no Ubuntu 26.04 / GNOME 50 Wayland):

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

Desinstalar: no Linux, `bash install.sh --uninstall`; no Windows, *Configurações → Aplicativos*.

## Uso

1. Em qualquer campo de texto (navegador, Word, chat, editor, terminal…) pressione `Alt+L`.
2. Digite a fórmula. `x^2` vira `x^{2}`, `//` cria uma fração e `Tab` pula para o próximo espaço. A fórmula renderizada aparece em cima.
3. Insira:

| Tecla | Insere | Bom para |
|---|---|---|
| `Enter` | a fórmula `x^{2}` | LaTeX, Overleaf |
| `Alt+Enter` | fórmula em linha `$x^{2}$` | Typora, Obsidian, Markdown |
| `Ctrl+Enter` | caracteres Unicode `x²` | chats sem fórmulas |
| `Ctrl+Shift+Enter` | equação nativa do Word (Windows) | Word, PowerPoint |
| `Shift+Enter` | nova linha na caixa | fórmulas de várias linhas |
| `Esc` ou `Alt+L` de novo | nada | cancelar |

Selecione um texto antes de pressionar `Alt+L` para editá-lo na caixa; ao inserir, ele substitui a seleção.
A caixa muda o método de entrada para inglês e depois o restaura. O conteúdo inserido também fica na área de transferência, para você colar de novo se algum app não o recebeu.

Ajuda: ícone da bandeja → *Ajuda*, ou `Alt+Shift+L` no Linux. Saída de emergência: `Win+Alt+L` (Windows) / `Super+Alt+L` (Linux).

## Configurações

O LatexAnywhere lê o `data.json` do Latex Suite dos seus cofres do Obsidian. Para usar outras configurações, coloque um `data.json` do Latex Suite em `%APPDATA%\LatexAnywhere\settings.json` (Windows) ou `~/.config/LatexAnywhere/settings.json` (Linux).

## Notas sobre o Linux

O Wayland não deixa programas lerem outras janelas nem digitarem nelas, então a versão para Linux funciona de outro jeito por dentro (atalho do GNOME, AT-SPI, teclado virtual, bandeja StatusNotifierItem). No Linux a caixa abre onde a área de trabalho a coloca, não ao lado do cursor. Detalhes: [docs/linux-design.md](docs/linux-design.md).

## Protótipo

[`prototype/`](prototype/) contém o primeiro protótipo rápido em Python (Windows, pywebview), a partir do qual esta versão nativa foi reescrita. Mantido como referência, sem manutenção.

## Compilar a partir do código-fonte

Requer CMake, Ninja, Node.js 22 e MSVC (Windows) ou GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev` (Linux). Os passos exatos estão em [.github/workflows/build.yml](.github/workflows/build.yml).

## Licença

MIT. Latex Suite (MIT), CodeMirror (MIT), MathJax (Apache 2.0), QuickJS-ng (MIT) e os demais componentes estão listados em [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
