#include "sysio.h"

#include <imm.h>
#include <oleacc.h>
#include <psapi.h>
#include <algorithm>
#include <thread>

#include "util.h"

namespace la {
namespace {
void Sleep(int ms) { ::Sleep((DWORD)ms); }

bool OpenClipboardRetry() {
	for (int i = 0; i < 10; i++) {
		if (OpenClipboard(nullptr)) return true;
		Sleep(20);
	}
	return false;
}

// 只能当作 HGLOBAL 复制的格式；位图、调色板、元文件句柄等由系统从 CF_DIB 等自动合成，跳过
bool IsHandleFormat(UINT f) {
	return f == CF_BITMAP || f == CF_PALETTE || f == CF_METAFILEPICT || f == CF_ENHMETAFILE || f == CF_OWNERDISPLAY ||
		f == CF_DSPBITMAP || f == CF_DSPMETAFILEPICT || f == CF_DSPENHMETAFILE || (f >= CF_GDIOBJFIRST && f <= CF_GDIOBJLAST);
}
}  // namespace

void SendKeys(const std::vector<KeyEv>& seq) {
	std::vector<INPUT> in(seq.size());
	for (size_t i = 0; i < seq.size(); i++) {
		in[i].type = INPUT_KEYBOARD;
		in[i].ki.wVk = seq[i].vk;
		in[i].ki.dwFlags = seq[i].up ? KEYEVENTF_KEYUP : 0;
	}
	if (!in.empty()) SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

bool KeyIsDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

bool ClipboardSnapshot::Save() {
	items_.clear();
	saved_ = false;
	if (!OpenClipboardRetry()) return false;
	size_t total = 0;
	for (UINT f = EnumClipboardFormats(0); f; f = EnumClipboardFormats(f)) {
		if (IsHandleFormat(f)) continue;
		HANDLE h = GetClipboardData(f);
		if (!h) continue;
		SIZE_T n = GlobalSize(h);
		if (n == 0 || total + n > 64u * 1024 * 1024) continue;   // 太大的内容不备份，避免占用过多内存
		void* p = GlobalLock(h);
		if (!p) continue;
		Item it{f, std::vector<unsigned char>((unsigned char*)p, (unsigned char*)p + n)};
		GlobalUnlock(h);
		total += n;
		items_.push_back(std::move(it));
	}
	CloseClipboard();
	saved_ = true;
	return true;
}

bool ClipboardSnapshot::Restore() const {
	if (!saved_) return false;
	if (!OpenClipboardRetry()) return false;
	EmptyClipboard();
	for (const Item& it : items_) {
		HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, it.data.size());
		if (!h) continue;
		void* p = GlobalLock(h);
		memcpy(p, it.data.data(), it.data.size());
		GlobalUnlock(h);
		if (!SetClipboardData(it.fmt, h)) GlobalFree(h);
	}
	CloseClipboard();
	return true;
}

bool ClipboardSetText(const std::wstring& s) {
	if (!OpenClipboardRetry()) return false;
	EmptyClipboard();
	bool ok = true;
	if (!s.empty()) {
		size_t bytes = (s.size() + 1) * sizeof(wchar_t);
		HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
		ok = h != nullptr;
		if (h) {
			memcpy(GlobalLock(h), s.c_str(), bytes);
			GlobalUnlock(h);
			if (!SetClipboardData(CF_UNICODETEXT, h)) { GlobalFree(h); ok = false; }
		}
	}
	CloseClipboard();
	return ok;
}

std::wstring ClipboardGetText(bool* ok) {
	std::wstring s;
	if (ok) *ok = false;
	if (!OpenClipboardRetry()) return s;
	if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
		if (auto* p = (const wchar_t*)GlobalLock(h)) {
			s.assign(p, wcsnlen(p, GlobalSize(h) / sizeof(wchar_t)));
			GlobalUnlock(h);
		}
	}
	CloseClipboard();
	if (ok) *ok = true;
	return s;
}

