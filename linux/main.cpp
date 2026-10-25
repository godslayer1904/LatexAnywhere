// LatexAnywhere（Linux）：任意输入框按 Alt+L 弹出公式框，用原版 Latex Suite 的方式打公式。
// 进程结构同 Windows 版：守护进程（无窗口）启动工作进程并在其崩溃时自动重启；工作进程负责托盘、公式框。
// 快捷键由 GNOME 自定义快捷键调用「latexanywhere --toggle」，经 D-Bus 交给正在运行的工作进程（GtkApplication 单实例）。
#include <gtk/gtk.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>

#include "box.h"
#include "help.h"
#include "i18n.h"
#include "js_engine.h"
#include "sysio.h"
#include "tray.h"
#include "util.h"

// 构建时嵌进程序的字节码与图标（linux/res.S）
extern "C" const char la_engine_start[], la_engine_end[], la_math_start[], la_math_end[], la_icon_start[], la_icon_end[];

namespace {

using namespace la;

constexpr char kAppId[] = "io.github.godslayer1904.LatexAnywhere";
constexpr int kExitQuit = 0;   // 用户退出：守护进程也退出

GtkApplication* g_app;
Engine* g_eng;
Box* g_box;
VirtualKeyboard g_kb;
Tray g_tray;
bool g_restarted = false;

std::string Exe() {
	// 装成 AppImage 时 /proc/self/exe 指向临时挂载点，要用 APPIMAGE 才稳定
	const char* ai = getenv("APPIMAGE");
	return ai && *ai ? ai : ExePath();
}

bool PrimaryRunning() {
	GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!bus) return false;
	GVariant* r = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner",
		g_variant_new("(s)", kAppId), G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, nullptr);
	gboolean has = FALSE;
	if (r) { g_variant_get(r, "(b)", &has); g_variant_unref(r); }
	g_object_unref(bus);
	return has;
}

void Quit(const char* why) {
	Log("退出：%s", why);
	g_tray.Hide();
	g_application_quit(G_APPLICATION(g_app));
}

// ---------- 托盘 ----------
void BuildMenu();

void OnLanguage(const std::string& code) {
	std::wstring w(code.begin(), code.end());
	SetLanguage(w.c_str());
	BuildMenu();
	if (HelpOpen()) { CloseHelp(); ShowHelp(g_app, SystemDark()); }   // 说明窗口开着：按新语言重开
	Log("语言：%s", WideToUtf8(Lang().code).c_str());
}

void BuildMenu() {
	const Strings& L = Lang();
	std::vector<TrayItem> m;
	m.push_back({WideToUtf8(L.trayHelp), TrayItem::Normal, false, [] { ShowHelp(g_app, SystemDark()); }});
	m.push_back({WideToUtf8(L.autostartLinux), TrayItem::Check, AutostartOn(), [] {
		bool on = !AutostartOn();
		AutostartSet(on, Exe());
		Log("开机自启：%s", on ? "开" : "关");
		BuildMenu();
	}});
	TrayItem lang{WideToUtf8(L.trayLanguage)};
	std::string cur = WideToUtf8(LanguageSetting());
	lang.children.push_back({WideToUtf8(L.followSystem), TrayItem::Radio, cur.empty(), [] { OnLanguage(""); }});
	lang.children.push_back({"", TrayItem::Separator});
	for (int i = 0; i < LanguageCount(); i++) {   // 每种语言都用它自己的文字显示，看不懂当前界面也能找到
		std::string code = WideToUtf8(LanguageAt(i).code);
		lang.children.push_back({WideToUtf8(LanguageAt(i).nativeName), TrayItem::Radio, !cur.empty() && !g_ascii_strcasecmp(cur.c_str(), code.c_str()),
			[code] { OnLanguage(code); }});
	}
	m.push_back(std::move(lang));
	m.push_back({"", TrayItem::Separator});
	m.push_back({WideToUtf8(L.trayQuit), TrayItem::Normal, false, [] { Quit("托盘退出"); }});
	g_tray.SetMenu(std::move(m));
	g_tray.SetTooltip(WideToUtf8(L.trayTip));
}

