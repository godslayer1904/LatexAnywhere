// LatexAnywhere（Windows）：任意输入框按 Alt+L 弹出公式框，用原版 Latex Suite 的方式打公式。
// 进程结构：守护进程（无窗口）启动工作进程并在其崩溃时自动重启；工作进程负责托盘、热键、公式框。
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <thread>
#include <regex>

#include "box.h"
#include "js_engine.h"
#include "resource.h"
#include "sysio.h"
#include "util.h"

namespace {

using namespace la;

constexpr UINT WM_TRAY = WM_APP + 10;
constexpr int HK_OPEN = 1, HK_KILL = 9;
constexpr int ID_HELP = 101, ID_AUTOSTART = 102, ID_QUIT = 103;
constexpr int kExitQuit = 0;          // 用户退出：守护进程也退出
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunName[] = L"LatexAnywhere";

HINSTANCE g_inst;
HWND g_msg;
NOTIFYICONDATAW g_nid{};
Engine* g_eng;
Box* g_box;
bool g_zh = true;

const wchar_t* T(const wchar_t* zh, const wchar_t* en) { return g_zh ? zh : en; }

// ---------- 资源 ----------
std::string LoadRes(int id) {
	HRSRC r = FindResourceW(g_inst, MAKEINTRESOURCEW(id), RT_RCDATA);
	if (!r) return {};
	HGLOBAL g = LoadResource(g_inst, r);
	return std::string(static_cast<const char*>(LockResource(g)), SizeofResource(g_inst, r));
}

// ---------- 设置：优先用 Obsidian 里 Latex Suite 的设置（与 Obsidian 体验一致），否则默认设置 ----------
std::string FindObsidianSettings(std::wstring* from) {
	PWSTR p = nullptr;
	std::wstring appdata;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) appdata = p;
	CoTaskMemFree(p);
	std::string own;
	if (ReadFileUtf8(appdata + L"\\LatexAnywhere\\settings.json", &own) && !own.empty()) { *from = L"settings.json"; return own; }
	std::string reg;
	if (!ReadFileUtf8(appdata + L"\\obsidian\\obsidian.json", &reg)) return {};
	static const std::regex re(R"re("path"\s*:\s*"((?:[^"\\]|\\.)*)")re");
	for (std::sregex_iterator it(reg.begin(), reg.end(), re), end; it != end; ++it) {
		std::string path = (*it)[1].str();
		std::string unesc;
		for (size_t i = 0; i < path.size(); i++) unesc += (path[i] == '\\' && i + 1 < path.size()) ? path[++i] : path[i];
		std::wstring f = Utf8ToWide(unesc) + L"\\.obsidian\\plugins\\obsidian-latex-suite\\data.json";
		std::string data;
		if (ReadFileUtf8(f, &data) && !data.empty()) { *from = f; return data; }
	}
	return {};
}

bool SystemDark() {
	DWORD v = 1, n = sizeof v;
	RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
		RRF_RT_REG_DWORD, nullptr, &v, &n);
	return v == 0;
}

// ---------- 开机自启 ----------
bool AutostartOn() {
	wchar_t buf[1024];
	DWORD n = sizeof buf;
	return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunName, RRF_RT_REG_SZ, nullptr, buf, &n) == ERROR_SUCCESS;
}
void AutostartSet(bool on) {
	HKEY k;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
	if (on) {
		std::wstring v = L"\"" + ExePath() + L"\"";
		RegSetValueExW(k, kRunName, 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
	} else {
		RegDeleteValueW(k, kRunName);
	}
	RegCloseKey(k);
}

// ---------- 使用说明：从资源释放到本地，经 explorer 打开（浏览器挂在 explorer 名下） ----------
void OpenHelp() {
	std::string html = LoadRes(g_zh ? IDR_HELP_ZH : IDR_HELP_EN);
	std::wstring path = AppDataDir() + L"\\help.html";
	WriteFileBytes(path, html.data(), html.size());
	ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"\"" + path + L"\"").c_str(), nullptr, SW_SHOWNORMAL);
}

void Quit(const char* why) {
	Log("退出：%s", why);
	Shell_NotifyIconW(NIM_DELETE, &g_nid);
	ExitProcess(kExitQuit);
}

