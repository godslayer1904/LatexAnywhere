#include "help.h"

#include <d2d1_1.h>
#include <dwmapi.h>
#include <dwrite.h>
#include <uxtheme.h>
#include <wrl/client.h>
#include <algorithm>
#include <string>
#include <vector>

#include "i18n.h"
#include "resource.h"

using Microsoft::WRL::ComPtr;

namespace la {
namespace {

constexpr wchar_t kClass[] = L"LatexAnywhereHelp";
constexpr wchar_t kRegKey[] = L"Software\\LatexAnywhere";
constexpr int ID_CHECK = 201, ID_START = 202;

struct Palette { D2D1_COLOR_F bg, fg, muted, card, accent, border, keyBg; };
Palette MakePalette(bool dark) {
	auto c = [](UINT32 rgb) { return D2D1::ColorF(rgb); };
	if (dark) return {c(0x1e1e1e), c(0xdadada), c(0x9a9a9a), c(0x262626), c(0xa99cff), c(0x363636), c(0x1e1e1e)};
	return {c(0xffffff), c(0x222222), c(0x666666), c(0xf5f5f7), c(0x7f6df2), c(0xe2e2e6), c(0xffffff)};
}

enum class Kind { Title, Subtitle, H2, Numbered, Bullet, Note, TableHead, TableRow };

struct Item {
	Kind kind;
	std::wstring text[3];                       // 表格三列；其余只用 text[0]
	int number = 0;
	std::vector<DWRITE_TEXT_RANGE> keys[3];     // ‹…› 键帽范围（去掉括号后的位置）
	ComPtr<IDWriteTextLayout> layout[3];
	float y = 0, h = 0;
};

// 去掉 ‹ › 并记下键帽范围
std::wstring StripKeys(const std::wstring& s, std::vector<DWRITE_TEXT_RANGE>* keys) {
	std::wstring out;
	size_t start = 0;
	bool in = false;
	for (wchar_t ch : s) {
		if (ch == L'‹') { in = true; start = out.size(); continue; }
		if (ch == L'›' && in) { in = false; keys->push_back({(UINT32)start, (UINT32)(out.size() - start)}); continue; }
		out.push_back(ch);
	}
	return out;
}

class HelpWindow {
public:
	HWND hwnd = nullptr;
	bool dark = true;
	bool welcome = false;              // 欢迎模式：底部有复选框和按钮
	HWND chk = nullptr, btn = nullptr;
	HFONT uiFont = nullptr;
	HBRUSH footerBrush = nullptr;
	float FooterH() const { return welcome ? Px(60) : 0.f; }
	float scale = 1.f, scroll = 0.f, content = 0.f;
	std::vector<Item> items;
	ComPtr<ID2D1Factory1> d2d;
	ComPtr<ID2D1HwndRenderTarget> rt;
	ComPtr<IDWriteFactory> dw;
	ComPtr<IDWriteTextFormat> fTitle, fSub, fH2, fBody, fSmall;
	float layoutWidth = -1;

	void Build() {
		const Strings& L = Lang();
		auto add = [&](Kind k, const wchar_t* a, const wchar_t* b = nullptr, const wchar_t* c = nullptr, int num = 0) {
			Item it;
			it.kind = k;
			it.number = num;
			const wchar_t* src[3] = {a, b, c};
			for (int i = 0; i < 3; i++) if (src[i]) it.text[i] = StripKeys(src[i], &it.keys[i]);
			items.push_back(std::move(it));
		};
		add(Kind::Title, (std::wstring(L"∑  ") + L.title).c_str());
		add(Kind::Subtitle, L.subtitle);
		add(Kind::H2, L.hBasic);
		for (int i = 0; i < 4; i++) add(Kind::Numbered, L.basic[i], nullptr, nullptr, i + 1);
		add(Kind::Note, L.tipSelection);
		add(Kind::H2, L.hInsert);
		add(Kind::TableHead, L.colKey, L.colInserts, L.colFor);
		for (const HelpRow& r : L.rows) add(Kind::TableRow, r.keys, r.inserts, r.forWhat);
		add(Kind::H2, L.hIme);
		for (const wchar_t* s : L.ime) add(Kind::Bullet, s);
		add(Kind::H2, L.hBox);
		for (const wchar_t* s : L.box) add(Kind::Bullet, s);
		add(Kind::H2, L.hSafety);
		for (const wchar_t* s : L.safety) add(Kind::Bullet, s);
	}