std::string InstallIcon() {
	// 图标放到用户图标目录，托盘和 .desktop 都按名字 latexanywhere 找
	std::string dir = Home() + "/.local/share/icons/hicolor/scalable/apps";
	g_mkdir_with_parents(dir.c_str(), 0755);
	std::string f = dir + "/latexanywhere.svg", have, want(la_icon_start, la_icon_end - la_icon_start);
	if (!ReadFile(f, &have) || have != want) WriteFile(f, want);
	return dir;
}

void CreateTray() {
	std::string dir = InstallIcon();
	if (!g_tray.Create("latexanywhere", dir, std::string(la_icon_start, la_icon_end - la_icon_start))) return;
	BuildMenu();
}

// ---------- 自我监控：内存或 CPU 异常就退出，由守护进程重启 ----------
long AnonKb() {
	std::string s;
	if (!ReadFile("/proc/self/status", &s)) return 0;
	size_t p = s.find("RssAnon:");
	return p == std::string::npos ? 0 : strtol(s.c_str() + p + 8, nullptr, 10);
}

void Watchdog() {
	double lastCpu = 0, lastT = NowMs();
	int hot = 0;
	for (;;) {
		sleep(5);
		rusage ru;
		getrusage(RUSAGE_SELF, &ru);
		double cpu = ru.ru_utime.tv_sec * 1e3 + ru.ru_utime.tv_usec / 1e3 + ru.ru_stime.tv_sec * 1e3 + ru.ru_stime.tv_usec / 1e3;
		double now = NowMs();
		double busy = (cpu - lastCpu) / (now - lastT);   // 占一个核的比例
		lastCpu = cpu;
		lastT = now;
		hot = busy > 0.9 ? hot + 1 : 0;
		long kb = AnonKb();
		if (kb > 400 * 1024) { Log("自我监控：内存超限 %ld MB，退出重启", kb >> 10); _exit(3); }
		if (hot >= 3) { Log("自我监控：CPU 持续占满，退出重启"); _exit(4); }
	}
}

void OnCrash(int sig) {
	// 信号处理里只用异步安全的调用：直接写日志文件
	static const char msg[] = "崩溃：收到致命信号，守护进程会重启\n";
	std::string path = StateDir() + "/log.txt";   // StateDir 已在启动时建好，这里只拼字符串
	int fd = open(path.c_str(), O_WRONLY | O_APPEND);
	if (fd >= 0) { ssize_t n = write(fd, msg, sizeof msg - 1); (void)n; close(fd); }
	signal(sig, SIG_DFL);
	raise(sig);
}

