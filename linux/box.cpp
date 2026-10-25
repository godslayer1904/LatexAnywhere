#include "box.h"

#include <librsvg/rsvg.h>
#include <linux/input-event-codes.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>

#include "util.h"

namespace la {
namespace {

// ---------- 配色（Obsidian / Latex Suite 的颜色，与 Windows 版相同） ----------
struct Rgb { int r, g, b; };
Rgb Blend(Rgb f, double a, Rgb b) {
	auto m = [&](int x, int y) { return (int)(x * a + y * (1 - a) + .5); };
	return {m(f.r, b.r), m(f.g, b.g), m(f.b, b.b)};
}
struct Theme {
	Rgb bg, text, border, cmd, var, num, dollar, sel;
	Rgb bracket[3], mismatch, highlight, ph[3];
};
Theme MakeTheme(bool dark) {
	Theme t;
	if (dark) {
		t.bg = {0x1e, 0x1e, 0x1e}; t.text = {0xda, 0xda, 0xda}; t.border = {0x36, 0x36, 0x36};
		t.cmd = {0xfb, 0x46, 0x4c}; t.var = {0x53, 0xdf, 0xdd}; t.num = {0xa8, 0x82, 0xff}; t.dollar = {0x44, 0xcf, 0x6e};
		t.bracket[0] = {0x47, 0xb8, 0xff}; t.bracket[1] = {0xff, 0x55, 0xcd}; t.bracket[2] = {0x73, 0xff, 0x63};
		t.highlight = Blend({0xa5, 0x8f, 0xf4}, 0.6, t.bg);
	} else {
		t.bg = {0xff, 0xff, 0xff}; t.text = {0x22, 0x22, 0x22}; t.border = {0xdd, 0xdd, 0xdd};
		t.cmd = {0xe9, 0x31, 0x47}; t.var = {0x00, 0xa3, 0xa0}; t.num = {0x8a, 0x5c, 0xf5}; t.dollar = {0x08, 0xb9, 0x4e};
		t.bracket[0] = {0x52, 0x7a, 0xff}; t.bracket[1] = {0xff, 0x50, 0xb7}; t.bracket[2] = {0x69, 0xba, 0x00};
		t.highlight = Blend({0x3b, 0x14, 0xb8}, 0.3, t.bg);
	}
	t.mismatch = Blend({255, 18, 18}, 0.8, t.bg);
	t.ph[0] = Blend({0x87, 0xce, 0xfa}, 0x2e / 255.0, t.bg);
	t.ph[1] = Blend({0xff, 0xa5, 0x00}, 0x33 / 255.0, t.bg);
	t.ph[2] = Blend({0x00, 0xff, 0x00}, 0x22 / 255.0, t.bg);
	t.sel = Blend({137, 108, 255}, 0.33, t.bg);
	return t;
}
Theme g_theme = MakeTheme(true);
std::string Hex(Rgb c) {
	char b[16];
	snprintf(b, sizeof b, "#%02x%02x%02x", c.r, c.g, c.b);
	return b;
}
bool Same(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

// 公式区段：$$…$$ 与 $…$（与 Windows 版、预览相同的规则）
struct MathSpan { int from, to; int open; };
std::vector<MathSpan> FindMath(const std::u16string& s) {
	std::vector<MathSpan> out;
	size_t i = 0;
	while (i < s.size()) {
		if (s[i] != u'$') { i++; continue; }
		bool disp = i + 1 < s.size() && s[i + 1] == u'$';
		if (disp) {
			size_t j = s.find(u"$$", i + 2);
			if (j == std::u16string::npos || j == i + 2) { i += 2; continue; }
			out.push_back({(int)i, (int)j + 2, 2});
			i = j + 2;
		} else {
			size_t j = i + 1;
			while (j < s.size() && s[j] != u'$' && s[j] != u'\n') j++;
			if (j >= s.size() || s[j] != u'$' || j == i + 1) { i++; continue; }
			out.push_back({(int)i, (int)j + 1, 1});
			i = j + 1;
		}
	}
	return out;
}

constexpr float kExPerEm = 0.431f;   // MathJax 的 ex 与字号之比（同 Windows 版）
bool AttrEx(const std::string& svg, const char* name, float* ex) {
	std::string key = std::string(" ") + name + "=\"";
	size_t p = svg.find(key);
	if (p == std::string::npos) return false;
	*ex = std::strtof(svg.c_str() + p + key.size(), nullptr);
	return true;
}
std::string PrepareSvg(const std::string& in, float wPx, float hPx, const std::string& color) {
	std::string s = in;
	size_t end = s.find('>');
	if (end == std::string::npos) return s;
	std::string head = s.substr(0, end);
	static const std::regex w(R"( width="[^"]*")"), h(R"( height="[^"]*")"), st(R"( style="[^"]*")");
	char buf[64];
	snprintf(buf, sizeof buf, " width=\"%.2f\"", wPx);
	head = std::regex_replace(head, w, buf, std::regex_constants::format_first_only);
	snprintf(buf, sizeof buf, " height=\"%.2f\"", hPx);
	head = std::regex_replace(head, h, buf, std::regex_constants::format_first_only);
	head = std::regex_replace(head, st, "", std::regex_constants::format_first_only);
	s = head + s.substr(end);
	for (size_t p = 0; (p = s.find("currentColor", p)) != std::string::npos; p += color.size()) s.replace(p, 12, color);
	return s;
}

bool Trace() { static bool on = getenv("LA_TRACE") != nullptr; return on; }   // 测试用：记录每个按键

// ---------- 上屏用的剪贴板内容 ----------
// Wayland 下没有焦点就不能再改剪贴板，粘贴完无法把原内容「放回去」。所以一直由我们持有剪贴板：
// 粘贴完成前对方要文字就给公式；restored 之后对方再要，一律给弹框前快照的原内容——效果等同于恢复原样。
struct ClipData {
	std::string formula;
	std::vector<std::pair<std::string, GBytes*>> old;
	bool restored = false;
	GdkContentFormats* formats = nullptr;
};
typedef struct { GdkContentProvider parent; ClipData* d; } LaClip;
typedef struct { GdkContentProviderClass parent_class; } LaClipClass;
G_DEFINE_TYPE(LaClip, la_clip, GDK_TYPE_CONTENT_PROVIDER)

const char* const kTextMimes[] = {"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "STRING", "TEXT"};
bool IsText(const char* m) {
	for (const char* t : kTextMimes) if (!strcmp(m, t)) return true;
	return false;
}
void la_clip_init(LaClip* self) { self->d = new ClipData; }
void la_clip_finalize(GObject* o) {
	ClipData* d = ((LaClip*)o)->d;
	for (auto& p : d->old) g_bytes_unref(p.second);
	if (d->formats) gdk_content_formats_unref(d->formats);
	delete d;
	G_OBJECT_CLASS(la_clip_parent_class)->finalize(o);
}
GdkContentFormats* la_clip_ref_formats(GdkContentProvider* p) { return gdk_content_formats_ref(((LaClip*)p)->d->formats); }
void la_clip_write_async(GdkContentProvider* p, const char* mime, GOutputStream* stream, int prio, GCancellable* c, GAsyncReadyCallback cb, gpointer data) {
	GTask* task = g_task_new(p, c, cb, data);
	ClipData* d = ((LaClip*)p)->d;
	GBytes* bytes = nullptr;
	if (!d->restored && IsText(mime)) bytes = g_bytes_new(d->formula.data(), d->formula.size());
	else
		for (auto& o : d->old)
			if (o.first == mime) { bytes = g_bytes_ref(o.second); break; }
	if (!bytes) {
		g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "no such format");
		g_object_unref(task);
		return;
	}
	g_task_set_task_data(task, bytes, (GDestroyNotify)g_bytes_unref);
	gsize n;
	const void* buf = g_bytes_get_data(bytes, &n);
	g_output_stream_write_all_async(stream, buf, n, prio, c, [](GObject* s, GAsyncResult* r, gpointer t) {
		GError* e = nullptr;
		if (g_output_stream_write_all_finish(G_OUTPUT_STREAM(s), r, nullptr, &e)) g_task_return_boolean(G_TASK(t), TRUE);
		else g_task_return_error(G_TASK(t), e);
		g_object_unref(t);
	}, task);
}
gboolean la_clip_write_finish(GdkContentProvider*, GAsyncResult* r, GError** e) { return g_task_propagate_boolean(G_TASK(r), e); }
void la_clip_class_init(LaClipClass* k) {
	G_OBJECT_CLASS(k)->finalize = la_clip_finalize;
	GdkContentProviderClass* pk = GDK_CONTENT_PROVIDER_CLASS(k);
	pk->ref_formats = la_clip_ref_formats;
	pk->write_mime_type_async = la_clip_write_async;
	pk->write_mime_type_finish = la_clip_write_finish;
}

}  // namespace

struct Box::Item {
	bool math = false, error = false;
	RsvgHandle* svg = nullptr;
	PangoLayout* text = nullptr;
	float w = 0, h = 0, baseline = 0, scale = 1, x = 0, y = 0;
	~Item() {
		if (svg) g_object_unref(svg);
		if (text) g_object_unref(text);
	}
};

void Box::ClearItems() {
	for (Item* it : items_) delete it;
	items_.clear();
	pvW_ = pvH_ = 0;
}

// ---------------- 创建 ----------------
bool Box::Create(GtkApplication* app, Engine* engine, VirtualKeyboard* kb) {
	eng_ = engine;
	kb_ = kb;
	win_ = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(win_), "LatexAnywhere");
	gtk_window_set_decorated(GTK_WINDOW(win_), FALSE);
	gtk_window_set_resizable(GTK_WINDOW(win_), FALSE);   // 大小跟着内容走
	gtk_window_set_hide_on_close(GTK_WINDOW(win_), TRUE);
	gtk_widget_add_css_class(win_, "la-box");

	// 整个框都能拖动（文字区自己处理鼠标，不受影响）
	GtkWidget* handle = gtk_window_handle_new();
	gtk_window_set_child(GTK_WINDOW(win_), handle);
	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_start(vbox, 6);
	gtk_widget_set_margin_end(vbox, 6);
	gtk_widget_set_margin_top(vbox, 6);
	gtk_widget_set_margin_bottom(vbox, 6);
	gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), vbox);