	ComPtr<IDWriteTextFormat> Format(float px, DWRITE_FONT_WEIGHT w) {
		ComPtr<IDWriteTextFormat> f;
		dw->CreateTextFormat(L"Segoe UI", nullptr, w, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, px * scale, Lang().code, &f);
		if (Lang().rtl) f->SetReadingDirection(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
		f->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
		return f;
	}

	void MakeFormats() {
		fTitle = Format(24, DWRITE_FONT_WEIGHT_SEMI_BOLD);
		fSub = Format(14.5f, DWRITE_FONT_WEIGHT_NORMAL);
		fH2 = Format(17, DWRITE_FONT_WEIGHT_SEMI_BOLD);
		fBody = Format(15, DWRITE_FONT_WEIGHT_NORMAL);
		fSmall = Format(13, DWRITE_FONT_WEIGHT_SEMI_BOLD);
		layoutWidth = -1;
	}

	float Px(float dip) const { return dip * scale; }

	ComPtr<IDWriteTextLayout> Make(const std::wstring& t, IDWriteTextFormat* f, float w, const std::vector<DWRITE_TEXT_RANGE>& keys) {
		ComPtr<IDWriteTextLayout> lay;
		dw->CreateTextLayout(t.c_str(), (UINT32)t.size(), f, std::max(10.f, w), 100000.f, &lay);
		for (const auto& k : keys) {
			lay->SetFontFamilyName(L"Consolas", k);
			lay->SetFontSize(f->GetFontSize() * 0.88f, k);
		}
		return lay;
	}

	static float Height(IDWriteTextLayout* l) { DWRITE_TEXT_METRICS m{}; l->GetMetrics(&m); return m.height; }

	// 按窗口宽度重新排版（宽度不变就跳过）
	void Layout(float clientW) {
		if (clientW == layoutWidth) return;
		layoutWidth = clientW;
		const float margin = Px(24), maxW = Px(780);
		const float w = std::min(maxW, clientW - 2 * margin);
		float y = Px(26);
		for (Item& it : items) {
			switch (it.kind) {
			case Kind::Title: it.layout[0] = Make(it.text[0], fTitle.Get(), w, it.keys[0]); it.y = y; it.h = Height(it.layout[0].Get()); y += it.h + Px(4); break;
			case Kind::Subtitle: it.layout[0] = Make(it.text[0], fSub.Get(), w, it.keys[0]); it.y = y; it.h = Height(it.layout[0].Get()); y += it.h + Px(14); break;
			case Kind::H2: y += Px(14); it.layout[0] = Make(it.text[0], fH2.Get(), w - Px(14), it.keys[0]); it.y = y; it.h = Height(it.layout[0].Get()); y += it.h + Px(10); break;
			case Kind::Numbered:
			case Kind::Bullet: it.layout[0] = Make(it.text[0], fBody.Get(), w - Px(26), it.keys[0]); it.y = y; it.h = Height(it.layout[0].Get()); y += it.h + Px(6); break;
			case Kind::Note: y += Px(6); it.layout[0] = Make(it.text[0], fBody.Get(), w - Px(28), it.keys[0]); it.y = y; it.h = Height(it.layout[0].Get()) + Px(20); y += it.h + Px(6); break;
			case Kind::TableHead:
			case Kind::TableRow: {
				const float cw[3] = {w * 0.34f, w * 0.36f, w * 0.30f};
				float hmax = 0;
				for (int c = 0; c < 3; c++) {
					it.layout[c] = Make(it.text[c], it.kind == Kind::TableHead ? fSmall.Get() : fBody.Get(), cw[c] - Px(20), it.keys[c]);
					hmax = std::max(hmax, Height(it.layout[c].Get()));
				}
				it.y = y;
				it.h = hmax + Px(18);
				y += it.h;
				break;
			}
			}
		}
		content = y + Px(30);
		UpdateScroll();
	}

	void UpdateScroll() {
		RECT rc;
		GetClientRect(hwnd, &rc);
		float page = (float)rc.bottom - FooterH();
		scroll = std::clamp(scroll, 0.f, std::max(0.f, content - page));
		SCROLLINFO si{sizeof si, SIF_RANGE | SIF_PAGE | SIF_POS, 0, (int)content, (UINT)page, (int)scroll};
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	}

	// 阿拉伯语整页镜像：x 从右往左算
	D2D1_RECT_F Box(float x, float y, float w, float h, float left, float width) const {
		if (Lang().rtl) x = left + width - (x - left) - w;
		return D2D1::RectF(x, y, x + w, y + h);
	}

	void DrawKeys(IDWriteTextLayout* lay, const std::vector<DWRITE_TEXT_RANGE>& keys, D2D1_POINT_2F o, ID2D1Brush* bg, ID2D1Brush* border) {
		for (const auto& k : keys) {
			UINT32 n = 0;
			lay->HitTestTextRange(k.startPosition, k.length, 0, 0, nullptr, 0, &n);
			std::vector<DWRITE_HIT_TEST_METRICS> hm(n);
			lay->HitTestTextRange(k.startPosition, k.length, 0, 0, hm.data(), n, &n);
			for (const auto& m : hm) {
				D2D1_ROUNDED_RECT r{D2D1::RectF(o.x + m.left - Px(4), o.y + m.top + Px(1), o.x + m.left + m.width + Px(4), o.y + m.top + m.height - Px(1)), Px(4), Px(4)};
				rt->FillRoundedRectangle(r, bg);
				rt->DrawRoundedRectangle(r, border, Px(1));
			}
		}
	}

	void Paint() {
		PAINTSTRUCT ps;
		BeginPaint(hwnd, &ps);
		RECT rc;
		GetClientRect(hwnd, &rc);
		if (!rt) {
			D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96.f, 96.f);
			d2d->CreateHwndRenderTarget(p, D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right, rc.bottom)), &rt);
		}
		if (rt) {
			Layout((float)rc.right);
			Palette P = MakePalette(dark);
			ComPtr<ID2D1SolidColorBrush> fg, muted, card, accent, border, keyBg;
			rt->CreateSolidColorBrush(P.fg, &fg);
			rt->CreateSolidColorBrush(P.muted, &muted);
			rt->CreateSolidColorBrush(P.card, &card);
			rt->CreateSolidColorBrush(P.accent, &accent);
			rt->CreateSolidColorBrush(P.border, &border);
			rt->CreateSolidColorBrush(P.keyBg, &keyBg);
			rt->BeginDraw();
			rt->Clear(P.bg);
			const float margin = Px(24), w = std::min(Px(780), rc.right - 2 * margin);
			const float left = (rc.right - w) / 2;
			const float cw[3] = {w * 0.34f, w * 0.36f, w * 0.30f};
			auto text = [&](IDWriteTextLayout* lay, float x, float y, float width, ID2D1Brush* b, const std::vector<DWRITE_TEXT_RANGE>& keys) {
				D2D1_RECT_F r = Box(x, y - scroll, width, 1, left, w);
				lay->SetMaxWidth(width);
				DrawKeys(lay, keys, D2D1::Point2F(r.left, r.top), keyBg.Get(), border.Get());
				rt->DrawTextLayout(D2D1::Point2F(r.left, r.top), lay, b);
			};
			rt->PushAxisAlignedClip(D2D1::RectF(0, 0, (float)rc.right, rc.bottom - FooterH()), D2D1_ANTIALIAS_MODE_ALIASED);
			for (Item& it : items) {
				float y = it.y;
				if (y - scroll > rc.bottom || y + it.h - scroll < 0) continue;
				switch (it.kind) {
				case Kind::Title: text(it.layout[0].Get(), left, y, w, fg.Get(), it.keys[0]); break;
				case Kind::Subtitle: text(it.layout[0].Get(), left, y, w, muted.Get(), it.keys[0]); break;
				case Kind::H2:
					rt->FillRectangle(Box(left, y - scroll, Px(4), it.h, left, w), accent.Get());
					text(it.layout[0].Get(), left + Px(14), y, w - Px(14), fg.Get(), it.keys[0]);
					break;
				case Kind::Numbered:
				case Kind::Bullet: {
					std::wstring mark = it.kind == Kind::Numbered ? std::to_wstring(it.number) + L"." : L"•";
					ComPtr<IDWriteTextLayout> m;
					dw->CreateTextLayout(mark.c_str(), (UINT32)mark.size(), fBody.Get(), Px(22), Px(40), &m);
					D2D1_RECT_F r = Box(left + Px(4), y - scroll, Px(22), 1, left, w);
					rt->DrawTextLayout(D2D1::Point2F(r.left, r.top), m.Get(), muted.Get());
					text(it.layout[0].Get(), left + Px(26), y, w - Px(26), fg.Get(), it.keys[0]);
					break;
				}
				case Kind::Note:
					rt->FillRoundedRectangle({Box(left, y - scroll, w, it.h, left, w), Px(8), Px(8)}, card.Get());
					text(it.layout[0].Get(), left + Px(14), y + Px(10), w - Px(28), muted.Get(), it.keys[0]);
					break;
				case Kind::TableHead:
				case Kind::TableRow: {
					rt->FillRectangle(Box(left, y - scroll, w, it.h, left, w), card.Get());
					rt->DrawLine(D2D1::Point2F(left, y + it.h - scroll), D2D1::Point2F(left + w, y + it.h - scroll), border.Get(), Px(1));
					float x = left;
					for (int c = 0; c < 3; c++) {
						text(it.layout[c].Get(), x + Px(10), y + Px(9), cw[c] - Px(20), it.kind == Kind::TableHead ? muted.Get() : fg.Get(), it.keys[c]);
						x += cw[c];
					}
					break;
				}
				}
			}
			rt->PopAxisAlignedClip();
			if (welcome) {
				float fy = rc.bottom - FooterH();
				rt->FillRectangle(D2D1::RectF(0, fy, (float)rc.right, (float)rc.bottom), card.Get());
				rt->DrawLine(D2D1::Point2F(0, fy + 0.5f), D2D1::Point2F((float)rc.right, fy + 0.5f), border.Get(), 1.f);
			}
			if (rt->EndDraw() == D2DERR_RECREATE_TARGET) rt.Reset();
		}
		EndPaint(hwnd, &ps);
	}

