#include "sysio.h"

#include <atspi/atspi.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <regex>

#include "util.h"

namespace la {

// ---------------- 虚拟键盘 ----------------
bool VirtualKeyboard::Create() {
	const char* be = getenv("LA_PASTE_BACKEND");
	if (be && !strcmp(be, "mutter")) return CreateMutter();
	fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd_ < 0) {
		error = std::string("打不开 /dev/uinput：") + strerror(errno);
		return CreateMutter();
	}
	ioctl(fd_, UI_SET_EVBIT, EV_KEY);
	for (int k : {KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_INSERT, KEY_V, KEY_RIGHT, KEY_END, KEY_SPACE}) ioctl(fd_, UI_SET_KEYBIT, k);
	uinput_setup us{};
	us.id.bustype = BUS_VIRTUAL;
	us.id.vendor = 0x4c41;   // "LA"
	us.id.product = 0x0001;
	snprintf(us.name, sizeof us.name, "LatexAnywhere virtual keyboard");
	if (ioctl(fd_, UI_DEV_SETUP, &us) < 0 || ioctl(fd_, UI_DEV_CREATE) < 0) {
		error = std::string("创建虚拟键盘失败：") + strerror(errno);
		close(fd_);
		fd_ = -1;
		return false;
	}
	return true;
}

void VirtualKeyboard::Emit(int type, int code, int value) {
	input_event ev{};
	ev.type = type;
	ev.code = code;
	ev.value = value;
	if (write(fd_, &ev, sizeof ev) < 0) Log("虚拟键盘写入失败：%s", strerror(errno));
}

bool VirtualKeyboard::CreateMutter() {
	GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!bus) return false;
	GError* e = nullptr;
	GVariant* r = g_dbus_connection_call_sync(bus, "org.gnome.Mutter.RemoteDesktop", "/org/gnome/Mutter/RemoteDesktop", "org.gnome.Mutter.RemoteDesktop",
		"CreateSession", nullptr, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, &e);
	if (r) {
		const char* path = nullptr;
		g_variant_get(r, "(&o)", &path);
		std::string p = path;
		g_variant_unref(r);
		GVariant* s = g_dbus_connection_call_sync(bus, "org.gnome.Mutter.RemoteDesktop", p.c_str(), "org.gnome.Mutter.RemoteDesktop.Session", "Start",
			nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 2000, nullptr, &e);
		if (s) { g_variant_unref(s); session_ = p; }
	}
	if (e) { error += std::string("；Mutter 远程桌面不可用：") + e->message; g_clear_error(&e); }
	g_object_unref(bus);
	return !session_.empty();
}

void VirtualKeyboard::Key(int code, bool down) {
	if (fd_ >= 0) { Emit(EV_KEY, code, down); Emit(EV_SYN, SYN_REPORT, 0); return; }
	GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!bus) return;
	GError* e = nullptr;
	GVariant* r = g_dbus_connection_call_sync(bus, "org.gnome.Mutter.RemoteDesktop", session_.c_str(), "org.gnome.Mutter.RemoteDesktop.Session",
		"NotifyKeyboardKeycode", g_variant_new("(ub)", (guint)code, (gboolean)down), nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, &e);
	if (r) g_variant_unref(r);
	if (e) { Log("Mutter 发键失败：%s", e->message); g_clear_error(&e); }
	g_object_unref(bus);
}

void VirtualKeyboard::Tap(const std::vector<int>& combo) {
	if (!Ok()) return;
	for (int k : combo) { Key(k, true); usleep(6000); }
	for (auto it = combo.rbegin(); it != combo.rend(); ++it) { Key(*it, false); usleep(6000); }
}