	preview_ = gtk_drawing_area_new();
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(preview_), DrawPreview, this, nullptr);
	previewScroll_ = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(previewScroll_), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(previewScroll_), TRUE);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(previewScroll_), 200);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(previewScroll_), preview_);
	gtk_widget_set_margin_top(previewScroll_, 4);
	gtk_widget_set_margin_bottom(previewScroll_, 4);
	gtk_box_append(GTK_BOX(vbox), previewScroll_);
	sep_ = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_margin_bottom(sep_, 3);
	gtk_box_append(GTK_BOX(vbox), sep_);

	text_ = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(text_), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_), 2);
	buf_ = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_));
	gtk_text_buffer_set_enable_undo(buf_, FALSE);   // 撤销由引擎（原版 CodeMirror 历史）负责
	gtk_text_buffer_create_tag(buf_, "it", "style", PANGO_STYLE_ITALIC, nullptr);
	gtk_text_buffer_create_tag(buf_, "preedit", "underline", PANGO_UNDERLINE_SINGLE, nullptr);
	editScroll_ = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editScroll_), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(editScroll_), 240);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(editScroll_), text_);
	gtk_box_append(GTK_BOX(vbox), editScroll_);

	// 自己的输入法上下文：按键先给输入法，输入法不要的才交给引擎（相当于 Windows 的 WM_IME_* 处理）
	im_ = gtk_im_multicontext_new();
	gtk_im_context_set_client_widget(im_, text_);
	gtk_im_context_set_use_preedit(im_, TRUE);
	g_signal_connect(im_, "commit", G_CALLBACK(OnCommit), this);
	g_signal_connect(im_, "preedit-changed", G_CALLBACK(OnPreeditChanged), this);
	GtkEventController* keys = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);   // 抢在文字控件之前：它的快捷键一个都不要
	gtk_event_controller_key_set_im_context(GTK_EVENT_CONTROLLER_KEY(keys), im_);
	g_signal_connect(keys, "key-pressed", G_CALLBACK(OnKey), this);
	gtk_widget_add_controller(text_, keys);
	GtkEventController* focus = gtk_event_controller_focus_new();
	g_signal_connect_swapped(focus, "enter", G_CALLBACK(gtk_im_context_focus_in), im_);
	g_signal_connect_swapped(focus, "leave", G_CALLBACK(gtk_im_context_focus_out), im_);
	gtk_widget_add_controller(text_, focus);

	g_signal_connect(buf_, "mark-set", G_CALLBACK(OnMarkSet), this);
	g_signal_connect(buf_, "changed", G_CALLBACK(OnChanged), this);
	// 框拿到焦点后：键盘焦点交给编辑区，自己的输入法切到英文（只动公式框自己的）
	g_signal_connect(win_, "notify::is-active", G_CALLBACK(+[](GObject* w, GParamSpec*, Box* self) {
		if (Trace()) Log("跟踪：框 is-active=%d", (int)gtk_window_is_active(GTK_WINDOW(w)));
		if (!self->visible_ || !gtk_window_is_active(GTK_WINDOW(w))) return;
		gtk_widget_grab_focus(self->text_);
		self->SnapshotClipboard();   // Wayland 只有拿着焦点时才能读剪贴板
		g_timeout_add(60, [](gpointer) -> gboolean { ImeSetActive(false); return G_SOURCE_REMOVE; }, nullptr);
	}), this);
	// 拿到焦点时剪贴板的格式可能还没传过来：内容一到再快照一次
	g_signal_connect(gtk_widget_get_clipboard(win_), "changed", G_CALLBACK(+[](GdkClipboard* cb, Box* self) {
		if (self->visible_ && !gdk_clipboard_is_local(cb) && gtk_window_is_active(GTK_WINDOW(self->win_))) self->SnapshotClipboard();
	}), this);
	g_signal_connect(win_, "close-request", G_CALLBACK(+[](GtkWindow*, Box* self) -> gboolean { self->Cancel(); return TRUE; }), this);

	css_ = gtk_css_provider_new();
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css_), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
	SetDark(dark_);
	return true;
}

