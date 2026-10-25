# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · **한국어** · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

**어느** 입력란에서든 Obsidian [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) 방식으로 수식을 입력합니다.
`Alt+L`을 누르고 Latex Suite 스니펫과 실시간 미리보기로 입력한 뒤 `Enter`를 누르면 수식이 원래 커서 위치에 들어갑니다.

- 상자 안에서 실행되는 것은 **원본 Latex Suite**(1.12.8)입니다. 재구현이 아니므로 스니펫, 탭 이동, 자동 분수, 행렬 단축키가 Obsidian과 똑같이 동작합니다(기록한 입력 5,220건을 원본과 키 단위로 대조).
- Obsidian의 **Latex Suite 개인 설정**을 자동으로 사용합니다.
- 네이티브이고 가볍습니다: 브라우저 엔진이 없고 몇 밀리초 만에 열립니다.
- Windows(x64, ARM64)와 Linux(x86_64, aarch64)를 지원하며 화면은 10개 언어입니다.

## 설치

**Windows 10/11**: [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest)에서 `LatexAnywhere-Setup-*.exe`를 내려받아 실행합니다. x64와 ARM64 공용 설치 파일이며 관리자 권한이 필요 없습니다.

**Linux**(GTK 4.12 이상, 예: Ubuntu 24.04+, Fedora 40+. Ubuntu 26.04 / GNOME 50 Wayland에서 테스트함):

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

제거: Linux는 `bash install.sh --uninstall`, Windows는 「설정 → 앱」에서 제거합니다.

## 사용법

1. 어느 입력란에서든(브라우저, Word, 메신저, 편집기, 터미널…) `Alt+L`을 누릅니다.
2. 수식을 입력합니다. `x^2`는 자동으로 `x^{2}`가 되고, `//`는 분수, `Tab`은 다음 칸으로 이동합니다. 위쪽에 렌더링 결과가 실시간으로 표시됩니다.
3. 입력:

| 키 | 들어가는 내용 | 용도 |
|---|---|---|
| `Enter` | 수식 자체 `x^{2}` | LaTeX, Overleaf |
| `Alt+Enter` | $가 붙은 인라인 수식 `$x^{2}$` | Typora, Obsidian, Markdown |
| `Ctrl+Enter` | 유니코드 문자 `x²` | 수식을 표시하지 못하는 메신저 |
| `Ctrl+Shift+Enter` | Word 기본 수식(Windows) | Word, PowerPoint |
| `Shift+Enter` | 상자 안에서 줄 바꿈 | 여러 줄 수식 |
| `Esc` 또는 `Alt+L` 다시 누르기 | 아무것도 넣지 않음 | 취소 |

텍스트를 선택한 뒤 `Alt+L`을 누르면 선택한 내용이 상자에 들어가고, 입력하면 원래 선택 영역을 대체합니다.
상자를 열면 입력기가 자동으로 영문으로 바뀌고 닫으면 원래대로 돌아갑니다. 클립보드도 원래대로 복원됩니다.

도움말: 트레이 아이콘 → 「사용법」, Linux에서는 `Alt+Shift+L`로도 열 수 있습니다. 긴급 종료: `Win+Alt+L`(Windows) / `Super+Alt+L`(Linux).

## 설정

LatexAnywhere는 Obsidian 보관소에서 Latex Suite의 `data.json`을 읽습니다. 다른 설정을 쓰려면 Latex Suite의 `data.json`을 `%APPDATA%\LatexAnywhere\settings.json`(Windows) 또는 `~/.config/LatexAnywhere/settings.json`(Linux)에 두세요.

## Linux 참고

Wayland는 프로그램이 다른 창을 읽거나 다른 창에 입력하는 것을 허용하지 않으므로 Linux 버전은 내부 방식이 다릅니다(GNOME 단축키, AT-SPI, 가상 키보드, StatusNotifierItem 트레이). Linux에서는 상자 위치를 데스크톱이 정하며 커서 옆에 나타나지 않습니다. 자세한 내용은 [docs/linux-design.md](docs/linux-design.md)를 보세요.

## 프로토타입

[`prototype/`](prototype/)에는 이 네이티브 버전의 바탕이 된 최초의 Python 빠른 프로토타입(Windows, pywebview)이 있습니다. 참고용으로 남겨 두었으며 유지보수하지 않습니다.

## 소스에서 빌드

CMake, Ninja, Node.js 22와 MSVC(Windows) 또는 GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev`(Linux)가 필요합니다. 정확한 단계는 [.github/workflows/build.yml](.github/workflows/build.yml)을 보세요.

## 라이선스

MIT. Latex Suite(MIT), CodeMirror(MIT), MathJax(Apache 2.0), QuickJS-ng(MIT) 등의 구성 요소는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 있습니다.
