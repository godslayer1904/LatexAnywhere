# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · **Tiếng Việt** · [Русский](README.ru.md) · [العربية](README.ar.md) · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

Gõ công thức LaTeX trong **bất kỳ** ô nhập nào, theo cách của [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) trên Obsidian.
Nhấn `Alt+L`, gõ bằng các snippet của Latex Suite với bản xem trước trực tiếp, nhấn `Enter` — công thức được chèn vào đúng chỗ con trỏ.

- Ô công thức chạy **chính Latex Suite gốc** (1.12.8), không phải bản viết lại: snippet, tabstop, phân số tự động, phím tắt ma trận hoạt động giống hệt trong Obsidian (đối chiếu từng phím với bản gốc trên 5.220 phiên nhập đã ghi lại).
- Tự động dùng **thiết lập Latex Suite của riêng bạn** trong Obsidian.
- Chương trình gốc, nhỏ gọn: không có nhân trình duyệt, mở ra chỉ trong vài mili giây.
- Hỗ trợ Windows (x64, ARM64) và Linux (x86_64, aarch64). Giao diện 10 ngôn ngữ.

## Cài đặt

**Windows 10/11** — tải `LatexAnywhere-Setup-*.exe` ở [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) rồi chạy. Một bộ cài dùng chung cho x64 và ARM64, không cần quyền quản trị.

**Linux** (GTK 4.12 trở lên, ví dụ Ubuntu 24.04+, Fedora 40+; đã thử trên Ubuntu 26.04 / GNOME 50 Wayland):

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

Gỡ cài đặt: trên Linux chạy `bash install.sh --uninstall`; trên Windows vào *Cài đặt → Ứng dụng*.

## Sử dụng

1. Trong bất kỳ ô nhập nào (trình duyệt, Word, ứng dụng chat, trình soạn thảo, terminal…) nhấn `Alt+L`.
2. Gõ công thức. `x^2` tự thành `x^{2}`, `//` tạo phân số, `Tab` nhảy tới ô tiếp theo. Công thức đã hiển thị nằm phía trên.
3. Chèn:

| Phím | Chèn vào | Phù hợp với |
|---|---|---|
| `Enter` | công thức `x^{2}` | LaTeX, Overleaf |
| `Alt+Enter` | công thức nội dòng `$x^{2}$` | Typora, Obsidian, Markdown |
| `Ctrl+Enter` | ký tự Unicode `x²` | ứng dụng chat không hiển thị công thức |
| `Ctrl+Shift+Enter` | phương trình gốc của Word (Windows) | Word, PowerPoint |
| `Shift+Enter` | xuống dòng trong ô | công thức nhiều dòng |
| `Esc` hoặc nhấn lại `Alt+L` | không chèn gì | hủy |

Bôi đen văn bản trước khi nhấn `Alt+L` để sửa nó trong ô; khi chèn, phần bôi đen sẽ được thay thế.
Ô công thức tự chuyển bộ gõ sang tiếng Anh và chuyển lại sau đó. Nội dung đã chèn cũng được giữ lại trong clipboard, nên nếu ứng dụng chưa nhận được, bạn có thể tự dán lại.

Hướng dẫn: biểu tượng ở khay → *Hướng dẫn*, trên Linux còn có thể nhấn `Alt+Shift+L`. Thoát khẩn cấp: `Win+Alt+L` (Windows) / `Super+Alt+L` (Linux).

## Thiết lập

LatexAnywhere đọc tệp `data.json` của Latex Suite trong các vault Obsidian của bạn. Muốn dùng thiết lập khác, đặt một tệp `data.json` của Latex Suite tại `%APPDATA%\LatexAnywhere\settings.json` (Windows) hoặc `~/.config/LatexAnywhere/settings.json` (Linux).

## Ghi chú về Linux

Wayland không cho chương trình đọc cửa sổ khác hay gõ vào cửa sổ khác, nên bản Linux hoạt động khác ở bên trong (phím tắt GNOME, AT-SPI, bàn phím ảo, khay StatusNotifierItem). Trên Linux, vị trí ô do môi trường desktop quyết định chứ không nằm cạnh con trỏ. Chi tiết: [docs/linux-design.md](docs/linux-design.md).

## Bản thử nghiệm

[`prototype/`](prototype/) chứa bản thử nghiệm nhanh đầu tiên viết bằng Python (Windows, pywebview), từ đó bản gốc này được viết lại. Giữ lại để tham khảo, không còn được bảo trì.

## Biên dịch từ mã nguồn

Cần CMake, Ninja, Node.js 22, và MSVC (Windows) hoặc GCC + `libgtk-4-dev librsvg2-dev libatspi2.0-dev` (Linux). Các bước cụ thể xem [.github/workflows/build.yml](.github/workflows/build.yml).

## Giấy phép

MIT. Latex Suite (MIT), CodeMirror (MIT), MathJax (Apache 2.0), QuickJS-ng (MIT) và các thành phần khác được liệt kê trong [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