void Box::SetDark(bool dark) {
	dark_ = dark;
	g_theme = MakeTheme(dark);
	const Theme& t = g_theme;
	std::string css =
		"window.la-box { background: " + Hex(t.bg) + "; border: 1px solid " + Hex(t.border) + "; border-radius: 6px; }\n"
		"window.la-box textview, window.la-box textview text { background: " + Hex(t.bg) + "; color: " + Hex(t.text) + "; font-size: 15px; caret-color: " + Hex(t.text) + "; }\n"
		"window.la-box textview text selection { background: " + Hex(t.sel) + "; color: inherit; }\n"
		"window.la-box separator { background: " + Hex(t.border) + "; min-height: 1px; }\n"
		"window.la-box scrolledwindow, window.la-box viewport { background: transparent; }\n";
	gtk_css_provider_load_from_string(css_, css.c_str());
	if (visible_) { Restyle(); UpdatePreview(); }
}

// ---------------- 弹框、上屏、取消 ----------------
void Box::Toggle() {
	double now = NowMs();
	if (now - lastToggle_ < 300) return;   // 防连按
	lastToggle_ = now;
	if (visible_) { Cancel(); return; }
	Open();
}

void Box::Open() {
	double t0 = NowMs();
	ImeQuery([this](bool on) { prevIme_ = on; });   // 趁原程序还有焦点时问
	bool atspi = false;
	std::u16string sel = SelectionGet(&atspi);
	double tSel = NowMs() - t0;
	// 一律放进行内公式里编辑（上屏时去掉外层 $）：Latex Suite 只在公式里生效
	EditResult r = eng_->Reset(u"$" + sel + u"$", 1, 1 + (int)sel.size());
	if (!r.ok) { Log("引擎出错，不弹框：%s", r.error.c_str()); return; }
	applying_ = true;
	gtk_text_buffer_set_text(buf_, "", 0);
	applying_ = false;
	mirror_.clear();
	preedit_.clear();
	Apply(r);
	UpdatePreview();
	visible_ = true;
	Relayout();
	gtk_widget_grab_focus(text_);
	gtk_window_present(GTK_WINDOW(win_));
	Log("弹框：选区 %.0fms（%s，长度 %zu），显示 %.0fms", tSel, atspi ? "AT-SPI" : "无", sel.size(), NowMs() - t0);
}

void Box::Hide() {
	visible_ = false;
	if (!preedit_.empty()) gtk_im_context_reset(im_);
	gtk_widget_set_visible(win_, FALSE);
	g_idle_add([](gpointer p) -> gboolean {   // 上屏/取消的动作先做，空闲了再回收
		Box* self = static_cast<Box*>(p);
		if (!self->visible_) {
			double t0 = NowMs();
			self->eng_->CollectGarbage();
			Log("收起：回收 %.0fms，JS 内存 %.1f MB", NowMs() - t0, self->eng_->MemoryUsed() / 1048576.0);
		}
		return G_SOURCE_REMOVE;
	}, this);
}

