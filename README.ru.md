# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · **Русский** · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

Набирайте формулы LaTeX в **любом** поле ввода так же, как в [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) для Obsidian.
Нажмите `Alt+L`, наберите формулу со сниппетами Latex Suite и живым предпросмотром, нажмите `Enter` — формула вставится туда, где был курсор.

- В окне работает **оригинальный Latex Suite** (1.12.8), а не его переделка: сниппеты, переходы по Tab, авто-дроби и сокращения для матриц ведут себя точно как в Obsidian (сверено с оригиналом по каждой клавише на 5220 записанных сеансах).
- Автоматически использует **ваши собственные настройки Latex Suite** из Obsidian.
- Нативная и лёгкая программа: без браузерного движка, окно открывается за несколько миллисекунд.
- Windows (x64, ARM64) и Linux (x86_64, aarch64). Интерфейс на 10 языках.

## Установка

**Windows 10/11** — скачайте `LatexAnywhere-Setup-*.exe` в [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) и запустите. Один установщик для x64 и ARM64, права администратора не нужны.

**Linux** (GTK 4.12+, например Ubuntu 24.04+, Fedora 40+; проверено на Ubuntu 26.04 / GNOME 50 Wayland):

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

Удаление: в Linux — `bash install.sh --uninstall`, в Windows — *Параметры → Приложения*.

## Использование

1. В любом поле ввода (браузер, Word, мессенджер, редактор, терминал…) нажмите `Alt+L`.
2. Наберите формулу. `x^2` превращается в `x^{2}`, `//` создаёт дробь, `Tab` переходит к следующему месту. Сверху показан результат.
3. Вставка:

| Клавиши | Что вставляется | Для чего |
|---|---|---|
| `Enter` | сама формула `x^{2}` | LaTeX, Overleaf |
| `Alt+Enter` | строчная формула `$x^{2}$` | Typora, Obsidian, Markdown |
| `Ctrl+Enter` | символы Unicode `x²` | мессенджеры без формул |
| `Ctrl+Shift+Enter` | формула Word (Windows) | Word, PowerPoint |
| `Shift+Enter` | новая строка в окне | многострочные формулы |
| `Esc` или снова `Alt+L` | ничего | отмена |

Выделите текст перед `Alt+L`, чтобы отредактировать его в окне; при вставке он заменит выделение.
Окно само переключает раскладку на английскую и затем возвращает прежнюю. Вставленный текст остаётся и в буфере обмена, так что его можно вставить ещё раз вручную.

Справка: значок в трее → *Справка*, в Linux также `Alt+Shift+L`. Аварийный выход: `Win+Alt+L` (Windows) / `Super+Alt+L` (Linux).

## Настройки

LatexAnywhere читает `data.json` Latex Suite из ваших хранилищ Obsidian. Чтобы использовать другие настройки, положите `data.json` от Latex Suite в `%APPDATA%\LatexAnywhere\settings.json` (Windows) или `~/.config/LatexAnywhere/settings.json` (Linux).

## О Linux

Wayland не позволяет программам читать чужие окна и печатать в них, поэтому версия для Linux устроена внутри иначе (ярлык GNOME, AT-SPI, виртуальная клавиатура, трей StatusNotifierItem). В Linux положение окна выбирает рабочий стол, а не позиция курсора. Подробности: [docs/linux-design.md](docs/linux-design.md).

## Прототип

В [`prototype/`](prototype/) лежит первый быстрый прототип на Python (Windows, pywebview), по которому написана эта нативная версия. Сохранён для справки, не поддерживается.

## Сборка из исходников

Нужны CMake, Ninja, Node.js 22 и MSVC (Windows) или GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev` (Linux). Точные шаги — в [.github/workflows/build.yml](.github/workflows/build.yml).

## Лицензия

MIT. Latex Suite (MIT), CodeMirror (MIT), MathJax (Apache 2.0), QuickJS-ng (MIT) и другие компоненты перечислены в [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
