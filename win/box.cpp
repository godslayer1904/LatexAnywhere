#include "box.h"

#include <dwmapi.h>
#include <imm.h>
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "sysio.h"
#include "util.h"

using Microsoft::WRL::ComPtr;

namespace la {
namespace {

constexpr UINT WM_APP_PREVIEW = WM_APP + 1;
constexpr UINT WM_APP_SYNC = WM_APP + 2;
constexpr UINT WM_APP_IDLE = WM_APP + 3;
constexpr wchar_t kBoxClass[] = L"LatexAnywhereBox";

// ---------- 后台动作线程：上屏、取消、切输入法都在这里依次执行（含等待），不卡界面 ----------
class Worker {
public:
	Worker() : th_([this] { Run(); }) {}
	void Post(std::function<void()> f) {
		std::lock_guard<std::mutex> lk(mu_);
		q_.push_back(std::move(f));
		cv_.notify_one();
	}
	// 输入法切换只执行最新的一个（连按开关时不叠加，原型 10-24 的教训）
	void PostIme(std::function<void()> f) {
		std::lock_guard<std::mutex> lk(mu_);
		ime_ = std::move(f);
		cv_.notify_one();
	}
private:
	void Run() {
		for (;;) {
			std::function<void()> f;
			{
				std::unique_lock<std::mutex> lk(mu_);
				cv_.wait(lk, [&] { return !q_.empty() || ime_; });
				if (!q_.empty()) { f = std::move(q_.front()); q_.pop_front(); }
				else { f = std::move(ime_); ime_ = nullptr; }
			}
			try { f(); } catch (...) { Log("后台动作异常"); }
		}
	}
	std::mutex mu_;
	std::condition_variable cv_;
	std::deque<std::function<void()>> q_;
	std::function<void()> ime_;
	std::thread th_;
};
Worker& Bg() { static Worker* w = new Worker; return *w; }   // 进程结束时随进程回收

// ---------- 配色（Obsidian / Latex Suite 的颜色） ----------
struct Theme {
	COLORREF bg, text, border, cmd, var, num, dollar, sel;
	COLORREF bracket[3], mismatch, highlight, ph[3];
};
COLORREF Blend(COLORREF fg, double a, COLORREF bg) {
	auto m = [&](int f, int b) { return (int)(f * a + b * (1 - a) + .5); };
	return RGB(m(GetRValue(fg), GetRValue(bg)), m(GetGValue(fg), GetGValue(bg)), m(GetBValue(fg), GetBValue(bg)));
}
Theme MakeTheme(bool dark) {
	Theme t;
	if (dark) {
		t.bg = RGB(0x1e, 0x1e, 0x1e); t.text = RGB(0xda, 0xda, 0xda); t.border = RGB(0x36, 0x36, 0x36);
		t.cmd = RGB(0xfb, 0x46, 0x4c); t.var = RGB(0x53, 0xdf, 0xdd); t.num = RGB(0xa8, 0x82, 0xff); t.dollar = RGB(0x44, 0xcf, 0x6e);
		t.bracket[0] = RGB(0x47, 0xb8, 0xff); t.bracket[1] = RGB(0xff, 0x55, 0xcd); t.bracket[2] = RGB(0x73, 0xff, 0x63);
		t.highlight = Blend(RGB(0xa5, 0x8f, 0xf4), 0.6, t.bg);   // hsl(254,80%,70%,.6)
	} else {
		t.bg = RGB(0xff, 0xff, 0xff); t.text = RGB(0x22, 0x22, 0x22); t.border = RGB(0xdd, 0xdd, 0xdd);
		t.cmd = RGB(0xe9, 0x31, 0x47); t.var = RGB(0x00, 0xa3, 0xa0); t.num = RGB(0x8a, 0x5c, 0xf5); t.dollar = RGB(0x08, 0xb9, 0x4e);
		t.bracket[0] = RGB(0x52, 0x7a, 0xff); t.bracket[1] = RGB(0xff, 0x50, 0xb7); t.bracket[2] = RGB(0x69, 0xba, 0x00);
		t.highlight = Blend(RGB(0x3b, 0x14, 0xb8), 0.3, t.bg);   // hsl(254,80%,40%,.3)
	}
	t.mismatch = Blend(RGB(255, 18, 18), 0.8, t.bg);
	t.ph[0] = Blend(RGB(0x87, 0xce, 0xfa), 0x2e / 255.0, t.bg);
	t.ph[1] = Blend(RGB(0xff, 0xa5, 0x00), 0x33 / 255.0, t.bg);
	t.ph[2] = Blend(RGB(0x00, 0xff, 0x00), 0x22 / 255.0, t.bg);
	t.sel = Blend(RGB(137, 108, 255), 0.33, t.bg);
	return t;
}
D2D1_COLOR_F ToD2D(COLORREF c) { return D2D1::ColorF(GetRValue(c) / 255.f, GetGValue(c) / 255.f, GetBValue(c) / 255.f); }

std::wstring W(const std::u16string& s) { return std::wstring(s.begin(), s.end()); }
std::u16string U(const std::wstring& s) { return std::u16string(s.begin(), s.end()); }

// 公式区段：$$…$$ 与 $…$（与预览、原型相同的规则）
struct MathSpan { int from, to; int open; };   // open：定界符长度
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

}  // namespace

static Theme g_theme = MakeTheme(true);
// 处理按键消息时读「这条消息发生时」的修饰键状态（与消息队列同步）；GetAsyncKeyState 读的是此刻状态，快速输入时会错位
static bool Held(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }
static bool Trace() { static bool on = GetEnvironmentVariableW(L"LA_TRACE", nullptr, 0) > 0; return on; }   // 测试用：设了 LA_TRACE 就记录每个按键

bool Box::Create(HINSTANCE inst, Engine* engine) {
	inst_ = inst;
	eng_ = engine;
	LoadLibraryW(L"Msftedit.dll");
	WNDCLASSEXW wc{sizeof wc};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursor(nullptr, IDC_SIZEALL);
	wc.lpszClassName = kBoxClass;
	RegisterClassExW(&wc);
	hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kBoxClass, L"LatexAnywhere", WS_POPUP | WS_CLIPCHILDREN,
		0, 0, 200, 60, nullptr, nullptr, inst, this);
	if (!hwnd_) return false;
	DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUNDSMALL;
	DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof pref);
	scale_ = GetDpiForWindow(hwnd_) / 96.f;

	edit_ = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL,
		0, 0, 100, 20, hwnd_, nullptr, inst, nullptr);
	if (!edit_) return false;
	SetWindowSubclass(edit_, EditProc, 1, (DWORD_PTR)this);
	SendMessageW(edit_, EM_SETUNDOLIMIT, 0, 0);                  // 撤销由引擎（原版 CodeMirror 历史）负责
	SendMessageW(edit_, EM_AUTOURLDETECT, FALSE, 0);
	SendMessageW(edit_, EM_SETTARGETDEVICE, 0, 0);               // 按窗口宽度折行
	LRESULT lo = SendMessageW(edit_, EM_GETLANGOPTIONS, 0, 0);
	SendMessageW(edit_, EM_SETLANGOPTIONS, 0, lo & ~(IMF_AUTOKEYBOARD | IMF_AUTOFONT));
	SendMessageW(edit_, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_REQUESTRESIZE);

	D2D1_FACTORY_OPTIONS fo{};
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &fo, &d2d_);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dw_.GetAddressOf()));
	CreateDeviceResources();
	SetDark(dark_);
	return true;
}