void Box::Commit(CommitMode mode) {
	std::u16string text = mirror_;
	const char* name = mode == CommitMode::MathML ? "MathML" : mode == CommitMode::Unicode ? "Unicode" : mode == CommitMode::Keep ? "保留$" : "LaTeX";
	Log("上屏：%s（%zu 字）", name, text.size());
	bool keep = mode == CommitMode::Keep, raw = mode == CommitMode::MathML;
	if (!keep && !raw && text.size() >= 2 && text.front() == u'$' && text.back() == u'$' && text.compare(0, 2, u"$$") != 0)
		text = text.substr(1, text.size() - 2);   // 去掉外层 $，只插公式本身
	std::string err;
	if (mode == CommitMode::Unicode) {
		std::u16string out;
		if (eng_->CallString("LatexAnywhereMath", "toUnicode", text, &out, &err)) text = out;
		else Log("Unicode 转换失败，按原样上屏：%s", err.c_str());
	} else if (raw) {
		std::u16string out;
		if (eng_->CallString("LatexAnywhereMath", "mathml", text, &out, &err)) text = out;
		else Log("MathML 转换失败，按原样上屏：%s", err.c_str());
	}
	if (text.empty()) { Cancel(); return; }
	// Wayland 只有拿着焦点时才能占用剪贴板：趁框还在前台放进剪贴板。
	// 不碰 PRIMARY 选区：GTK 程序一失去 PRIMARY 就取消自己的选中，「选中后替换」会变成插入（实测）
	LaClip* lc = (LaClip*)g_object_new(la_clip_get_type(), nullptr);
	lc->d->formula = ToUtf8(text);
	GdkContentFormatsBuilder* fb = gdk_content_formats_builder_new();
	for (const char* t : kTextMimes) gdk_content_formats_builder_add_mime_type(fb, t);
	if (clipPending_ == 0)
		for (ClipItem& c : clip_) { lc->d->old.push_back({c.mime, g_bytes_ref(c.data)}); gdk_content_formats_builder_add_mime_type(fb, c.mime.c_str()); }
	else lc->d->old.clear();
	lc->d->formats = gdk_content_formats_builder_free_to_formats(fb);
	if (curClip_) g_object_unref(curClip_);
	curClip_ = G_OBJECT(lc);
	keepFormula_ = clipPending_ > 0;   // 快照没读完：只好把公式留在剪贴板里
	gdk_clipboard_set_content(gtk_widget_get_clipboard(win_), GDK_CONTENT_PROVIDER(lc));
	// 用户往往还按着 Ctrl / Alt（Ctrl+Enter、Alt+Enter）：这时发 Shift+Insert 会变成别的组合键。等他松手再收框、粘贴（最多 1.5 秒）
	struct Wait { Box* self; std::u16string text; double t0; bool ime; };
	auto* w = new Wait{this, text, NowMs(), prevIme_};
	g_timeout_add(10, [](gpointer p) -> gboolean {
		auto* w = static_cast<Wait*>(p);
		GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
		GdkDevice* kbd = seat ? gdk_seat_get_keyboard(seat) : nullptr;
		guint held = kbd ? gdk_device_get_modifier_state(kbd) & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SHIFT_MASK | GDK_SUPER_MASK | GDK_META_MASK) : 0;
		if (held && NowMs() - w->t0 < 1500) return G_SOURCE_CONTINUE;
		if (held) Log("修饰键 1.5 秒还没松开，照常上屏");
		w->self->Hide();
		w->self->FinishPaste(std::move(w->text), w->ime);
		delete w;
		return G_SOURCE_REMOVE;
	}, w);
}

void Box::FinishPaste(std::u16string text, bool restoreIme) {
	if (!kb_->Ok()) { Log("没有虚拟键盘（%s）：公式已放进剪贴板，需要手动 Ctrl+V", kb_->error.c_str()); return; }
	struct Job { Box* self; bool ime; };
	auto* j = new Job{this, restoreIme};
	// 等焦点回到原程序再粘贴（Wayland 拿不到「焦点已切换」的通知，实测 150ms 足够）
	g_timeout_add(150, [](gpointer p) -> gboolean {
		auto* j = static_cast<Job*>(p);
		j->self->kb_->Tap({KEY_LEFTSHIFT, KEY_INSERT});   // 与键盘布局无关；GTK、Qt、浏览器、LibreOffice、终端都认
		if (j->ime) g_timeout_add(80, [](gpointer) -> gboolean { ImeSetActive(true); return G_SOURCE_REMOVE; }, nullptr);
		// 对方读剪贴板是异步的：放回太早它会读到旧内容（Windows 版同样等 300ms）
		g_timeout_add(400, [](gpointer p) -> gboolean { static_cast<Box*>(p)->RestoreClipboard(); return G_SOURCE_REMOVE; }, j->self);
		delete j;
		return G_SOURCE_REMOVE;
	}, j);
	(void)text;
}

void Box::SnapshotClipboard() {
	for (ClipItem& c : clip_) g_bytes_unref(c.data);
	clip_.clear();
	clipPending_ = 0;
	unsigned gen = ++clipGen_;
	GdkClipboard* cb = gtk_widget_get_clipboard(win_);
	GdkContentFormats* f = gdk_clipboard_get_formats(cb);
	gsize n = 0;
	const char* const* mimes = f ? gdk_content_formats_get_mime_types(f, &n) : nullptr;
	clipEmpty_ = n == 0;
	if (Trace()) Log("跟踪：剪贴板快照 %zu 种格式%s", (size_t)n, gdk_clipboard_is_local(cb) ? "（自己的）" : "");
	for (gsize i = 0; i < n && i < 24; i++) {
		struct Req { Box* self; unsigned gen; std::string mime; };
		auto* r = new Req{this, gen, mimes[i]};
		const char* one[] = {mimes[i], nullptr};
		clipPending_++;
		gdk_clipboard_read_async(cb, one, G_PRIORITY_DEFAULT, nullptr, [](GObject* src, GAsyncResult* res, gpointer p) {
			auto* r = static_cast<Req*>(p);
			GInputStream* in = gdk_clipboard_read_finish(GDK_CLIPBOARD(src), res, nullptr, nullptr);
			if (!in || r->gen != r->self->clipGen_) { if (in) g_object_unref(in); if (r->gen == r->self->clipGen_) r->self->clipPending_--; delete r; return; }
			GOutputStream* out = g_memory_output_stream_new_resizable();
			g_output_stream_splice_async(out, in, (GOutputStreamSpliceFlags)(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
				G_PRIORITY_DEFAULT, nullptr, [](GObject* o, GAsyncResult* res, gpointer p) {
					auto* r = static_cast<Req*>(p);
					gssize got = g_output_stream_splice_finish(G_OUTPUT_STREAM(o), res, nullptr);
					if (r->gen == r->self->clipGen_) {
						r->self->clipPending_--;
						if (got >= 0 && got < 32 * 1024 * 1024)
							r->self->clip_.push_back({r->mime, g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(o))});
					}
					g_object_unref(o);
					delete r;
				}, r);
			g_object_unref(in);
		}, r);
	}
}

void Box::RestoreClipboard() {
	if (!curClip_) return;
	if (keepFormula_) { Log("剪贴板快照没读完，公式留在剪贴板里"); return; }
	((LaClip*)curClip_)->d->restored = true;
	if (Trace()) Log("跟踪：剪贴板恢复原样（%zu 种格式）", ((LaClip*)curClip_)->d->old.size());
}