void ShowTrayMenu() {
	HMENU m = CreatePopupMenu();
	AppendMenuW(m, MF_STRING, ID_HELP, T(L"使用说明", L"Help"));
	AppendMenuW(m, MF_STRING | (AutostartOn() ? MF_CHECKED : 0), ID_AUTOSTART, T(L"开机自启", L"Start with Windows"));
	AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(m, MF_STRING, ID_QUIT, T(L"退出", L"Quit"));
	POINT pt;
	GetCursorPos(&pt);
	SetForegroundWindow(g_msg);
	int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_msg, nullptr);
	DestroyMenu(m);
	if (cmd == ID_HELP) OpenHelp();
	else if (cmd == ID_AUTOSTART) AutostartSet(!AutostartOn());
	else if (cmd == ID_QUIT) Quit("托盘退出");
}

LRESULT CALLBACK MsgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
	static UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
	if (m == taskbarCreated) { Shell_NotifyIconW(NIM_ADD, &g_nid); return 0; }   // 资源管理器重启后补回托盘图标
	switch (m) {
	case WM_HOTKEY:
		if (w == HK_KILL) Quit("紧急强杀 Win+Alt+L");
		if (w == HK_OPEN && g_box) g_box->Toggle();
		return 0;
	case WM_TRAY:
		if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_CONTEXTMENU || LOWORD(l) == WM_LBUTTONUP) ShowTrayMenu();
		return 0;
	case WM_SETTINGCHANGE:
		if (l && !lstrcmpW(reinterpret_cast<LPCWSTR>(l), L"ImmersiveColorSet") && g_box) g_box->SetDark(SystemDark());
		return 0;
	}
	return DefWindowProcW(h, m, w, l);
}

// ---------- 自我监控：内存或 CPU 异常就退出，由守护进程重启 ----------
void Watchdog() {
	HANDLE self = GetCurrentProcess();
	ULONGLONG lastCpu = 0;
	double lastT = NowMs();
	int hot = 0;
	for (;;) {
		Sleep(5000);
		PROCESS_MEMORY_COUNTERS_EX pmc{sizeof pmc};
		GetProcessMemoryInfo(self, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc);
		FILETIME c, e, k, u;
		GetProcessTimes(self, &c, &e, &k, &u);
		ULONGLONG cpu = ((ULONGLONG)k.dwHighDateTime << 32 | k.dwLowDateTime) + ((ULONGLONG)u.dwHighDateTime << 32 | u.dwLowDateTime);
		double now = NowMs();
		double busy = (cpu - lastCpu) / 1e4 / (now - lastT);   // 占一个核的比例
		lastCpu = cpu;
		lastT = now;
		hot = busy > 0.9 ? hot + 1 : 0;
		if (pmc.PrivateUsage > 400ull * 1024 * 1024) { Log("自我监控：内存超限 %llu MB，退出重启", pmc.PrivateUsage >> 20); ExitProcess(3); }
		if (hot >= 3) { Log("自我监控：CPU 持续占满，退出重启"); ExitProcess(4); }
	}
}

LONG WINAPI OnCrash(EXCEPTION_POINTERS* ep) {
	std::wstring dir = AppDataDir() + L"\\crash";
	CreateDirectoryW(dir.c_str(), nullptr);
	SYSTEMTIME t;
	GetLocalTime(&t);
	wchar_t name[64];
	swprintf_s(name, L"\\%04d%02d%02d-%02d%02d%02d.dmp", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	HANDLE f = CreateFileW((dir + name).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (f != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), ep, FALSE};
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, &mei, nullptr, nullptr);
		CloseHandle(f);
	}
	Log("崩溃：异常码 0x%08lx，已写崩溃转储，守护进程会重启", ep->ExceptionRecord->ExceptionCode);
	return EXCEPTION_EXECUTE_HANDLER;
}