	void CreateFooter(HINSTANCE inst) {
		if (!welcome) return;
		const Strings& L = Lang();
		chk = CreateWindowExW(0, L"BUTTON", L.dontShow, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)ID_CHECK, inst, nullptr);
		btn = CreateWindowExW(0, L"BUTTON", L.startUsing, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)ID_START, inst, nullptr);
		ApplyFooterStyle();
		LayoutFooter();
	}
	void ApplyFooterStyle() {
		if (!welcome || !chk) return;
		if (uiFont) DeleteObject(uiFont);
		// 底栏是系统原生控件（GDI 绘制，不像 DirectWrite 会自动补字）：按语言选系统自带、含对应文字的界面字体
		const std::wstring code = Lang().code;
		const wchar_t* face = code == L"zh-CN" ? L"Microsoft YaHei UI" : code == L"zh-TW" ? L"Microsoft JhengHei UI"
			: code == L"ja" ? L"Yu Gothic UI" : code == L"ko" ? L"Malgun Gothic" : L"Segoe UI";
		uiFont = CreateFontW(-(int)Px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
		SendMessageW(chk, WM_SETFONT, (WPARAM)uiFont, TRUE);
		SendMessageW(btn, WM_SETFONT, (WPARAM)uiFont, TRUE);
		// 深色：按钮用系统深色样式；复选框去掉主题才能自定义文字颜色（WM_CTLCOLORSTATIC）
		SetWindowTheme(btn, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
		if (dark) SetWindowTheme(chk, L"", L"");
		if (footerBrush) DeleteObject(footerBrush);
		D2D1_COLOR_F c = MakePalette(dark).card;
		footerBrush = CreateSolidBrush(RGB((int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255)));
	}
	void LayoutFooter() {
		if (!welcome || !chk) return;
		RECT rc;
		GetClientRect(hwnd, &rc);
		int fh = (int)FooterH(), m = (int)Px(20), bw = (int)Px(120), bh = (int)Px(34), ch = (int)Px(24);
		int y = rc.bottom - fh;
		int cw = rc.right - bw - 3 * m;
		if (Lang().rtl) {   // 阿拉伯语：复选框在右，按钮在左
			MoveWindow(chk, rc.right - m - cw, y + (fh - ch) / 2, cw, ch, TRUE);
			MoveWindow(btn, m, y + (fh - bh) / 2, bw, bh, TRUE);
		} else {
			MoveWindow(chk, m, y + (fh - ch) / 2, cw, ch, TRUE);
			MoveWindow(btn, rc.right - m - bw, y + (fh - bh) / 2, bw, bh, TRUE);
		}
	}
	void SaveChoice() {
		if (!welcome || !chk) return;
		bool hide = SendMessageW(chk, BM_GETCHECK, 0, 0) == BST_CHECKED;
		HKEY k;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
			DWORD v = hide ? 1 : 0;
			RegSetValueExW(k, L"HideWelcome", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof v);
			RegCloseKey(k);
		}
	}

	void ScrollBy(float dy) {
		scroll += dy;
		UpdateScroll();
		InvalidateRect(hwnd, nullptr, FALSE);
	}

	LRESULT OnMessage(UINT m, WPARAM w, LPARAM l) {
		switch (m) {
		case WM_PAINT: Paint(); return 0;
		case WM_ERASEBKGND: return 1;
		case WM_SIZE:
			if (rt) rt->Resize(D2D1::SizeU(LOWORD(l), HIWORD(l)));
			LayoutFooter();
			UpdateScroll();
			InvalidateRect(hwnd, nullptr, FALSE);
			return 0;
		case WM_MOUSEWHEEL: ScrollBy(-GET_WHEEL_DELTA_WPARAM(w) / 120.f * Px(60)); return 0;
		case WM_VSCROLL: {
			SCROLLINFO si{sizeof si, SIF_ALL};
			GetScrollInfo(hwnd, SB_VERT, &si);
			switch (LOWORD(w)) {
			case SB_LINEUP: scroll -= Px(40); break;
			case SB_LINEDOWN: scroll += Px(40); break;
			case SB_PAGEUP: scroll -= si.nPage; break;
			case SB_PAGEDOWN: scroll += si.nPage; break;
			case SB_THUMBTRACK: case SB_THUMBPOSITION: scroll = (float)si.nTrackPos; break;
			}
			ScrollBy(0);
			return 0;
		}
		case WM_COMMAND:
			if (LOWORD(w) == ID_START) { DestroyWindow(hwnd); return 0; }
			if (LOWORD(w) == ID_CHECK) {
				SaveChoice();   // 一勾就保存：窗口没关程序就退出（关机、被结束）也不丢
				SetFocus(hwnd);   // 键盘焦点留在窗口上，Enter / Esc 始终能关
				return 0;
			}
			break;
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORBTN:
			if (welcome && footerBrush) {
				D2D1_COLOR_F f = MakePalette(dark).fg, c = MakePalette(dark).card;
				SetTextColor((HDC)w, RGB((int)(f.r * 255), (int)(f.g * 255), (int)(f.b * 255)));
				SetBkColor((HDC)w, RGB((int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255)));
				return (LRESULT)footerBrush;
			}
			break;
		case WM_KEYDOWN:
			if (w == VK_ESCAPE || (w == VK_RETURN && welcome)) { DestroyWindow(hwnd); return 0; }
			if (w == VK_DOWN) { ScrollBy(Px(40)); return 0; }
			if (w == VK_UP) { ScrollBy(-Px(40)); return 0; }
			if (w == VK_NEXT || w == VK_SPACE) { RECT rc; GetClientRect(hwnd, &rc); ScrollBy((float)rc.bottom * 0.9f); return 0; }
			if (w == VK_PRIOR) { RECT rc; GetClientRect(hwnd, &rc); ScrollBy(-(float)rc.bottom * 0.9f); return 0; }
			if (w == VK_HOME) { scroll = 0; ScrollBy(0); return 0; }
			if (w == VK_END) { scroll = content; ScrollBy(0); return 0; }
			break;
		case WM_DPICHANGED: {
			scale = HIWORD(w) / 96.f;
			MakeFormats();
			ApplyFooterStyle();
			const RECT* r = reinterpret_cast<const RECT*>(l);
			SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
			return 0;
		}
		case WM_DESTROY:
			SaveChoice();   // 点按钮、按 Enter / Esc、点右上角 × 都按当前勾选状态保存
			hwnd = nullptr;
			rt.Reset();
			return 0;
		}
		return DefWindowProcW(hwnd, m, w, l);
	}
};