// ---------- 光标位置 ----------
static bool MsaaCaret(HWND hwnd, POINT* out) {
	IAccessible* acc = nullptr;
	if (FAILED(AccessibleObjectFromWindow(hwnd, (DWORD)OBJID_CARET, IID_IAccessible, (void**)&acc)) || !acc) return false;
	long l = 0, t = 0, w = 0, h = 0;
	VARIANT self;
	self.vt = VT_I4;
	self.lVal = CHILDID_SELF;
	HRESULT hr = acc->accLocation(&l, &t, &w, &h, self);
	acc->Release();
	if (FAILED(hr) || (l == 0 && t == 0)) return false;
	*out = {l, t + h};
	return true;
}

POINT CaretScreenPos() {
	GUITHREADINFO gti{sizeof gti};
	if (GetGUIThreadInfo(0, &gti) && gti.hwndCaret) {
		POINT pt{gti.rcCaret.left, gti.rcCaret.bottom};
		ClientToScreen(gti.hwndCaret, &pt);
		if (pt.x || pt.y) return pt;
	}
	// Electron（Claude、VS Code）、浏览器：系统光标为空，用辅助功能（MSAA）光标对象
	HWND f = gti.hwndFocus ? gti.hwndFocus : GetForegroundWindow();
	POINT pt;
	if (f && MsaaCaret(f, &pt)) return pt;
	GetCursorPos(&pt);
	pt.y += 20;
	return pt;
}

// ---------- 取选区 ----------
std::wstring GrabSelection() {
	ClipboardSnapshot saved;
	saved.Save();
	ClipboardSetText(L"");
	DWORD seq0 = GetClipboardSequenceNumber();
	// 用户还按着 Alt（Alt+L）：先补屏蔽键防止激活菜单，再补 Alt 抬起，然后马上 Ctrl+C——不用等用户松手
	SendKeys({{0xFF, false}, {0xFF, true}, {VK_MENU, true}, {VK_LMENU, true}, {VK_RMENU, true},
		{VK_CONTROL, false}, {'C', false}, {'C', true}, {VK_CONTROL, true}});
	double t0 = NowMs();
	while (NowMs() - t0 < 100 && GetClipboardSequenceNumber() == seq0) Sleep(5);   // 没选中时只能等超时；常见软件 50ms 内响应
	std::wstring got;
	if (GetClipboardSequenceNumber() != seq0) got = ClipboardGetText(nullptr);
	saved.Restore();
	if (got.empty() || got.back() == L'\n' || got.size() > 4000) return L"";   // 没选中时有的编辑器会复制整行
	std::wstring out;
	out.reserve(got.size());
	for (size_t i = 0; i < got.size(); i++) {
		if (got[i] == L'\r' && i + 1 < got.size() && got[i + 1] == L'\n') continue;
		out.push_back(got[i]);
	}
	return out;
}

// ---------- 粘贴上屏 ----------
void PasteText(const std::wstring& s, int settleMs, const std::function<void()>& after) {
	ClipboardSnapshot saved;
	saved.Save();
	if (!ClipboardSetText(s)) { Log("粘贴失败：剪贴板打不开"); return; }
	Sleep(30);
	// 用户上屏时往往还按着 Ctrl+Shift 或 Alt：直接发 Ctrl+V 会变成 Ctrl+Shift+V（Office 的粘贴格式）或 Ctrl+Alt+V。先补屏蔽键和 Shift/Alt 抬起
	SendKeys({{0xFF, false}, {0xFF, true},
		{VK_SHIFT, true}, {VK_LSHIFT, true}, {VK_RSHIFT, true}, {VK_MENU, true}, {VK_LMENU, true}, {VK_RMENU, true},
		{VK_CONTROL, false}, {'V', false}, {'V', true}, {VK_CONTROL, true}});
	Sleep(settleMs);   // Word/PowerPoint 转换 MathML 慢：恢复太早它会读到被换回去的旧内容（实测）
	if (after) after();
	saved.Restore();
}