// ---------------- 取选区（AT-SPI） ----------------
namespace {
AtspiAccessible* g_focused = nullptr;   // 最近一次拿到焦点的、别的程序里的对象

void OnFocus(AtspiEvent* ev, void*) {
	if (!ev->source || ev->detail1 != 1) return;
	GError* e = nullptr;
	if ((pid_t)atspi_accessible_get_process_id(ev->source, &e) == getpid()) { g_clear_error(&e); return; }   // 自己的框不算
	g_clear_error(&e);
	if (g_focused) g_object_unref(g_focused);
	g_focused = (AtspiAccessible*)g_object_ref(ev->source);
	if (getenv("LA_TRACE")) {
		gchar* n = atspi_accessible_get_role_name(ev->source, nullptr);
		Log("跟踪：AT-SPI 焦点 → %s", n ? n : "?");
		g_free(n);
	}
}

bool HasState(AtspiAccessible* a, AtspiStateType s) {
	AtspiStateSet* ss = atspi_accessible_get_state_set(a);
	if (!ss) return false;
	bool r = atspi_state_set_contains(ss, s);
	g_object_unref(ss);
	return r;
}

// 它所在的窗口此刻是否是活动窗口：焦点事件可能已经过时（换到了不支持无障碍接口的程序，比如没开无障碍的 Electron）
bool WindowActive(AtspiAccessible* a) {
	AtspiAccessible* cur = (AtspiAccessible*)g_object_ref(a);
	for (int depth = 0; cur && depth < 64; depth++) {
		GError* e = nullptr;
		AtspiRole role = atspi_accessible_get_role(cur, &e);
		g_clear_error(&e);
		if (role == ATSPI_ROLE_FRAME || role == ATSPI_ROLE_WINDOW || role == ATSPI_ROLE_DIALOG) {
			bool act = HasState(cur, ATSPI_STATE_ACTIVE);
			g_object_unref(cur);
			return act;
		}
		AtspiAccessible* p = atspi_accessible_get_parent(cur, &e);
		g_clear_error(&e);
		g_object_unref(cur);
		cur = p;
	}
	if (cur) g_object_unref(cur);
	return false;
}
}  // namespace

void SelectionInit() {
	if (atspi_init() > 1) { Log("AT-SPI 初始化失败，取不到选区"); return; }
	atspi_set_timeout(300, 3000);   // 对方程序卡住时最多等 0.3 秒，不拖慢弹框
	GError* e = nullptr;
	AtspiEventListener* l = atspi_event_listener_new(OnFocus, nullptr, nullptr);
	if (!atspi_event_listener_register(l, "object:state-changed:focused", &e)) {
		Log("AT-SPI 焦点监听失败：%s", e ? e->message : "?");
		g_clear_error(&e);
	}
}

std::u16string SelectionGet(bool* fromAtspi) {
	*fromAtspi = false;
	if (!g_focused) { if (getenv("LA_TRACE")) Log("跟踪：AT-SPI 没有焦点对象"); return {}; }
	bool f = HasState(g_focused, ATSPI_STATE_FOCUSED), w = WindowActive(g_focused);
	if (getenv("LA_TRACE")) Log("跟踪：AT-SPI 焦点对象 focused=%d 窗口活动=%d", (int)f, (int)w);
	if (!f || !w) return {};
	AtspiText* t = atspi_accessible_get_text_iface(g_focused);
	if (!t) return {};
	std::u16string out;
	GError* e = nullptr;
	gint n = atspi_text_get_n_selections(t, &e);
	g_clear_error(&e);
	if (n > 0) {
		AtspiRange* r = atspi_text_get_selection(t, 0, &e);
		g_clear_error(&e);
		if (r && r->end_offset > r->start_offset && r->end_offset - r->start_offset <= 4000) {
			gchar* s = atspi_text_get_text(t, r->start_offset, r->end_offset, &e);
			g_clear_error(&e);
			if (s) { out = FromUtf8(s); g_free(s); *fromAtspi = true; }
		}
		if (r) g_free(r);
	}
	g_object_unref(t);
	if (!out.empty() && out.back() == u'\n') out.pop_back();
	return out;
}

// ---------------- 输入法：fcitx5 或 IBus ----------------
// fcitx5：用它的控制接口开关当前输入框的中文。
// IBus（GNOME 默认）：没有公开接口能读写「中/英」状态，照编辑器自动切换插件的通行做法切换全局引擎：
// 进框时记下当前引擎、切到 xkb:us::eng（GNOME 切到拉丁键盘时也是设这个引擎），离开时切回。
namespace {
bool FcitxRunning() {
	GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!bus) return false;
	GVariant* r = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameHasOwner",
		g_variant_new("(s)", "org.fcitx.Fcitx5"), G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 300, nullptr, nullptr);
	gboolean has = FALSE;
	if (r) { g_variant_get(r, "(b)", &has); g_variant_unref(r); }
	g_object_unref(bus);
	return has;
}

void FcitxCall(const char* method, GAsyncReadyCallback cb, gpointer data) {
	GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!bus) { if (cb) cb(nullptr, nullptr, data); return; }
	g_dbus_connection_call(bus, "org.fcitx.Fcitx5", "/controller", "org.fcitx.Fcitx.Controller1", method, nullptr, nullptr,
		G_DBUS_CALL_FLAGS_NO_AUTO_START, 300, nullptr, cb, data);
	g_object_unref(bus);
}

// IBus 有自己的总线，地址写在 ~/.config/ibus/bus/<机器号>-unix-<显示名> 里
GDBusConnection* g_ibus = nullptr;
std::string g_prevEngine;   // 进框前的引擎（非空 = 是我们切走的，离开时要切回）