void Box::SetDark(bool dark) {
	dark_ = dark;
	g_theme = MakeTheme(dark);
	BOOL d = dark;
	DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &d, sizeof d);
	if (font_) DeleteObject(font_);
	font_ = CreateFontW(-Px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
	SendMessageW(edit_, WM_SETFONT, (WPARAM)font_, FALSE);
	SendMessageW(edit_, EM_SETBKGNDCOLOR, 0, g_theme.bg);
	if (visible_) { Restyle(); previewDirty_ = true; UpdatePreview(); }
}

void Box::CreateDeviceResources() {
	if (rt_) return;
	RECT rc;
	GetClientRect(hwnd_, &rc);
	D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.f, 96.f);
	if (FAILED(d2d_->CreateHwndRenderTarget(p, D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(rc.right, rc.bottom)), &rt_))) {
		Log("Direct2D 渲染目标创建失败");
		return;
	}
	if (FAILED(rt_.As(&dc_))) Log("系统不支持 Direct2D SVG（需要 Windows 10 1703 以上）");
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
	prev_ = GetForegroundWindow();
	prevNative_ = ImeIsNative(ImmGetDefaultIMEWnd(FocusWindowOf(prev_)));
	caret_ = CaretScreenPos();
	double tCaret = NowMs() - t0;
	std::u16string sel = U(GrabSelection());
	double tSel = NowMs() - t0;
	// 一律放进行内公式里编辑（上屏时去掉外层 $）：Latex Suite 只在公式里生效
	EditResult r = eng_->Reset(u"$" + sel + u"$", 1, 1 + (int)sel.size());
	if (!r.ok) { Log("引擎出错，不弹框：%s", r.error.c_str()); return; }
	scale_ = GetDpiForWindow(hwnd_) / 96.f;
	moved_ = false;
	placed_ = {INT_MIN, INT_MIN};
	applying_ = true;
	SetWindowTextW(edit_, L"");   // 控件与镜像一起清空，再按引擎结果写入
	applying_ = false;
	mirror_.clear();
	preview_.Reset();
	Apply(r);
	previewDirty_ = true;
	UpdatePreview();
	visible_ = true;
	Relayout();
	ShowWindow(hwnd_, SW_SHOWNORMAL);
	FocusBox();
	Log("弹框：光标 %.0fms，选区 %.0fms，显示 %.0fms（选区长度 %zu）", tCaret, tSel, NowMs() - t0, sel.size());
	HWND box = hwnd_, ed = edit_;
	Bg().PostIme([box, ed] { ImeSwitchToEnglish(box, ed); });
}