void Box::Cancel() {
	bool ime = prevIme_;
	Hide();
	if (ime) g_timeout_add(150, [](gpointer) -> gboolean { ImeSetActive(true); return G_SOURCE_REMOVE; }, nullptr);
}

// ---------------- 下标换算 ----------------
int Box::Cp(int u16) const {
	int cp = 0;
	for (int i = 0; i < u16 && i < (int)mirror_.size(); i++) {
		if (mirror_[i] >= 0xDC00 && mirror_[i] < 0xE000 && i > 0 && mirror_[i - 1] >= 0xD800 && mirror_[i - 1] < 0xDC00) continue;
		cp++;
	}
	return cp;
}
int Box::U16(int cp) const {
	int i = 0;
	for (int c = 0; c < cp && i < (int)mirror_.size(); c++) {
		if (mirror_[i] >= 0xD800 && mirror_[i] < 0xDC00 && i + 1 < (int)mirror_.size()) i += 2;
		else i++;
	}
	return i;
}
GtkTextIter Box::Iter(int u16) {
	GtkTextIter it;
	gtk_text_buffer_get_iter_at_offset(buf_, &it, Cp(u16));
	return it;
}

// ---------------- 引擎结果 → 控件 ----------------
void Box::Apply(const EditResult& r) {
	if (!r.ok) { Log("引擎出错，保留原文：%s", r.error.c_str()); return; }
	applying_ = true;
	if (r.doc != mirror_) {
		size_t p = 0, oldN = mirror_.size(), newN = r.doc.size();
		while (p < oldN && p < newN && mirror_[p] == r.doc[p]) p++;
		size_t s = 0;
		while (s < oldN - p && s < newN - p && mirror_[oldN - 1 - s] == r.doc[newN - 1 - s]) s++;
		GtkTextIter a = Iter((int)p), b = Iter((int)(oldN - s));
		gtk_text_buffer_delete(buf_, &a, &b);
		std::string mid = ToUtf8(r.doc.substr(p, newN - p - s));
		gtk_text_buffer_insert(buf_, &a, mid.data(), (int)mid.size());
		mirror_ = r.doc;
		g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, [](gpointer p) -> gboolean {   // 预览放到按键处理之后再画，不挡打字
			Box* self = static_cast<Box*>(p);
			if (self->visible_) { self->UpdatePreview(); self->Relayout(); }
			return G_SOURCE_REMOVE;
		}, this, nullptr);
	}
	anchor_ = r.anchor;
	head_ = r.head;
	Restyle();
	applying_ = false;
}

void Box::Restyle() {
	const Theme& t = g_theme;
	bool was = applying_;
	applying_ = true;
	GtkTextIter s, e;
	gtk_text_buffer_get_bounds(buf_, &s, &e);
	gtk_text_buffer_remove_all_tags(buf_, &s, &e);
	auto tag = [&](const char* kind, Rgb c) {
		std::string name = std::string(kind) + Hex(c);
		GtkTextTagTable* tt = gtk_text_buffer_get_tag_table(buf_);
		if (!gtk_text_tag_table_lookup(tt, name.c_str())) {
			GdkRGBA rgba{c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f};
			gtk_text_buffer_create_tag(buf_, name.c_str(), kind[0] == 'f' ? "foreground-rgba" : "background-rgba", &rgba, nullptr);
		}
		return name;
	};
	auto fmt = [&](int a, int b, const char* name) {
		if (a >= b) return;
		GtkTextIter x = Iter(a), y = Iter(b);
		gtk_text_buffer_apply_tag_by_name(buf_, name, &x, &y);
	};
	const int n = (int)mirror_.size();
	std::vector<Rgb> back(n, t.bg);
	std::vector<int> fg(n, -1);   // -1：按语法着色；否则是括号色的下标（0..2 括号配色，3 不匹配）
	for (const Mark& m : eng_->Marks()) {
		int a = std::max(0, m.from), b = std::min(n, m.to);
		for (int i = a; i < b; i++) {
			const std::string& c = m.cls;
			if (c.find("latex-suite-snippet-placeholder") != std::string::npos) {
				int k = c.back() - '0';
				back[i] = t.ph[(k >= 0 && k < 3) ? k : 0];
			}
			if (c.find("latex-suite-highlighted-bracket") != std::string::npos) back[i] = t.highlight;
			if (c.find("latex-suite-mismatched-bracket") != std::string::npos) fg[i] = 3;
			size_t cb = c.find("latex-suite-color-bracket-");
			if (cb != std::string::npos) { int k = c[cb + 26] - '0'; fg[i] = (k >= 0 && k < 3) ? k : 0; }
		}
	}
	// 公式内着色：命令红、字母青、数字紫、$ 绿、斜体（与 Windows 版一致）
	std::vector<int> syn(n, -1);   // 0 $ 1 命令 2 字母 3 数字 4 其他
	std::vector<bool> ital(n, false);
	for (const MathSpan& ms : FindMath(mirror_)) {
		for (int k = 0; k < ms.open; k++) { syn[ms.from + k] = 0; syn[ms.to - 1 - k] = 0; }
		int i = ms.from + ms.open, end = ms.to - ms.open;
		while (i < end) {
			char16_t c = mirror_[i];
			int j = i + 1, cls = 4;
			if (c == u'\\') {
				while (j < end && ((mirror_[j] >= u'a' && mirror_[j] <= u'z') || (mirror_[j] >= u'A' && mirror_[j] <= u'Z'))) j++;
				if (j == i + 1 && j < end) j++;
				cls = 1;
			} else if ((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z')) cls = 2;
			else if (c >= u'0' && c <= u'9') cls = 3;
			for (int k = i; k < j; k++) { syn[k] = cls; ital[k] = true; }
			i = j;
		}
	}
	const Rgb synCol[5] = {t.dollar, t.cmd, t.var, t.num, t.text};
	for (int i = 0; i < n;) {
		int j = i + 1;
		auto key = [&](int k) { return std::make_tuple(fg[k], syn[k], (bool)ital[k], back[k].r, back[k].g, back[k].b); };
		while (j < n && key(j) == key(i)) j++;
		Rgb col = fg[i] >= 0 ? (fg[i] == 3 ? t.mismatch : t.bracket[fg[i]]) : syn[i] >= 0 ? synCol[syn[i]] : t.text;
		if (!Same(col, t.text)) fmt(i, j, tag("f", col).c_str());
		if (ital[i] && fg[i] < 0) fmt(i, j, "it");
		if (!Same(back[i], t.bg)) fmt(i, j, tag("b", back[i]).c_str());
		i = j;
	}
	GtkTextIter ins = Iter(head_), bound = Iter(anchor_);
	gtk_text_buffer_select_range(buf_, &ins, &bound);
	gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(text_), gtk_text_buffer_get_insert(buf_));
	applying_ = was;
	UpdateImCursor();
}

void Box::UpdateImCursor() {
	if (!gtk_widget_get_realized(text_)) return;
	GtkTextIter it;
	gtk_text_buffer_get_iter_at_mark(buf_, &it, gtk_text_buffer_get_insert(buf_));
	GdkRectangle r;
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(text_), &it, &r);
	int x, y;
	gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(text_), GTK_TEXT_WINDOW_WIDGET, r.x, r.y, &x, &y);
	r.x = x;
	r.y = y;
	gtk_im_context_set_cursor_location(im_, &r);   // 候选窗跟着光标
}