std::string IbusAddress() {
	if (const char* a = getenv("IBUS_ADDRESS"); a && *a) return a;
	std::string mid;
	if (!ReadFile("/etc/machine-id", &mid) && !ReadFile("/var/lib/dbus/machine-id", &mid)) return {};
	while (!mid.empty() && (mid.back() == '\n' || mid.back() == ' ')) mid.pop_back();
	std::string disp;
	if (const char* w = getenv("WAYLAND_DISPLAY"); w && *w) disp = w;
	else if (const char* d = getenv("DISPLAY"); d && *d) {
		std::string x = d;
		size_t c = x.find(':');
		disp = c == std::string::npos ? "0" : x.substr(c + 1, x.find('.', c) == std::string::npos ? std::string::npos : x.find('.', c) - c - 1);
	} else disp = "wayland-0";
	const char* x = getenv("XDG_CONFIG_HOME");
	std::string f = ((x && *x) ? std::string(x) : Home() + "/.config") + "/ibus/bus/" + mid + "-unix-" + disp, s;
	if (!ReadFile(f, &s)) return {};
	size_t p = s.find("IBUS_ADDRESS=");
	if (p == std::string::npos) return {};
	size_t e = s.find('\n', p);
	return s.substr(p + 13, e == std::string::npos ? std::string::npos : e - p - 13);
}

GDBusConnection* Ibus() {
	if (g_ibus && !g_dbus_connection_is_closed(g_ibus)) return g_ibus;
	if (g_ibus) { g_object_unref(g_ibus); g_ibus = nullptr; }
	std::string addr = IbusAddress();
	if (addr.empty()) return nullptr;
	GError* e = nullptr;
	g_ibus = g_dbus_connection_new_for_address_sync(addr.c_str(),
		(GDBusConnectionFlags)(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr, &e);
	if (e) { Log("连不上 IBus：%s", e->message); g_clear_error(&e); }
	return g_ibus;
}

std::string IbusEngine() {
	GDBusConnection* c = Ibus();
	if (!c) return {};
	GError* e = nullptr;
	GVariant* r = g_dbus_connection_call_sync(c, "org.freedesktop.IBus", "/org/freedesktop/IBus", "org.freedesktop.DBus.Properties", "Get",
		g_variant_new("(ss)", "org.freedesktop.IBus", "GlobalEngine"), nullptr, G_DBUS_CALL_FLAGS_NONE, 300, nullptr, &e);
	if (!r) { g_clear_error(&e); return {}; }
	GVariant* v = g_variant_get_child_value(r, 0);
	while (v && g_variant_is_of_type(v, G_VARIANT_TYPE_VARIANT)) { GVariant* in = g_variant_get_variant(v); g_variant_unref(v); v = in; }
	std::string name;   // IBusEngineDesc：("IBusEngineDesc", {…}, 名字, …)
	if (v && g_variant_is_container(v) && g_variant_n_children(v) > 2) {
		GVariant* n = g_variant_get_child_value(v, 2);
		if (g_variant_is_of_type(n, G_VARIANT_TYPE_STRING)) name = g_variant_get_string(n, nullptr);
		g_variant_unref(n);
	}
	if (v) g_variant_unref(v);
	g_variant_unref(r);
	return name;
}

void IbusSetEngine(const std::string& name) {
	GDBusConnection* c = Ibus();
	if (!c || name.empty()) return;
	g_dbus_connection_call(c, "org.freedesktop.IBus", "/org/freedesktop/IBus", "org.freedesktop.IBus", "SetGlobalEngine", g_variant_new("(s)", name.c_str()),
		nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, [](GObject* src, GAsyncResult* res, gpointer) {
			GError* e = nullptr;
			if (GVariant* r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &e)) g_variant_unref(r);
			if (e) { Log("IBus 切换引擎失败：%s", e->message); g_clear_error(&e); }
		}, nullptr);
}

std::string MarkerFile() { return StateDir() + "/ibus-engine-to-restore"; }
bool IsLatin(const std::string& engine) { return engine.empty() || engine.rfind("xkb:", 0) == 0; }
}  // namespace

void ImeQuery(std::function<void(bool)> done) {
	if (FcitxRunning()) {
		g_prevEngine.clear();
		auto* f = new std::function<void(bool)>(std::move(done));
		FcitxCall("State", [](GObject* src, GAsyncResult* res, gpointer data) {
			auto* f = static_cast<std::function<void(bool)>*>(data);
			gint st = 0;   // 0 未连接 1 英文 2 本地输入
			if (src) {
				GError* e = nullptr;
				if (GVariant* r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &e)) { g_variant_get(r, "(i)", &st); g_variant_unref(r); }
				g_clear_error(&e);
			}
			(*f)(st == 2);
			delete f;
		}, f);
		return;
	}
	std::string eng = IbusEngine();
	g_prevEngine = IsLatin(eng) ? "" : eng;
	done(!g_prevEngine.empty());
}