// ---------- 输入法 ----------
HWND FocusWindowOf(HWND fg) {
	GUITHREADINFO gti{sizeof gti};
	GetGUIThreadInfo(0, &gti);
	return gti.hwndFocus ? gti.hwndFocus : fg;
}

bool ImeIsNative(HWND ime) {
	if (!ime) return false;
	DWORD_PTR open = 0, conv = 0;
	SendMessageTimeoutW(ime, WM_IME_CONTROL, 0x0005 /*IMC_GETOPENSTATUS*/, 0, SMTO_ABORTIFHUNG, 100, &open);
	SendMessageTimeoutW(ime, WM_IME_CONTROL, 0x0001 /*IMC_GETCONVERSIONMODE*/, 0, SMTO_ABORTIFHUNG, 100, &conv);
	return open && (conv & IME_CMODE_NATIVE);
}

void ImeSwitchToEnglish(HWND boxWnd, HWND editWnd) {
	HWND ime = ImmGetDefaultIMEWnd(editWnd);
	if (!ime) return;
	LANGID lang = LOWORD((UINT_PTR)GetKeyboardLayout(GetWindowThreadProcessId(editWnd, nullptr)));
	if (!ImeIsNative(ime)) { Log("框内已是英文 0x%04x", lang); return; }
	// 1) Windows 自带的「关闭输入法」键 VK_IME_OFF（微软日文、中文输入法都认）；只在前台确实是公式框时按
	if (GetForegroundWindow() == boxWnd) { SendKeys({{VK_IME_OFF, false}, {VK_IME_OFF, true}}); Sleep(120); }
	// 2) 还是中文：补一次 Shift 单击（微软拼音的中英切换键）
	if (ImeIsNative(ime) && lang == 0x0804 && GetForegroundWindow() == boxWnd) { SendKeys({{VK_LSHIFT, false}, {VK_LSHIFT, true}}); Sleep(120); }
	Log("框内输入法切英文：%s 0x%04x", ImeIsNative(ime) ? "失败" : "成功", lang);
}

void ImeRestoreNative(HWND prev) {
	Sleep(100);
	HWND f = FocusWindowOf(GetForegroundWindow());
	HWND ime = ImmGetDefaultIMEWnd(f);
	LANGID lang = LOWORD((UINT_PTR)GetKeyboardLayout(GetWindowThreadProcessId(f, nullptr)));
	if (!ime || ImeIsNative(ime) || GetForegroundWindow() != prev) return;
	SendKeys({{VK_IME_ON, false}, {VK_IME_ON, true}});   // Windows 自带的「打开输入法」键
	Sleep(120);
	if (!ImeIsNative(ime) && lang == 0x0804 && GetForegroundWindow() == prev) {
		SendKeys({{VK_LSHIFT, false}, {VK_LSHIFT, true}});   // 微软拼音：Shift 单击切回中文
		Sleep(120);
	}
	Log("回原输入框切回母语输入：%s 0x%04x", ImeIsNative(ime) ? "成功" : "失败", lang);
}

std::wstring ForegroundExeLower() {
	DWORD pid = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &pid);
	std::wstring name;
	if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
		wchar_t buf[MAX_PATH * 2];
		DWORD n = (DWORD)std::size(buf);
		if (QueryFullProcessImageNameW(h, 0, buf, &n)) {
			name.assign(buf, n);
			size_t k = name.find_last_of(L"\\/");
			if (k != std::wstring::npos) name = name.substr(k + 1);
			std::transform(name.begin(), name.end(), name.begin(), ::towlower);
		}
		CloseHandle(h);
	}
	return name;
}

RECT WorkAreaAt(POINT pt) {
	MONITORINFO mi{sizeof mi};
	GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi);
	return mi.rcWork;
}

}  // namespace la