void Startup(GApplication* app, gpointer) {
	double t0 = NowMs();
	std::string from = "默认设置";
	std::string settings = FindObsidianSettings(&from);
	std::vector<Engine::Script> scripts = {{la_engine_start, (size_t)(la_engine_end - la_engine_start), true, "engine"},
		{la_math_start, (size_t)(la_math_end - la_math_start), true, "math"}};
	g_eng = new Engine;
	std::string err;
	if (!g_eng->Init(scripts, settings, &err)) {
		Log("引擎初始化失败（设置来源 %s）：%s", from.c_str(), err.c_str());
		delete g_eng;
		g_eng = new Engine;   // 设置文件有问题：退回默认设置，不能因此用不了
		if (settings.empty() || !g_eng->Init(scripts, "", &err)) { Log("默认设置也失败：%s", err.c_str()); _exit(2); }
		from = "默认设置（Obsidian 设置读取失败）";
	}
	std::u16string warm;   // 预热：第一次弹框不用再等 MathJax 初始化
	g_eng->CallString("LatexAnywhereMath", "previewPacked", u"$x^2+\\frac{1}{2}$", &warm, nullptr);
	Log("就绪：引擎 %.0fms，设置来源 %s，JS 内存 %.1f MB", NowMs() - t0, from.c_str(), g_eng->MemoryUsed() / 1048576.0);

	if (!g_kb.Create()) Log("%s：上屏需要手动 Ctrl+V（安装脚本会加 udev 规则解决）", g_kb.error.c_str());
	else Log("上屏通道：%s%s", g_kb.Backend(), g_kb.error.c_str());
	SelectionInit();
	ImeRecover();
	g_box = new Box;
	g_box->Create(GTK_APPLICATION(app), g_eng, &g_kb);
	g_box->SetDark(SystemDark());
	CreateTray();

	if (!getenv("LA_NO_SHORTCUTS")) {
		if (EnsureShortcuts(Exe())) Log("Alt+L 已注册（GNOME 自定义快捷键）");
		else {
			Log("Alt+L 已被其他快捷键占用");
			GNotification* n = g_notification_new("LatexAnywhere");
			g_notification_set_body(n, WideToUtf8(Lang().hotkeyBusy).c_str());
			g_application_send_notification(app, "hotkey", n);
			g_object_unref(n);
		}
	}
	// 跟随系统深色 / 浅色
	GSettingsSchemaSource* src = g_settings_schema_source_get_default();
	if (GSettingsSchema* sc = src ? g_settings_schema_source_lookup(src, "org.gnome.desktop.interface", TRUE) : nullptr) {
		g_settings_schema_unref(sc);
		GSettings* s = g_settings_new("org.gnome.desktop.interface");
		g_signal_connect(s, "changed", G_CALLBACK(+[](GSettings*, const char* key, gpointer) {
			if (!strcmp(key, "color-scheme") || !strcmp(key, "gtk-theme")) g_box->SetDark(SystemDark());
		}), nullptr);
	}
	if (GtkSettings* gs = gtk_settings_get_default())   // GTK 自己的配色变了（门户通知）也跟着换
		for (const char* prop : {"notify::gtk-interface-color-scheme", "notify::gtk-application-prefer-dark-theme"})
			if (g_signal_lookup("notify", G_OBJECT_TYPE(gs))) g_signal_connect(gs, prop, G_CALLBACK(+[](GObject*, GParamSpec*, gpointer) { g_box->SetDark(SystemDark()); }), nullptr);
	std::thread(Watchdog).detach();
	g_application_hold(app);   // 没有窗口也常驻
	// 首次启动：弹出欢迎（使用说明）窗口，直到用户勾选「不再显示」；崩溃后由守护进程重启时不弹
	if (!WelcomeHidden() && !g_restarted) ShowHelp(GTK_APPLICATION(app), SystemDark(), true);
}

int CommandLine(GApplication*, GApplicationCommandLine* cl, gpointer) {
	int argc;
	gchar** argv = g_application_command_line_get_arguments(cl, &argc);
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--toggle")) g_box->Toggle();
		else if (!strcmp(argv[i], "--quit")) Quit("命令行 --quit（紧急退出快捷键或卸载）");
		else if (!strcmp(argv[i], "--help")) ShowHelp(g_app, SystemDark(), false);
	}
	g_strfreev(argv);
	return 0;
}