void ImeSetActive(bool on) {
	if (FcitxRunning()) { FcitxCall(on ? "Activate" : "Deactivate", nullptr, nullptr); return; }
	if (g_prevEngine.empty()) return;
	if (on) {
		IbusSetEngine(g_prevEngine);
		unlink(MarkerFile().c_str());
	} else {
		WriteFile(MarkerFile(), g_prevEngine);   // 万一中途崩溃，重启后据此切回
		IbusSetEngine("xkb:us::eng");
	}
}

void ImeRecover() {
	std::string eng;
	if (!ReadFile(MarkerFile(), &eng) || eng.empty()) return;
	unlink(MarkerFile().c_str());
	if (IsLatin(IbusEngine())) { IbusSetEngine(eng); Log("上次异常退出时输入法停在英文，已切回 %s", eng.c_str()); }
}

// ---------------- GNOME 快捷键 ----------------
namespace {
constexpr char kMediaKeys[] = "org.gnome.settings-daemon.plugins.media-keys";
constexpr char kCustom[] = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr char kBase[] = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/";

bool HaveSchema(const char* id) {
	GSettingsSchemaSource* src = g_settings_schema_source_get_default();
	if (!src) return false;
	GSettingsSchema* s = g_settings_schema_source_lookup(src, id, TRUE);
	if (s) g_settings_schema_unref(s);
	return s != nullptr;
}

// 别的自定义快捷键是否已经用了这个组合
bool TakenByOther(GSettings* mk, const char* binding, const std::string& own) {
	gchar** paths = g_settings_get_strv(mk, "custom-keybindings");
	bool taken = false;
	for (gchar** p = paths; *p && !taken; p++) {
		if (strstr(*p, own.c_str())) continue;
		GSettings* c = g_settings_new_with_path(kCustom, *p);
		gchar* b = g_settings_get_string(c, "binding");
		taken = b && !g_ascii_strcasecmp(b, binding);
		g_free(b);
		g_object_unref(c);
	}
	g_strfreev(paths);
	return taken;
}

void SetBinding(GSettings* mk, const char* id, const char* name, const char* command, const char* binding) {
	std::string path = std::string(kBase) + id + "/";
	gchar** paths = g_settings_get_strv(mk, "custom-keybindings");
	bool have = false;
	for (gchar** p = paths; *p; p++) have = have || path == *p;
	if (!have) {
		GPtrArray* a = g_ptr_array_new();
		for (gchar** p = paths; *p; p++) g_ptr_array_add(a, *p);
		g_ptr_array_add(a, (gpointer)path.c_str());
		g_ptr_array_add(a, nullptr);
		g_settings_set_strv(mk, "custom-keybindings", (const gchar* const*)a->pdata);
		g_ptr_array_free(a, TRUE);
	}
	g_strfreev(paths);
	GSettings* c = g_settings_new_with_path(kCustom, path.c_str());
	g_settings_set_string(c, "name", name);
	g_settings_set_string(c, "command", command);
	g_settings_set_string(c, "binding", binding);
	g_object_unref(c);
}
}  // namespace

bool EnsureShortcuts(const std::string& exe) {
	if (!HaveSchema(kMediaKeys) || !HaveSchema(kCustom)) { Log("不是 GNOME：请在系统设置里把 Alt+L 绑定到「%s --toggle」", exe.c_str()); return true; }
	GSettings* mk = g_settings_new(kMediaKeys);
	bool busy = TakenByOther(mk, "<Alt>l", "latexanywhere-");
	std::string q = "'" + exe + "'";
	if (!busy) SetBinding(mk, "latexanywhere-toggle", "LatexAnywhere", (q + " --toggle").c_str(), "<Alt>l");
	if (!TakenByOther(mk, "<Super><Alt>l", "latexanywhere-")) SetBinding(mk, "latexanywhere-quit", "LatexAnywhere (emergency quit)", (q + " --quit").c_str(), "<Super><Alt>l");
	g_settings_sync();
	g_object_unref(mk);
	return !busy;
}