// 控件被直接改动（拖放、鼠标中键粘贴等不经过引擎的修改）：把差异交给引擎，同 Windows 的 SyncFromControl
void Box::SyncFromBuffer() {
	if (!preedit_.empty() || !visible_) return;
	GtkTextIter s, e;
	gtk_text_buffer_get_bounds(buf_, &s, &e);
	char* t = gtk_text_buffer_get_text(buf_, &s, &e, TRUE);
	std::u16string now = FromUtf8(t);
	g_free(t);
	if (now == mirror_) return;
	size_t p = 0, oldN = mirror_.size(), newN = now.size();
	while (p < oldN && p < newN && mirror_[p] == now[p]) p++;
	size_t q = 0;
	while (q < oldN - p && q < newN - p && mirror_[oldN - 1 - q] == now[newN - 1 - q]) q++;
	eng_->SetSelection((int)p, (int)(oldN - q));
	EditResult r = eng_->InsertText(now.substr(p, newN - p - q));
	mirror_ = now;
	Apply(r);
}

void Box::OnChanged(GtkTextBuffer*, Box* self) {
	if (self->applying_ || !self->visible_ || self->syncId_) return;
	self->syncId_ = g_idle_add([](gpointer p) -> gboolean {
		Box* self = static_cast<Box*>(p);
		self->syncId_ = 0;
		self->SyncFromBuffer();
		return G_SOURCE_REMOVE;
	}, self);
}

void Box::OnMarkSet(GtkTextBuffer* buf, const GtkTextIter*, GtkTextMark* mark, Box* self) {
	if (self->applying_ || !self->visible_ || !self->preedit_.empty()) return;
	if (mark != gtk_text_buffer_get_insert(buf) && mark != gtk_text_buffer_get_selection_bound(buf)) return;
	GtkTextIter i, b;
	gtk_text_buffer_get_iter_at_mark(buf, &i, gtk_text_buffer_get_insert(buf));
	gtk_text_buffer_get_iter_at_mark(buf, &b, gtk_text_buffer_get_selection_bound(buf));
	int head = self->U16(gtk_text_iter_get_offset(&i)), anchor = self->U16(gtk_text_iter_get_offset(&b));
	if (head == self->head_ && anchor == self->anchor_) return;
	EditResult r = self->eng_->SetSelection(anchor, head);   // 鼠标改了选区
	if (r.ok) { self->anchor_ = r.anchor; self->head_ = r.head; self->Restyle(); }
}

// ---------------- 按键与输入法 ----------------
void Box::Key(const char* name, bool shift, bool ctrl) {
	EditResult r = eng_->Key(name, shift, ctrl, false);
	if (Trace()) Log("跟踪：引擎 %s shift=%d -> handled=%d commit=%d cancel=%d head=%d", name, (int)shift, (int)r.handled, (int)r.commit, (int)r.cancel, r.head);
	Apply(r);
	if (r.commit) Commit(CommitMode::Latex);
	else if (r.cancel) Cancel();
}

void Box::Typed(const std::string& utf8, bool shift) { Key(utf8.c_str(), shift, false); }