int RunWorker() {
	SetUnhandledExceptionFilter(OnCrash);
	HANDLE mtx = CreateMutexW(nullptr, FALSE, L"Local\\LatexAnywhereWorker");
	if (GetLastError() == ERROR_ALREADY_EXISTS) return kExitQuit;
	(void)mtx;
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	g_zh = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;

	double t0 = NowMs();
	std::wstring from = L"默认设置";
	std::string settings = FindObsidianSettings(&from);
	g_eng = new Engine;
	std::string err;
	if (!g_eng->Init({{LoadRes(IDR_ENGINE), true, "engine"}, {LoadRes(IDR_MATH), true, "math"}}, settings, &err)) {
		Log("引擎初始化失败（设置来源 %s）：%s", WideToUtf8(from).c_str(), err.c_str());
		if (!settings.empty()) {   // 设置文件有问题：退回默认设置，不能因此用不了
			delete g_eng;
			g_eng = new Engine;
			if (!g_eng->Init({{LoadRes(IDR_ENGINE), true, "engine"}, {LoadRes(IDR_MATH), true, "math"}}, "", &err)) { Log("默认设置也失败：%s", err.c_str()); return 2; }
			from = L"默认设置（Obsidian 设置读取失败）";
		} else {
			return 2;
		}
	}
	// 预热：第一次弹框不用再等 MathJax 初始化
	std::u16string warm;
	g_eng->CallString("LatexAnywhereMath", "previewPacked", u"$x^2+\\frac{1}{2}$", &warm, nullptr);
	Log("就绪：引擎 %.0fms，设置来源 %s，JS 内存 %.1f MB", NowMs() - t0, WideToUtf8(from).c_str(), g_eng->MemoryUsed() / 1048576.0);

	WNDCLASSEXW wc{sizeof wc};
	wc.lpfnWndProc = MsgProc;
	wc.hInstance = g_inst;
	wc.lpszClassName = L"LatexAnywhereMsg";
	RegisterClassExW(&wc);
	g_msg = CreateWindowExW(0, wc.lpszClassName, L"LatexAnywhereMsg", 0, 0, 0, 0, 0, nullptr, nullptr, g_inst, nullptr);

	g_box = new Box;
	if (!g_box->Create(g_inst, g_eng)) { Log("公式框创建失败"); return 2; }
	g_box->SetDark(SystemDark());

	g_nid.cbSize = sizeof g_nid;
	g_nid.hWnd = g_msg;
	g_nid.uID = 1;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAY;
	g_nid.hIcon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
	wcscpy_s(g_nid.szTip, T(L"LatexAnywhere：Alt+L 打公式", L"LatexAnywhere: Alt+L to type math"));
	Shell_NotifyIconW(NIM_ADD, &g_nid);

	if (!RegisterHotKey(g_msg, HK_OPEN, MOD_ALT | MOD_NOREPEAT, 'L')) {
		Log("Alt+L 注册失败（被其他程序占用）");
		g_nid.uFlags |= NIF_INFO;
		wcscpy_s(g_nid.szInfoTitle, L"LatexAnywhere");
		wcscpy_s(g_nid.szInfo, T(L"Alt+L 已被其他程序占用，公式框暂时无法唤起。", L"Alt+L is used by another program."));
		Shell_NotifyIconW(NIM_MODIFY, &g_nid);
	} else {
		Log("Alt+L 已注册");
	}
	RegisterHotKey(g_msg, HK_KILL, MOD_WIN | MOD_ALT | MOD_NOREPEAT, 'L');   // 紧急强杀（不用 Ctrl+Alt+Shift 系，和 Moonlight 冲突）
	std::thread(Watchdog).detach();

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return kExitQuit;
}

// 守护进程：启动工作进程；工作进程异常退出就重启（一分钟内最多 5 次，防止反复崩溃拖垮电脑）
int RunGuardian() {
	HANDLE mtx = CreateMutexW(nullptr, FALSE, L"Local\\LatexAnywhereGuardian");
	if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
	(void)mtx;
	std::wstring cmd = L"\"" + ExePath() + L"\" --worker";
	double window = NowMs();
	int restarts = 0;
	for (;;) {
		STARTUPINFOW si{sizeof si};
		PROCESS_INFORMATION pi{};
		std::wstring c = cmd;
		if (!CreateProcessW(nullptr, c.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { Log("启动工作进程失败 %lu", GetLastError()); return 1; }
		CloseHandle(pi.hThread);
		WaitForSingleObject(pi.hProcess, INFINITE);
		DWORD code = 0;
		GetExitCodeProcess(pi.hProcess, &code);
		CloseHandle(pi.hProcess);
		if (code == kExitQuit) return 0;
		if (NowMs() - window > 60000) { window = NowMs(); restarts = 0; }
		if (++restarts > 5) { Log("工作进程一分钟内异常退出超过 5 次，停止重启"); return 1; }
		Log("工作进程异常退出（%lu），1 秒后重启", code);
		Sleep(1000);
	}
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdline, int) {
	g_inst = inst;
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	LogInit();
	std::wstring args = cmdline ? cmdline : L"";
	if (args.find(L"--worker") != std::wstring::npos) return RunWorker();
	if (args.find(L"--quit") != std::wstring::npos) {   // 卸载程序用：结束正在运行的实例
		if (HWND w = FindWindowW(L"LatexAnywhereMsg", nullptr)) {
			DWORD pid = 0;
			GetWindowThreadProcessId(w, &pid);
			PostMessageW(w, WM_HOTKEY, HK_KILL, 0);
		}
		return 0;
	}
	return RunGuardian();
}