HelpWindow* g_help = nullptr;

LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
	if (g_help && (g_help->hwnd == h || m == WM_NCCREATE)) {
		if (m == WM_NCCREATE) g_help->hwnd = h;
		return g_help->OnMessage(m, w, l);
	}
	return DefWindowProcW(h, m, w, l);
}

}  // namespace

bool WelcomeHidden() {
	DWORD v = 0, n = sizeof v;
	return RegGetValueW(HKEY_CURRENT_USER, kRegKey, L"HideWelcome", RRF_RT_REG_DWORD, nullptr, &v, &n) == ERROR_SUCCESS && v == 1;
}

void ShowHelp(HINSTANCE inst, bool dark, bool welcome) {
	if (g_help && g_help->hwnd) {
		ShowWindow(g_help->hwnd, SW_RESTORE);
		SetForegroundWindow(g_help->hwnd);
		return;
	}
	static bool registered = false;
	if (!registered) {
		WNDCLASSEXW wc{sizeof wc};
		wc.lpfnWndProc = Proc;
		wc.hInstance = inst;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
		wc.lpszClassName = kClass;
		RegisterClassExW(&wc);
		registered = true;
	}
	delete g_help;
	g_help = new HelpWindow;
	g_help->dark = dark;
	g_help->welcome = welcome;
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), nullptr, &g_help->d2d);
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(g_help->dw.GetAddressOf()));
	g_help->Build();

	POINT pt;
	GetCursorPos(&pt);
	HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi{sizeof mi};
	GetMonitorInfoW(mon, &mi);
	UINT dpiX = 96, dpiY = 96;
	HMODULE shcore = LoadLibraryW(L"Shcore.dll");
	if (shcore) {
		auto getDpi = reinterpret_cast<HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*)>(GetProcAddress(shcore, "GetDpiForMonitor"));
		if (getDpi) getDpi(mon, 0, &dpiX, &dpiY);
	}
	g_help->scale = dpiX / 96.f;
	g_help->MakeFormats();
	int w = (int)(860 * g_help->scale), h = (int)(860 * g_help->scale);
	RECT wa = mi.rcWork;
	w = std::min<int>(w, wa.right - wa.left - 40);
	h = std::min<int>(h, wa.bottom - wa.top - 40);
	int x = wa.left + (wa.right - wa.left - w) / 2, y = wa.top + (wa.bottom - wa.top - h) / 2;
	HWND hwnd = CreateWindowExW(0, kClass,   // 阿拉伯语的镜像排版由绘制代码自己算，不用 WS_EX_LAYOUTRTL（与 Direct2D 叠加会出错）
		 Lang().helpWindow, WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_CLIPCHILDREN,
		x, y, w, h, nullptr, nullptr, inst, nullptr);
	if (!hwnd) return;
	BOOL d = dark;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &d, sizeof d);
	g_help->CreateFooter(inst);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	SetForegroundWindow(hwnd);
}

}  // namespace la
