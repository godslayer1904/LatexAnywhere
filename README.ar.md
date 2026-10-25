<div dir="rtl">

# LatexAnywhere

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Tiếng Việt](README.vi.md) · [Русский](README.ru.md) · **العربية** · [Español](README.es.md) · [Português (Brasil)](README.pt-BR.md)

اكتب معادلات LaTeX في **أي** حقل إدخال، بطريقة [Latex Suite](https://github.com/artisticat1/obsidian-latex-suite) في Obsidian.
اضغط `Alt+L`، واكتب باستخدام مقتطفات Latex Suite مع معاينة فورية، ثم اضغط `Enter`، فتُدرج المعادلة في موضع المؤشر.

- يعمل داخل المربع **Latex Suite الأصلي** (الإصدار 1.12.8)، لا نسخة مُعادة كتابتها: المقتطفات والتنقل بين الخانات والكسور التلقائية واختصارات المصفوفات تعمل تمامًا كما في Obsidian (تمت مطابقتها مع الأصل مفتاحًا بمفتاح في 5220 جلسة إدخال مسجّلة).
- يستخدم تلقائيًا **إعدادات Latex Suite الخاصة بك** في Obsidian.
- برنامج أصلي وخفيف: بلا محرك متصفح، ويُفتح خلال أجزاء من الثانية.
- يدعم Windows (x64 وARM64) وLinux (x86_64 وaarch64)، والواجهة بعشر لغات.

## التثبيت

**Windows 10/11**: نزّل `LatexAnywhere-Setup-*.exe` من [Releases](https://github.com/godslayer1904/LatexAnywhere/releases/latest) وشغّله. مثبّت واحد لـ x64 وARM64، ولا يحتاج إلى صلاحيات المسؤول.

**Linux** (GTK 4.12 أو أحدث، مثل Ubuntu 24.04+ وFedora 40+؛ تم اختباره على Ubuntu 26.04 / GNOME 50 Wayland):

</div>

```bash
curl -fsSL https://github.com/godslayer1904/LatexAnywhere/releases/latest/download/install.sh | bash
```

<div dir="rtl">

لإلغاء التثبيت: في Linux نفّذ `bash install.sh --uninstall`، وفي Windows من *الإعدادات ← التطبيقات*.

## الاستخدام

1. في أي حقل إدخال (المتصفح، Word، تطبيقات الدردشة، المحرر، الطرفية…) اضغط `Alt+L`.
2. اكتب المعادلة. يتحول `x^2` إلى `x^{2}`، و`//` تُنشئ كسرًا، و`Tab` ينتقل إلى الخانة التالية. تظهر المعادلة المعروضة في الأعلى.
3. الإدراج:

| المفتاح | ما يُدرَج | مناسب لـ |
|---|---|---|
| `Enter` | المعادلة نفسها `x^{2}` | LaTeX وOverleaf |
| `Alt+Enter` | معادلة داخل السطر `$x^{2}$` | Typora وObsidian وMarkdown |
| `Ctrl+Enter` | رموز Unicode `x²` | تطبيقات الدردشة التي لا تعرض المعادلات |
| `Ctrl+Shift+Enter` | معادلة Word أصلية (Windows) | Word وPowerPoint |
| `Shift+Enter` | سطر جديد داخل المربع | المعادلات متعددة الأسطر |
| `Esc` أو `Alt+L` مرة أخرى | لا شيء | إلغاء |

حدّد نصًا قبل الضغط على `Alt+L` لتعديله داخل المربع؛ وعند الإدراج يحل محل التحديد.
يبدّل المربع طريقة الإدخال إلى الإنجليزية تلقائيًا ثم يعيدها بعد ذلك، وتُستعاد الحافظة كما كانت.

التعليمات: أيقونة شريط النظام ← *التعليمات*، وفي Linux أيضًا `Alt+Shift+L`. الخروج الطارئ: `Win+Alt+L` (Windows) / `Super+Alt+L` (Linux).

## الإعدادات

يقرأ LatexAnywhere ملف `data.json` الخاص بـ Latex Suite من خزائن Obsidian لديك. لاستخدام إعدادات أخرى، ضع ملف `data.json` من Latex Suite في `%APPDATA%\LatexAnywhere\settings.json` (Windows) أو `~/.config/LatexAnywhere/settings.json` (Linux).

## ملاحظات Linux

لا يسمح Wayland للبرامج بقراءة النوافذ الأخرى أو الكتابة فيها، لذلك تعمل نسخة Linux بآلية داخلية مختلفة (اختصار GNOME، وAT-SPI، ولوحة مفاتيح افتراضية، وأيقونة StatusNotifierItem). في Linux يحدد سطح المكتب موضع المربع، ولا يظهر بجوار المؤشر. التفاصيل في [docs/linux-design.md](docs/linux-design.md).

## النموذج الأولي

يحتوي [`prototype/`](prototype/) على النموذج الأولي السريع الأول المكتوب بلغة Python (Windows، pywebview)، والذي أُعيدت كتابة هذه النسخة الأصلية على أساسه. أُبقي عليه للاطلاع، ولا تتم صيانته.

## البناء من المصدر

تحتاج إلى CMake وNinja وNode.js 22، مع MSVC (Windows) أو GCC و`libgtk-4-dev librsvg2-dev libatspi2.0-dev` (Linux). الخطوات بالتفصيل في [.github/workflows/build.yml](.github/workflows/build.yml).

## الترخيص

MIT. المكوّنات Latex Suite (MIT) وCodeMirror (MIT) وMathJax (Apache 2.0) وQuickJS-ng (MIT) وغيرها مذكورة في [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

</div>