void Box::FocusBox() {
	if (!SetForegroundWindow(hwnd_) || GetForegroundWindow() != hwnd_) {
		// 借前台线程的输入队列绕开抢焦点限制（原型同款做法）
		SendKeys({{0, false}, {0, true}});
		DWORD t1 = GetWindowThreadProcessId(GetForegroundWindow(), nullptr), t2 = GetCurrentThreadId();
		AttachThreadInput(t1, t2, TRUE);
		SetForegroundWindow(hwnd_);
		AttachThreadInput(t1, t2, FALSE);
	}
	SetFocus(edit_);
}

void Box::Hide() {
	visible_ = false;
	ShowWindow(hwnd_, SW_HIDE);
	PostMessageW(hwnd_, WM_APP_IDLE, 0, 0);   // 上屏/取消的动作先交出去，空闲了再回收
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
	Hide();
	HWND prev = prev_;
	bool restoreIme = prevNative_;
	std::wstring payload = W(text);
	Bg().Post([prev, restoreIme, payload, raw] {
		if (prev) { SetForegroundWindow(prev); Sleep(80); }
		if (!payload.empty()) {
			std::wstring exe = ForegroundExeLower();
			bool office = exe == L"winword.exe" || exe == L"powerpnt.exe";
			auto leave = [exe] {
				// 插入公式后光标还在公式里，下一次粘贴会并进同一个公式：先把光标移出公式，再打空格隔开
				// Word：按 → 即可移出；PowerPoint：光标会落在公式中间，要按 End（实测）
				WORD k = exe == L"powerpnt.exe" ? VK_END : VK_RIGHT;
				SendKeys({{k, false}, {k, true}});
				Sleep(50);
				SendKeys({{VK_SPACE, false}, {VK_SPACE, true}});
			};
			PasteText(payload, (office && raw) ? 1200 : 300, (office && raw) ? std::function<void()>(leave) : nullptr);
		}
		if (restoreIme) ImeRestoreNative(prev);
	});
}