gboolean Box::OnKey(GtkEventControllerKey*, guint kv, guint, GdkModifierType state, Box* self) {
	if (!self->visible_) return TRUE;
	bool ctrl = state & GDK_CONTROL_MASK, shift = state & GDK_SHIFT_MASK, alt = state & GDK_ALT_MASK;
	self->lastShift_ = shift;
	if (Trace()) Log("跟踪：KEY 0x%x ctrl=%d shift=%d alt=%d 输入法组字=%d", kv, (int)ctrl, (int)shift, (int)alt, (int)!self->preedit_.empty());
	if (!self->preedit_.empty()) return TRUE;
	switch (kv) {
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_ISO_Enter:
		if (ctrl && shift && !alt) self->Commit(CommitMode::MathML);
		else if (alt && !ctrl && !shift) self->Commit(CommitMode::Keep);      // 保留外层 $（Typora / Obsidian）
		else if (ctrl && !shift && !alt) self->Commit(CommitMode::Unicode);   // Unicode
		else self->Key("Enter", shift, false);                               // 插件先处理（矩阵换行等），没处理就上屏；Shift+Enter 换行
		return TRUE;
	case GDK_KEY_Tab: case GDK_KEY_KP_Tab: self->Key("Tab", shift, ctrl); return TRUE;
	case GDK_KEY_ISO_Left_Tab: self->Key("Tab", true, ctrl); return TRUE;
	case GDK_KEY_BackSpace: self->Key("Backspace", shift, ctrl); return TRUE;
	case GDK_KEY_Delete: case GDK_KEY_KP_Delete: self->Key("Delete", shift, ctrl); return TRUE;
	case GDK_KEY_Escape: self->Key("Escape", shift, ctrl); return TRUE;
	case GDK_KEY_Left: case GDK_KEY_KP_Left: self->Key("ArrowLeft", shift, ctrl); return TRUE;
	case GDK_KEY_Right: case GDK_KEY_KP_Right: self->Key("ArrowRight", shift, ctrl); return TRUE;
	case GDK_KEY_Up: case GDK_KEY_KP_Up: self->Key("ArrowUp", shift, ctrl); return TRUE;
	case GDK_KEY_Down: case GDK_KEY_KP_Down: self->Key("ArrowDown", shift, ctrl); return TRUE;
	case GDK_KEY_Home: case GDK_KEY_KP_Home: self->Key("Home", shift, ctrl); return TRUE;
	case GDK_KEY_End: case GDK_KEY_KP_End: self->Key("End", shift, ctrl); return TRUE;
	}
	if (ctrl && !alt) {
		switch (gdk_keyval_to_lower(kv)) {
		case GDK_KEY_z: self->Key("z", shift, true); return TRUE;
		case GDK_KEY_y: self->Key("y", shift, true); return TRUE;
		case GDK_KEY_a: self->Apply(self->eng_->SetSelection(0, (int)self->mirror_.size())); return TRUE;
		case GDK_KEY_c:
		case GDK_KEY_x: {
			int a = std::min(self->anchor_, self->head_), b = std::max(self->anchor_, self->head_);
			if (a < b) {
				gdk_clipboard_set_text(gtk_widget_get_clipboard(self->win_), ToUtf8(self->mirror_.substr(a, b - a)).c_str());
				if (gdk_keyval_to_lower(kv) == GDK_KEY_x) self->Apply(self->eng_->InsertText(u""));
			}
			return TRUE;
		}
		case GDK_KEY_v:
			gdk_clipboard_read_text_async(gtk_widget_get_clipboard(self->win_), nullptr, [](GObject* src, GAsyncResult* res, gpointer p) {
				Box* self = static_cast<Box*>(p);
				char* t = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(src), res, nullptr);
				if (t && *t && self->visible_) {
					std::u16string s = FromUtf8(t);
					s.erase(std::remove(s.begin(), s.end(), u'\r'), s.end());
					self->Apply(self->eng_->Paste(s));
				}
				g_free(t);
			}, self);
			return TRUE;
		}
		return TRUE;   // 其余 Ctrl 组合键不交给控件
	}
	if (alt) return TRUE;
	gunichar u = gdk_keyval_to_unicode(kv);
	if (u >= 0x20 && u != 0x7F) {
		char b[8] = {};
		g_unichar_to_utf8(u, b);
		self->Typed(b, shift);
	}
	return TRUE;
}

void Box::OnCommit(GtkIMContext*, const char* str, Box* self) {
	if (!self->visible_ || !str || !*str) return;
	if (!self->preedit_.empty()) { self->preedit_.clear(); self->ShowPreedit(); }
	if (Trace()) Log("跟踪：输入法上屏 [%s]", str);
	if (g_utf8_strlen(str, -1) == 1) self->Typed(str, self->lastShift_);   // 单个字符当作按键：片段、自动括号照常触发
	else self->Apply(self->eng_->InsertText(FromUtf8(str)));
}

void Box::OnPreeditChanged(GtkIMContext* im, Box* self) {
	char* s = nullptr;
	gtk_im_context_get_preedit_string(im, &s, nullptr, nullptr);
	self->preedit_ = s ? s : "";
	g_free(s);
	self->ShowPreedit();
}

// 组字中的拼音临时插在光标处（带下划线），不进引擎
void Box::ShowPreedit() {
	static int start = -1, len = 0;
	applying_ = true;
	if (start >= 0 && len > 0) {
		GtkTextIter a, b;
		gtk_text_buffer_get_iter_at_offset(buf_, &a, start);
		gtk_text_buffer_get_iter_at_offset(buf_, &b, start + len);
		gtk_text_buffer_delete(buf_, &a, &b);
	}
	start = -1;
	len = 0;
	if (!preedit_.empty()) {
		start = Cp(head_);
		len = (int)g_utf8_strlen(preedit_.c_str(), -1);
		GtkTextIter a;
		gtk_text_buffer_get_iter_at_offset(buf_, &a, start);
		gtk_text_buffer_insert_with_tags_by_name(buf_, &a, preedit_.c_str(), -1, "preedit", nullptr);
	} else {
		GtkTextIter ins = Iter(head_), bound = Iter(anchor_);
		gtk_text_buffer_select_range(buf_, &ins, &bound);
	}
	applying_ = false;
}

// ---------------- 预览与排版 ----------------
void Box::UpdatePreview() {
	ClearItems();
	std::u16string packed;
	std::string err;
	if (!eng_->CallString("LatexAnywhereMath", "previewPacked", mirror_, &packed, &err)) { Log("预览失败：%s", err.c_str()); return; }
	const float fontPx = 17, exPx = fontPx * kExPerEm;
	const std::string col = Hex(g_theme.text);
	PangoFontDescription* fd = pango_font_description_new();
	pango_font_description_set_absolute_size(fd, fontPx * 0.85 * PANGO_SCALE);
	size_t i = 0;
	while (i < packed.size()) {
		size_t e = packed.find(u'\x1e', i);
		if (e == std::u16string::npos) e = packed.size();
		std::u16string rec = packed.substr(i, e - i);
		i = e + 1;
		if (rec.size() < 2 || rec[1] != u'\x1f') continue;
		auto* it = new Item;
		if (rec[0] == u'm') {
			std::string svg = ToUtf8(rec.substr(2));
			float wex = 0, hex = 0, va = 0;
			if (!AttrEx(svg, "width", &wex) || !AttrEx(svg, "height", &hex)) { delete it; continue; }
			size_t vp = svg.find("vertical-align:");
			if (vp != std::string::npos) va = std::strtof(svg.c_str() + vp + 15, nullptr);   // 负数：基线以下的深度
			it->math = true;
			it->w = wex * exPx;
			it->h = hex * exPx;
			it->baseline = it->h + va * exPx;
			std::string doc = PrepareSvg(svg, it->w, it->h, col);
			GError* ge = nullptr;
			it->svg = rsvg_handle_new_from_data(reinterpret_cast<const guint8*>(doc.data()), doc.size(), &ge);
			if (!it->svg) { Log("预览：SVG 解析失败（%zu 字节）：%s", doc.size(), ge ? ge->message : "?"); g_clear_error(&ge); delete it; continue; }
		} else {
			it->error = rec[0] == u'e';
			std::string t = ToUtf8(rec.substr(2));
			it->text = gtk_widget_create_pango_layout(preview_, t.c_str());
			pango_layout_set_font_description(it->text, fd);
			int w, h;
			pango_layout_get_pixel_size(it->text, &w, &h);
			it->w = (float)w;
			it->h = (float)h;
			it->baseline = pango_layout_get_baseline(it->text) / (float)PANGO_SCALE;
		}
		items_.push_back(it);
	}
	pango_font_description_free(fd);
}