int RunWorker(int argc, char** argv) {
	for (int s : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) signal(s, OnCrash);
	for (int i = 1; i < argc; i++) g_restarted = g_restarted || !strcmp(argv[i], "--restarted");
	g_set_prgname("latexanywhere");
	g_app = gtk_application_new(kAppId, G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect(g_app, "startup", G_CALLBACK(Startup), nullptr);
	g_signal_connect(g_app, "command-line", G_CALLBACK(CommandLine), nullptr);
	g_application_register(G_APPLICATION(g_app), nullptr, nullptr);
	if (g_application_get_is_remote(G_APPLICATION(g_app))) return kExitQuit;   // 已经有一个在跑
	char* args[] = {argv[0], nullptr};   // 自己的 --worker 不当作命令执行
	int r = g_application_run(G_APPLICATION(g_app), 1, args);
	return r == 0 ? kExitQuit : r;
}

// 把 --toggle / --quit / --help 交给正在运行的工作进程（含桌面给的激活令牌，框才能拿到焦点）
int RunLauncher(int argc, char** argv) {
	GtkApplication* app = gtk_application_new(kAppId, G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect(app, "command-line", G_CALLBACK(+[](GApplication*, GApplicationCommandLine*, gpointer) { return 0; }), nullptr);
	g_application_register(G_APPLICATION(app), nullptr, nullptr);
	if (!g_application_get_is_remote(G_APPLICATION(app))) return 1;   // 刚好没在跑（不应发生：调用前已检查）
	return g_application_run(G_APPLICATION(app), argc, argv);
}

int ShowHelpStandalone(bool welcome) {
	GtkApplication* app = gtk_application_new(nullptr, G_APPLICATION_NON_UNIQUE);
	g_signal_connect(app, "activate", G_CALLBACK(+[](GApplication* a, gpointer w) { ShowHelp(GTK_APPLICATION(a), SystemDark(), w != nullptr); }),
		welcome ? (gpointer)1 : nullptr);
	char* args[] = {(char*)"latexanywhere", nullptr};
	return g_application_run(G_APPLICATION(app), 1, args);
}

// 守护进程：启动工作进程；工作进程异常退出就重启（一分钟内最多 5 次，防止反复崩溃拖垮电脑）
int RunGuardian() {
	std::string lock = StateDir() + "/guardian.lock";
	int lfd = open(lock.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (lfd < 0 || flock(lfd, LOCK_EX | LOCK_NB) != 0) return 0;   // 已有守护进程
	// 旧实例可能正在退出（升级、重装时紧接着启动）：最多等 3 秒，确实还在跑才放弃
	for (int i = 0; i < 30 && PrimaryRunning(); i++) usleep(100000);
	if (PrimaryRunning()) return 0;
	std::string exe = ExePath();
	bool restarted = false;
	double window = NowMs();
	int restarts = 0;
	for (;;) {
		pid_t pid = fork();
		if (pid < 0) { Log("启动工作进程失败：%s", strerror(errno)); return 1; }
		if (pid == 0) {
			const char* args[] = {exe.c_str(), "--worker", restarted ? "--restarted" : nullptr, nullptr};   // 重启的不再弹欢迎窗口
			execv(exe.c_str(), (char* const*)args);
			_exit(127);
		}
		int st = 0;
		while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
		if (WIFEXITED(st) && WEXITSTATUS(st) == kExitQuit) return 0;
		if (NowMs() - window > 60000) { window = NowMs(); restarts = 0; }
		if (++restarts > 5) { Log("工作进程一分钟内异常退出超过 5 次，停止重启"); return 1; }
		if (WIFSIGNALED(st)) Log("工作进程被信号 %d 结束，1 秒后重启", WTERMSIG(st));
		else Log("工作进程异常退出（%d），1 秒后重启", WEXITSTATUS(st));
		restarted = true;
		sleep(1);
	}
}

void Detach() {
	// 从终端或快捷键启动时脱离调用者：不随终端关闭而退出
	if (fork() > 0) _exit(0);
	setsid();
}

}  // namespace

int main(int argc, char** argv) {
	LogInit();
	bool worker = false, toggle = false, quit = false, help = false, welcome = false, foreground = false, uninstall = false;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		worker |= a == "--worker";
		toggle |= a == "--toggle";
		quit |= a == "--quit";
		help |= a == "--help";
		welcome |= a == "--welcome";
		foreground |= a == "--foreground";
		uninstall |= a == "--uninstall-cleanup";
	}
	if (worker) return RunWorker(argc, argv);
	if (uninstall) {   // 卸载脚本用：去掉快捷键和自启项
		RemoveShortcuts();
		AutostartSet(false, "");
		return 0;
	}
	if (toggle || quit || help) {
		if (PrimaryRunning()) {
			int r = RunLauncher(argc, argv);
			if (quit)   // 等它真正退出再返回：安装、卸载脚本紧接着就要替换文件或重新启动
				for (int i = 0; i < 50 && PrimaryRunning(); i++) usleep(100000);
			return r;
		}
		if (help) return ShowHelpStandalone(welcome);
		if (quit) return 0;
		// 按了 Alt+L 但程序没在运行（比如被手动退出了）：直接启动它
		Log("Alt+L：程序没在运行，启动之");
	}
	if (!foreground) Detach();
	return RunGuardian();
}