void Box::Cancel() {
	Hide();
	HWND prev = prev_;
	bool restoreIme = prevNative_;
	Bg().Post([prev, restoreIme] {
		if (prev) { SetForegroundWindow(prev); Sleep(80); }
		if (restoreIme) ImeRestoreNative(prev);
	});
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
		std::wstring mid = W(r.doc.substr(p, newN - p - s));
		std::replace(mid.begin(), mid.end(), L'\n', L'\r');
		SendMessageW(edit_, EM_SETSEL, p, oldN - s);
		SendMessageW(edit_, EM_REPLACESEL, FALSE, (LPARAM)mid.c_str());
		mirror_ = r.doc;
		previewDirty_ = true;
		PostMessageW(hwnd_, WM_APP_PREVIEW, 0, 0);   // 预览放到按键处理之后再画，不挡打字
	}
	anchor_ = r.anchor;
	head_ = r.head;
	Restyle();
	applying_ = false;
}

void Box::Restyle() {
	const Theme& t = g_theme;
	DWORD mask = (DWORD)SendMessageW(edit_, EM_SETEVENTMASK, 0, 0);
	SendMessageW(edit_, WM_SETREDRAW, FALSE, 0);
	CHARFORMAT2W cf{};
	cf.cbSize = sizeof cf;
	auto fmt = [&](int a, int b, COLORREF color, bool italic, COLORREF back) {
		if (a >= b) return;
		cf.dwMask = CFM_COLOR | CFM_ITALIC | CFM_BACKCOLOR;
		cf.dwEffects = italic ? CFE_ITALIC : 0;
		cf.crTextColor = color;
		cf.crBackColor = back;
		SendMessageW(edit_, EM_SETSEL, a, b);
		SendMessageW(edit_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	};
	const int n = (int)mirror_.size();
	fmt(0, n, t.text, false, t.bg);
	std::vector<COLORREF> back(n, t.bg);
	std::vector<int> fg(n, -1);   // -1：按语法着色；否则为 Latex Suite 标记给的颜色
	std::vector<bool> isBracket(n, false);
	for (const Mark& m : eng_->Marks()) {
		int a = std::max(0, m.from), b = std::min(n, m.to);
		for (int i = a; i < b; i++) {
			const std::string& c = m.cls;
			if (c.find("latex-suite-snippet-placeholder") != std::string::npos) {
				int k = c.back() - '0';
				back[i] = t.ph[(k >= 0 && k < 3) ? k : 0];
			}
			if (c.find("latex-suite-highlighted-bracket") != std::string::npos) back[i] = t.highlight;
			if (c.find("latex-suite-mismatched-bracket") != std::string::npos) { fg[i] = (int)t.mismatch; isBracket[i] = true; }
			size_t cb = c.find("latex-suite-color-bracket-");
			if (cb != std::string::npos) { int k = c[cb + 26] - '0'; fg[i] = (int)t.bracket[(k >= 0 && k < 3) ? k : 0]; isBracket[i] = true; }
		}
	}
	// 公式内着色：命令红、字母青、数字紫、$ 绿、斜体（与原型一致）
	for (const MathSpan& ms : FindMath(mirror_)) {
		fmt(ms.from, ms.from + ms.open, t.dollar, false, t.bg);
		fmt(ms.to - ms.open, ms.to, t.dollar, false, t.bg);
		int i = ms.from + ms.open, e = ms.to - ms.open;
		while (i < e) {
			char16_t c = mirror_[i];
			int j = i + 1;
			COLORREF col = t.text;
			if (c == u'\\') {
				while (j < e && ((mirror_[j] >= u'a' && mirror_[j] <= u'z') || (mirror_[j] >= u'A' && mirror_[j] <= u'Z'))) j++;
				if (j == i + 1 && j < e) j++;
				col = t.cmd;
			} else if ((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z')) {
				col = t.var;
			} else if (c >= u'0' && c <= u'9') {
				col = t.num;
			}
			fmt(i, j, col, true, t.bg);
			i = j;
		}
	}
	// 括号颜色与背景（标签、光标所在括号）
	for (int i = 0; i < n;) {
		int j = i + 1;
		while (j < n && fg[j] == fg[i] && back[j] == back[i]) j++;
		if (fg[i] >= 0) fmt(i, j, (COLORREF)fg[i], false, back[i]);
		else if (back[i] != t.bg) {
			cf.dwMask = CFM_BACKCOLOR;
			cf.crBackColor = back[i];
			SendMessageW(edit_, EM_SETSEL, i, j);
			SendMessageW(edit_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
		}
		i = j;
	}
	SendMessageW(edit_, EM_SETSEL, anchor_, head_);
	SendMessageW(edit_, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(edit_, nullptr, FALSE);
	SendMessageW(edit_, EM_SETEVENTMASK, 0, mask);
}

void Box::SyncFromControl() {
	GETTEXTLENGTHEX gl{GTL_DEFAULT | GTL_NUMCHARS, 1200};
	LONG len = (LONG)SendMessageW(edit_, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
	std::wstring buf(len + 1, L'\0');
	GETTEXTEX gt{(DWORD)((len + 1) * sizeof(wchar_t)), GT_DEFAULT, 1200, nullptr, nullptr};
	LRESULT got = SendMessageW(edit_, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buf.data());
	buf.resize(got);
	std::replace(buf.begin(), buf.end(), L'\r', L'\n');
	std::u16string now = U(buf);
	if (now == mirror_) return;
	size_t p = 0, oldN = mirror_.size(), newN = now.size();
	while (p < oldN && p < newN && mirror_[p] == now[p]) p++;
	size_t s = 0;
	while (s < oldN - p && s < newN - p && mirror_[oldN - 1 - s] == now[newN - 1 - s]) s++;
	std::u16string ins = now.substr(p, newN - p - s);
	eng_->SetSelection((int)p, (int)(oldN - s));
	EditResult r = eng_->InsertText(ins);
	mirror_ = now;   // 控件已经是新内容：以它为基准算引擎结果的差异
	Apply(r);
}

// ---------------- 预览与排版 ----------------
void Box::UpdatePreview() {
	if (!previewDirty_ || !dc_) return;
	previewDirty_ = false;
	std::u16string packed;
	std::string err;
	std::vector<PreviewItem> items;
	if (eng_->CallString("LatexAnywhereMath", "previewPacked", mirror_, &packed, &err)) {
		size_t i = 0;
		while (i < packed.size()) {
			size_t e = packed.find(u'\x1e', i);
			if (e == std::u16string::npos) e = packed.size();
			std::u16string rec = packed.substr(i, e - i);
			if (rec.size() >= 2 && rec[1] == u'\x1f') {
				PreviewItem it;
				if (rec[0] == u'm') { it.math = true; it.svg = WideToUtf8(W(rec.substr(2))); }
				else { it.text = W(rec.substr(2)); it.error = rec[0] == u'e'; }
				items.push_back(std::move(it));
			}
			i = e + 1;
		}
	} else {
		Log("预览失败：%s", err.c_str());
	}
	preview_.SetItems(std::move(items));
	if (visible_) Relayout();
}

void Box::Relayout() {
	RECT wa = WorkAreaAt(caret_);
	const int maxW = std::min((int)((wa.right - wa.left) * 0.4), Px(560));
	const int pad = Px(6);
	// 编辑区自然宽度：最长一行
	int natural = Px(24);
	{
		HDC dc = GetDC(edit_);
		HGDIOBJ old = SelectObject(dc, font_);
		size_t i = 0;
		while (i <= mirror_.size()) {
			size_t e = mirror_.find(u'\n', i);
			if (e == std::u16string::npos) e = mirror_.size();
			std::wstring line = W(mirror_.substr(i, e - i));
			SIZE sz{};
			GetTextExtentPoint32W(dc, line.c_str(), (int)line.size(), &sz);
			natural = std::max(natural, (int)(sz.cx * 1.08) + Px(14));   // 斜体略宽
			i = e + 1;
		}
		SelectObject(dc, old);
		ReleaseDC(edit_, dc);
	}
	float pvW = 0, pvH = 0;
	if (dc_ && !preview_.Empty()) {
		D2D1_SIZE_F s = preview_.Layout(dc_.Get(), dw_.Get(), (float)(maxW - 2 * pad), (float)Px(17), ToD2D(g_theme.text));
		pvW = s.width;
		pvH = std::min(s.height, (float)Px(200));
	}
	int w = std::clamp(std::max(natural, (int)pvW + Px(8)) + 2 * pad, Px(60), maxW);
	int editW = w - 2 * pad;
	MoveWindow(edit_, pad, 0, editW, editH_, FALSE);
	SendMessageW(edit_, EM_REQUESTRESIZE, 0, 0);   // 回调 EN_REQUESTRESIZE 更新 editH_
	int editH = std::min(editH_, Px(240));
	int pvArea = preview_.Empty() ? 0 : (int)pvH + Px(8) + 1;
	int total = pad + pvArea + editH + pad;
	MoveWindow(edit_, pad, pad + pvArea, editW, editH, TRUE);

	int x, y;
	RECT cur;
	GetWindowRect(hwnd_, &cur);
	if (moved_) { x = cur.left; y = cur.top; }   // 用户拖动过：留在原位，只改大小
	else {
		x = std::max<int>(wa.left, std::min<int>(caret_.x - Px(12), wa.right - w));
		// 照 Latex Suite 预览气泡：默认在光标上方，上面放不下就翻到下方
		y = caret_.y - Px(28) - total;
		if (y < wa.top) y = caret_.y + Px(4);
		y = std::max<int>(wa.top, std::min<int>(y, wa.bottom - total));
	}
	SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, total, SWP_NOACTIVATE);
	placed_ = {x, y};
	InvalidateRect(hwnd_, nullptr, FALSE);
}

void Box::Paint() {
	PAINTSTRUCT ps;
	BeginPaint(hwnd_, &ps);
	CreateDeviceResources();
	if (rt_) {
		RECT rc;
		GetClientRect(hwnd_, &rc);
		rt_->BeginDraw();
		rt_->Clear(ToD2D(g_theme.bg));
		ComPtr<ID2D1SolidColorBrush> br;
		rt_->CreateSolidColorBrush(ToD2D(g_theme.border), &br);
		rt_->DrawRectangle(D2D1::RectF(0.5f, 0.5f, rc.right - 0.5f, rc.bottom - 0.5f), br.Get(), 1.f);
		if (!preview_.Empty() && dc_) {
			const float pad = (float)Px(6);
			preview_.Draw(dc_.Get(), D2D1::Point2F(pad + Px(4), pad + Px(4)));
			RECT er;
			GetWindowRect(edit_, &er);
			POINT p{er.left, er.top};
			ScreenToClient(hwnd_, &p);
			float ly = p.y - Px(4) + 0.5f;
			rt_->DrawLine(D2D1::Point2F(pad, ly), D2D1::Point2F(rc.right - pad, ly), br.Get(), 1.f);
		}
		if (rt_->EndDraw() == D2DERR_RECREATE_TARGET) { rt_.Reset(); dc_.Reset(); }
	}
	EndPaint(hwnd_, &ps);
}

// ---------------- 消息 ----------------
LRESULT CALLBACK Box::WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
	Box* self = reinterpret_cast<Box*>(GetWindowLongPtrW(h, GWLP_USERDATA));
	if (m == WM_NCCREATE) {
		self = static_cast<Box*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
		SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
		self->hwnd_ = h;
	}
	return self ? self->OnMessage(m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT Box::OnMessage(UINT m, WPARAM w, LPARAM l) {
	switch (m) {
	case WM_PAINT: Paint(); return 0;
	case WM_ERASEBKGND: return 1;
	case WM_SIZE:
		if (rt_) rt_->Resize(D2D1::SizeU(LOWORD(l), HIWORD(l)));
		return 0;
	case WM_NCHITTEST: {   // 文字以外的地方都能拖动整个框
		LRESULT r = DefWindowProcW(hwnd_, m, w, l);
		return r == HTCLIENT ? HTCAPTION : r;
	}
	case WM_EXITSIZEMOVE: moved_ = true; return 0;
	case WM_MOUSEACTIVATE: return MA_ACTIVATE;
	case WM_ACTIVATE:   // 框被重新激活（点了预览区、被别的程序还回焦点）：键盘焦点始终交给编辑框
		if (Trace()) Log("跟踪：框 WM_ACTIVATE %d", (int)LOWORD(w));
		if (LOWORD(w) != WA_INACTIVE && edit_) SetFocus(edit_);
		return 0;
	case WM_SETFOCUS:
		if (edit_) SetFocus(edit_);
		return 0;
	case WM_DPICHANGED: {
		scale_ = HIWORD(w) / 96.f;
		SetDark(dark_);
		Relayout();
		return 0;
	}
	case WM_APP_PREVIEW: UpdatePreview(); InvalidateRect(hwnd_, nullptr, FALSE); return 0;
	case WM_APP_SYNC: if (!composing_ && visible_) SyncFromControl(); return 0;
	case WM_APP_IDLE:
		if (!visible_) {
			double t0 = NowMs();
			eng_->CollectGarbage();
			Log("收起：回收 %.0fms，JS 内存 %.1f MB", NowMs() - t0, eng_->MemoryUsed() / 1048576.0);
		}
		return 0;
	case WM_COMMAND:
		if ((HWND)l == edit_ && HIWORD(w) == EN_CHANGE && !applying_ && !composing_ && visible_) PostMessageW(hwnd_, WM_APP_SYNC, 0, 0);
		return 0;
	case WM_NOTIFY: {
		auto* nh = reinterpret_cast<NMHDR*>(l);
		if (nh->hwndFrom != edit_) break;
		if (nh->code == EN_REQUESTRESIZE) {
			auto* rr = reinterpret_cast<REQRESIZE*>(l);
			editH_ = std::max(Px(18), (int)(rr->rc.bottom - rr->rc.top));
		} else if (nh->code == EN_SELCHANGE && !applying_ && !composing_ && visible_) {
			auto* sc = reinterpret_cast<SELCHANGE*>(l);
			if (sc->chrg.cpMin != std::min(anchor_, head_) || sc->chrg.cpMax != std::max(anchor_, head_)) {   // 鼠标改了选区
				EditResult r = eng_->SetSelection(sc->chrg.cpMin, sc->chrg.cpMax);
				if (r.ok) { anchor_ = r.anchor; head_ = r.head; Restyle(); }
			}
		}
		return 0;
	}
	case WM_SETTINGCHANGE:
		if (l && !lstrcmpW(reinterpret_cast<LPCWSTR>(l), L"ImmersiveColorSet")) PostMessageW(GetParent(hwnd_) ? GetParent(hwnd_) : hwnd_, WM_NULL, 0, 0);
		break;
	}
	return DefWindowProcW(hwnd_, m, w, l);
}

LRESULT CALLBACK Box::EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
	bool handled = false;
	LRESULT r = reinterpret_cast<Box*>(ref)->OnEditMessage(h, m, w, l, &handled);
	return handled ? r : DefSubclassProc(h, m, w, l);
}

LRESULT Box::OnEditMessage(HWND h, UINT m, WPARAM w, LPARAM l, bool* handled) {
	if (Trace() && (m == WM_KEYDOWN || m == WM_SYSKEYDOWN || m == WM_CHAR))
		Log("跟踪：%s vk=0x%02x 前台=%s 焦点=%s composing=%d", m == WM_CHAR ? "CHAR" : m == WM_KEYDOWN ? "KEYDOWN" : "SYSKEYDOWN", (unsigned)w,
			GetForegroundWindow() == hwnd_ ? "框" : "别处", GetFocus() == edit_ ? "编辑框" : "别处", (int)composing_);
	auto key = [&](const char* name, bool shift, bool ctrl) {
		EditResult r = eng_->Key(name, shift, ctrl, false);
		if (Trace()) Log("跟踪：引擎 %s shift=%d -> handled=%d commit=%d cancel=%d head=%d", name, (int)shift, (int)r.handled, (int)r.commit, (int)r.cancel, r.head);
		Apply(r);
		if (r.commit) Commit(CommitMode::Latex);
		else if (r.cancel) Cancel();
	};
	switch (m) {
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN: {
		if (composing_) return 0;
		bool ctrl = Held(VK_CONTROL), shift = Held(VK_SHIFT), alt = Held(VK_MENU);
		*handled = true;
		switch (w) {
		case VK_RETURN:
			if (ctrl && shift && !alt) Commit(CommitMode::MathML);        // Word / PowerPoint 原生公式
			else if (alt && !ctrl && !shift) Commit(CommitMode::Keep);    // 保留外层 $（Typora / Obsidian）
			else if (ctrl && !shift && !alt) Commit(CommitMode::Unicode); // Unicode
			else key("Enter", shift, false);                              // 插件先处理（矩阵换行等），没处理就上屏；Shift+Enter 换行
			return 0;
		case VK_TAB: key("Tab", shift, ctrl); return 0;
		case VK_BACK: key("Backspace", shift, ctrl); return 0;
		case VK_DELETE: key("Delete", shift, ctrl); return 0;
		case VK_ESCAPE: key("Escape", shift, ctrl); return 0;
		case VK_LEFT: key("ArrowLeft", shift, ctrl); return 0;
		case VK_RIGHT: key("ArrowRight", shift, ctrl); return 0;
		case VK_UP: key("ArrowUp", shift, ctrl); return 0;
		case VK_DOWN: key("ArrowDown", shift, ctrl); return 0;
		case VK_HOME: key("Home", shift, ctrl); return 0;
		case VK_END: key("End", shift, ctrl); return 0;
		}
		if (ctrl && !alt) {
			switch (w) {
			case 'Z': key("z", shift, true); return 0;
			case 'Y': key("y", shift, true); return 0;
			case 'A': { EditResult r = eng_->SetSelection(0, (int)mirror_.size()); Apply(r); return 0; }
			case 'C':
			case 'X': {
				int a = std::min(anchor_, head_), b = std::max(anchor_, head_);
				if (a < b) {
					ClipboardSetText(W(mirror_.substr(a, b - a)));
					if (w == 'X') Apply(eng_->InsertText(u""));
				}
				return 0;
			}
			case 'V': {
				bool ok = false;
				std::wstring t = ClipboardGetText(&ok);
				t.erase(std::remove(t.begin(), t.end(), L'\r'), t.end());
				if (ok && !t.empty()) Apply(eng_->Paste(U(t)));
				return 0;
			}
			}
			return 0;   // 其余 Ctrl 组合键不交给控件（避免 RichEdit 的对齐、行距等快捷键）
		}
		if (m == WM_SYSKEYDOWN) { *handled = false; return 0; }
		*handled = false;   // 字符键：交给系统生成 WM_CHAR
		return 0;
	}
	case WM_CHAR: {
		*handled = true;
		wchar_t c = (wchar_t)w;
		if (composing_) { *handled = false; return 0; }
		if (c < 0x20 || c == 0x7F) return 0;   // Tab/Enter/Backspace 等已在 WM_KEYDOWN 处理
		if (Held(VK_CONTROL) && !Held(VK_MENU)) return 0;
		if (IS_HIGH_SURROGATE(c)) { pendingHigh_ = c; return 0; }
		if (IS_LOW_SURROGATE(c)) {
			if (pendingHigh_) { std::u16string s{(char16_t)pendingHigh_, (char16_t)c}; pendingHigh_ = 0; Apply(eng_->InsertText(s)); }
			return 0;
		}
		std::string u8 = WideToUtf8(std::wstring(1, c));
		key(u8.c_str(), Held(VK_SHIFT), false);
		return 0;
	}
	case WM_IME_STARTCOMPOSITION: composing_ = true; break;
	case WM_IME_ENDCOMPOSITION: composing_ = false; PostMessageW(hwnd_, WM_APP_SYNC, 0, 0); break;
	case WM_PASTE: {
		*handled = true;
		bool ok = false;
		std::wstring t = ClipboardGetText(&ok);
		t.erase(std::remove(t.begin(), t.end(), L'\r'), t.end());
		if (ok && !t.empty()) Apply(eng_->Paste(U(t)));
		return 0;
	}
	case WM_CUT: case WM_UNDO: case EM_UNDO: case EM_REDO: case WM_CLEAR: case WM_CONTEXTMENU:
		*handled = true;
		return 0;
	}
	(void)h;
	return 0;
}

}  // namespace la