void Box::Relayout() {
	GdkDisplay* d = gdk_display_get_default();
	int monW = 1920;
	GdkSurface* surf = gtk_widget_get_realized(win_) ? gtk_native_get_surface(GTK_NATIVE(win_)) : nullptr;
	GdkMonitor* mon = surf ? gdk_display_get_monitor_at_surface(d, surf) : nullptr;
	if (!mon) {
		GListModel* ms = gdk_display_get_monitors(d);
		if (g_list_model_get_n_items(ms) > 0) { mon = GDK_MONITOR(g_list_model_get_item(ms, 0)); g_object_unref(mon); }
	}
	if (mon) { GdkRectangle g; gdk_monitor_get_geometry(mon, &g); monW = g.width; }
	const int maxW = std::min((int)(monW * 0.4), 560), pad = 6;
	// 编辑区自然宽度：最长一行
	int natural = 24;
	size_t i = 0;
	while (i <= mirror_.size()) {
		size_t e = mirror_.find(u'\n', i);
		if (e == std::u16string::npos) e = mirror_.size();
		PangoLayout* l = gtk_widget_create_pango_layout(text_, ToUtf8(mirror_.substr(i, e - i)).c_str());
		int w, h;
		pango_layout_get_pixel_size(l, &w, &h);
		g_object_unref(l);
		natural = std::max(natural, (int)(w * 1.08) + 14);   // 斜体略宽
		i = e + 1;
	}
	// 预览：按行排，放不下就换行；每行按基线对齐（同 Windows 版）
	const float maxPv = (float)(maxW - 2 * pad - 8), fontPx = 17, gap = fontPx * 0.15f;
	float y = 0, width = 0;
	for (Item* it : items_) it->scale = (it->w > maxPv && it->w > 0) ? std::max(0.35f, maxPv / it->w) : 1.f;   // 单独一段仍比框宽：只把这一段缩小
	size_t a = 0;
	while (a < items_.size()) {
		size_t b = a;
		float lineW = 0, asc = 0, desc = 0;
		while (b < items_.size()) {
			Item* it = items_[b];
			float w = it->w * it->scale;
			if (b > a && lineW + gap + w > maxPv) break;
			lineW += (b > a ? gap : 0) + w;
			asc = std::max(asc, it->baseline * it->scale);
			desc = std::max(desc, (it->h - it->baseline) * it->scale);
			b++;
		}
		width = std::max(width, lineW);
		float x = (maxPv - lineW) / 2;
		for (size_t k = a; k < b; k++) {
			Item* it = items_[k];
			it->x = x;
			it->y = y + asc - it->baseline * it->scale;
			x += it->w * it->scale + gap;
		}
		y += asc + desc + fontPx * 0.2f;
		a = b;
	}
	float shift = (maxPv - width) / 2;   // 内容比可用宽度窄：整体左移，贴合内容
	for (Item* it : items_) it->x -= shift;
	pvW_ = (int)std::ceil(width);
	pvH_ = items_.empty() ? 0 : (int)std::ceil(y);

	int w = std::clamp(std::max(natural, pvW_ + 8) + 2 * pad, 60, maxW);
	int editW = w - 2 * pad;
	gtk_widget_set_size_request(editScroll_, editW, -1);
	// 编辑区高度按实际宽度折行后算出来，精确设定（滚动区自己估的偏高，框下面会多一截空白）；超过 240 才出滚动条
	{
		std::string all = ToUtf8(mirror_);
		if (all.empty() || all.back() == '\n') all += " ";
		PangoLayout* l = gtk_widget_create_pango_layout(text_, all.c_str());
		pango_layout_set_width(l, (editW - 4) * PANGO_SCALE);
		pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
		int lw, lh;
		pango_layout_get_pixel_size(l, &lw, &lh);
		g_object_unref(l);
		// 放得下就关掉竖向滚动：开着时滚动区至少留滚动条的高度，框下面会多一截空白
		bool scroll = lh + 2 > 240;
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editScroll_), GTK_POLICY_NEVER, scroll ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
		gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(editScroll_), scroll ? 240 : -1);
	}
	gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(preview_), std::max(1, pvW_));
	gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(preview_), std::max(1, pvH_));
	gtk_widget_set_size_request(previewScroll_, editW, -1);
	bool pvScroll = pvH_ > 200;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(previewScroll_), GTK_POLICY_NEVER, pvScroll ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(previewScroll_), pvScroll ? 200 : -1);
	gtk_widget_set_visible(previewScroll_, !items_.empty());
	gtk_widget_set_visible(sep_, !items_.empty());
	gtk_widget_queue_draw(preview_);
	gtk_window_set_default_size(GTK_WINDOW(win_), 1, 1);   // 内容变少时窗口跟着缩小
}

void Box::DrawPreview(GtkDrawingArea*, cairo_t* cr, int w, int, gpointer p) {
	Box* self = static_cast<Box*>(p);
	float ox = std::max(0.f, (w - self->pvW_) / 2.f);
	for (Item* it : self->items_) {
		if (it->math && it->svg) {
			RsvgRectangle vp{ox + it->x, it->y, it->w * it->scale, it->h * it->scale};
			rsvg_handle_render_document(it->svg, cr, &vp, nullptr);
		} else if (it->text) {
			Rgb c = it->error ? Rgb{0xfa, 0x45, 0x4c} : g_theme.text;
			cairo_save(cr);
			cairo_translate(cr, ox + it->x, it->y);
			cairo_scale(cr, it->scale, it->scale);
			cairo_set_source_rgb(cr, c.r / 255.0, c.g / 255.0, c.b / 255.0);
			pango_cairo_show_layout(cr, it->text);
			cairo_restore(cr);
		}
	}
}

}  // namespace la