void RemoveShortcuts() {
	if (!HaveSchema(kMediaKeys)) return;
	GSettings* mk = g_settings_new(kMediaKeys);
	gchar** paths = g_settings_get_strv(mk, "custom-keybindings");
	GPtrArray* a = g_ptr_array_new();
	for (gchar** p = paths; *p; p++) {
		if (strstr(*p, "/latexanywhere-")) {
			GSettings* c = g_settings_new_with_path(kCustom, *p);
			for (const char* k : {"name", "command", "binding"}) g_settings_reset(c, k);
			g_object_unref(c);
		} else {
			g_ptr_array_add(a, *p);
		}
	}
	g_ptr_array_add(a, nullptr);
	g_settings_set_strv(mk, "custom-keybindings", (const gchar* const*)a->pdata);
	g_ptr_array_free(a, TRUE);
	g_strfreev(paths);
	g_settings_sync();
	g_object_unref(mk);
}

// ---------------- 开机自启 ----------------
static std::string AutostartFile() {
	const char* x = getenv("XDG_CONFIG_HOME");
	std::string d = (x && *x) ? x : Home() + "/.config";
	return d + "/autostart/latexanywhere.desktop";
}

bool AutostartOn() { return access(AutostartFile().c_str(), F_OK) == 0; }

void AutostartSet(bool on, const std::string& exe) {
	std::string f = AutostartFile();
	if (!on) { unlink(f.c_str()); return; }
	mkdir((f.substr(0, f.rfind('/'))).c_str(), 0700);
	WriteFile(f, "[Desktop Entry]\nType=Application\nName=LatexAnywhere\nExec=\"" + exe + "\"\nIcon=latexanywhere\nX-GNOME-Autostart-enabled=true\nNoDisplay=true\n");
}

// ---------------- 设置：优先 Obsidian 里 Latex Suite 的设置 ----------------
std::string FindObsidianSettings(std::string* from) {
	std::string own;
	if (ReadFile(ConfigDir() + "/settings.json", &own) && !own.empty()) { *from = "settings.json"; return own; }
	const char* x = getenv("XDG_CONFIG_HOME");
	std::string cfg = (x && *x) ? x : Home() + "/.config";
	// 普通安装、Flatpak、Snap 三种 Obsidian 的位置
	for (const std::string& reg : {cfg + "/obsidian/obsidian.json", Home() + "/.var/app/md.obsidian.Obsidian/config/obsidian/obsidian.json",
			 Home() + "/snap/obsidian/current/.config/obsidian/obsidian.json"}) {
		std::string s;
		if (!ReadFile(reg, &s)) continue;
		static const std::regex re(R"re("path"\s*:\s*"((?:[^"\\]|\\.)*)")re");
		for (std::sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
			std::string path = (*it)[1].str(), unesc;
			for (size_t i = 0; i < path.size(); i++) unesc += (path[i] == '\\' && i + 1 < path.size()) ? path[++i] : path[i];
			std::string f = unesc + "/.obsidian/plugins/obsidian-latex-suite/data.json", data;
			if (ReadFile(f, &data) && !data.empty()) { *from = f; return data; }
		}
	}
	return {};
}

bool SystemDark() {
	// 首选 GTK 自己解析出的配色（GTK 4.20 起有，来自桌面门户），保证与窗口标题栏等系统部件一致
	if (GtkSettings* gs = gtk_settings_get_default()) {
		if (g_object_class_find_property(G_OBJECT_GET_CLASS(gs), "gtk-interface-color-scheme")) {
			int v = 0;
			g_object_get(gs, "gtk-interface-color-scheme", &v, nullptr);
			if (v == 2) return true;    // GTK_INTERFACE_COLOR_SCHEME_DARK
			if (v == 3) return false;   // GTK_INTERFACE_COLOR_SCHEME_LIGHT
		}
		gboolean prefer = FALSE;
		g_object_get(gs, "gtk-application-prefer-dark-theme", &prefer, nullptr);
		if (prefer) return true;
	}
	if (HaveSchema("org.gnome.desktop.interface")) {
		GSettings* s = g_settings_new("org.gnome.desktop.interface");
		bool dark = false;
		GSettingsSchema* sc = nullptr;
		g_object_get(s, "settings-schema", &sc, nullptr);
		if (sc && g_settings_schema_has_key(sc, "color-scheme")) {
			gchar* v = g_settings_get_string(s, "color-scheme");
			dark = v && !strcmp(v, "prefer-dark");
			g_free(v);
		}
		if (!dark) {
			gchar* t = g_settings_get_string(s, "gtk-theme");
			dark = t && g_str_has_suffix(t, "-dark");
			g_free(t);
		}
		if (sc) g_settings_schema_unref(sc);
		g_object_unref(s);
		return dark;
	}
	return false;
}

}  // namespace la
