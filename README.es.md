# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · **Español** · [Português (Brasil)](README.pt-BR.md)

Escribe fórmulas LaTeX en **cualquier** campo de texto, al estilo de [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) para Obsidian.
Pulsa `Alt+L`, escribe con los snippets de Latex Suite y una vista previa en vivo, pulsa `Enter` y la fórmula aparece donde estaba el cursor.

- El cuadro ejecuta **el Latex Suite original** (1.12.8), no una reimplementación: snippets, tabstops, fracciones automáticas y atajos de matrices funcionan exactamente como en Obsidian (comprobado tecla a tecla contra el original en 5.220 sesiones grabadas).
- Usa automáticamente **tu propia configuración de Latex Suite** de Obsidian.
- Nativo y ligero: sin motor de navegador, se abre en pocos milisegundos.
- Windows (x64, ARM64) y Linux (x86_64, aarch64). Interfaz en 10 idiomas.

## Instalación

**Windows 10/11**: descarga `LatexAnywhere-Setup-*.exe` desde [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) y ejecútalo. Un único instalador para x64 y ARM64, sin permisos de administrador.

**Linux** (GTK 4.12+, por ejemplo Ubuntu 24.04+, Fedora 40+; probado en Ubuntu 26.04 / GNOME 50 Wayland):

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

Desinstalar: en Linux, `bash install.sh --uninstall`; en Windows, *Configuración → Aplicaciones*.

## Uso

1. En cualquier campo de texto (navegador, Word, chat, editor, terminal…) pulsa `Alt+L`.
2. Escribe la fórmula. `x^2` se convierte en `x^{2}`, `//` crea una fracción y `Tab` salta al siguiente hueco. Arriba se muestra la fórmula renderizada.
3. Insértala:

| Tecla | Inserta | Ideal para |
|---|---|---|
| `Enter` | la fórmula `x^{2}` | LaTeX, Overleaf |
| `Alt+Enter` | fórmula en línea `$x^{2}$` | Typora, Obsidian, Markdown |
| `Ctrl+Enter` | caracteres Unicode `x²` | chats sin fórmulas |
| `Ctrl+Shift+Enter` | ecuación nativa de Word (Windows) | Word, PowerPoint |
| `Shift+Enter` | una línea nueva en el cuadro | fórmulas de varias líneas |
| `Esc` o `Alt+L` de nuevo | nada | cancelar |

Selecciona texto antes de pulsar `Alt+L` para editarlo en el cuadro; al insertar, reemplaza la selección.
El cuadro cambia el método de entrada a inglés y luego lo restaura. Lo insertado también queda en el portapapeles, así que puedes volver a pegarlo si una aplicación no lo recibió.

Ayuda: icono de la bandeja → *Ayuda*, o `Alt+Shift+L` en Linux. Salida de emergencia: `Win+Alt+L` (Windows) / `Super+Alt+L` (Linux).

## Configuración

LatexAnywhere lee el `data.json` de Latex Suite de tus bóvedas de Obsidian. Para usar otra configuración, pon un `data.json` de Latex Suite en `%APPDATA%\LatexAnywhere\settings.json` (Windows) o `~/.config/LatexAnywhere/settings.json` (Linux).

## Notas sobre Linux

Wayland no permite que los programas lean otras ventanas ni escriban en ellas, así que la versión de Linux funciona de otra forma por dentro (atajo de GNOME, AT-SPI, teclado virtual, bandeja StatusNotifierItem). En Linux el cuadro se abre donde lo coloca el escritorio, no junto al cursor. Detalles: [docs/linux-design.md](docs/linux-design.md).

## Prototipo

[`prototype/`](prototype/) contiene el primer prototipo rápido en Python (Windows, pywebview) a partir del cual se reescribió esta versión nativa. Se conserva como referencia y no se mantiene.

## Compilar desde el código fuente

Se necesitan CMake, Ninja, Node.js 22 y MSVC (Windows) o GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev` (Linux). Los pasos exactos están en [.github/workflows/build.yml](.github/workflows/build.yml).

## Licencia

MIT. Latex Suite (MIT), CodeMirror (MIT), MathJax (Apache 2.0), QuickJS-ng (MIT) y el resto de componentes figuran en [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
