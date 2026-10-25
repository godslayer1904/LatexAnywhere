#include "help.h"

#include <unistd.h>
#include <cstring>
#include <string>

#include "i18n.h"
#include "util.h"

namespace la {
namespace {

GtkWidget* g_win = nullptr;
GtkWidget* g_check = nullptr;
bool g_rtl = false;

std::string HideFile() { return ConfigDir() + "/hide-welcome"; }

// Windows 专有的说法换成 Linux 的
std::string Linuxify(const std::string& s) {
	std::string o = s;
	for (size_t p; (p = o.find("‹Win›")) != std::string::npos;) o.replace(p, strlen("‹Win›"), "‹Super›");
	return o;
}

// ‹…› 画成键帽，其余转义
std::string Markup(const std::string& s, bool dark) {
	std::string o;
	const std::string open = "‹", close = "›";
	size_t i = 0;
	while (i < s.size()) {
		size_t a = s.find(open, i);
		if (a == std::string::npos) { gchar* e = g_markup_escape_text(s.c_str() + i, -1); o += e; g_free(e); break; }
		gchar* e = g_markup_escape_text(s.substr(i, a - i).c_str(), -1);
		o += e;
		g_free(e);
		size_t b = s.find(close, a);
		if (b == std::string::npos) b = s.size();
		gchar* k = g_markup_escape_text(s.substr(a + open.size(), b - a - open.size()).c_str(), -1);
		o += std::string("<span font_family=\"monospace\" background=\"") + (dark ? "#3a3a3a" : "#e8e8ec") + "\"> " + k + " </span>";
		g_free(k);
		i = b + close.size();
	}
	return o;
}

GtkWidget* Dir(GtkWidget* w) {
	gtk_widget_set_direction(w, g_rtl ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
	return w;
}

GtkWidget* Label(const std::string& markup, const char* cls = nullptr) {
	GtkWidget* l = Dir(gtk_label_new(nullptr));
	gtk_label_set_markup(GTK_LABEL(l), markup.c_str());
	gtk_label_set_wrap(GTK_LABEL(l), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_xalign(GTK_LABEL(l), g_rtl ? 1.f : 0.f);
	gtk_label_set_selectable(GTK_LABEL(l), FALSE);
	if (cls) gtk_widget_add_css_class(l, cls);
	return l;
}

void SaveChoice() {
	if (!g_check) return;
	if (gtk_check_button_get_active(GTK_CHECK_BUTTON(g_check))) WriteFile(HideFile(), "1\n");
	else unlink(HideFile().c_str());
}

}  // namespace

bool WelcomeHidden() { return access(HideFile().c_str(), F_OK) == 0; }
bool HelpOpen() { return g_win != nullptr; }
void CloseHelp() { if (g_win) gtk_window_destroy(GTK_WINDOW(g_win)); }

void ShowHelp(GtkApplication* app, bool dark, bool welcome) {
	if (g_win) { gtk_window_present(GTK_WINDOW(g_win)); return; }
	const Strings& L = Lang();
	g_rtl = L.rtl;
	auto u = [](const wchar_t* w) { return Linuxify(WideToUtf8(w)); };

	g_win = Dir(gtk_application_window_new(app));
	gtk_window_set_title(GTK_WINDOW(g_win), u(L.helpWindow).c_str());
	gtk_window_set_default_size(GTK_WINDOW(g_win), 680, 760);
	gtk_widget_add_css_class(g_win, "la-help");

	GtkCssProvider* css = gtk_css_provider_new();
	std::string bg = dark ? "#1e1e1e" : "#ffffff", fg = dark ? "#dadada" : "#222222", card = dark ? "#2a2a2a" : "#f4f4f7", line = dark ? "#3a3a3a" : "#dddddd";
	std::string c =
		"window.la-help, window.la-help scrolledwindow, window.la-help viewport { background: " + bg + "; color: " + fg + "; }\n"
		".la-title { font-size: 22px; font-weight: bold; }\n.la-sub { opacity: .75; }\n"
		".la-h { font-size: 16px; font-weight: bold; margin-top: 14px; }\n"
		".la-tip { background: " + card + "; border-radius: 6px; padding: 10px; }\n"
		".la-cell { padding: 6px 8px; border-bottom: 1px solid " + line + "; }\n.la-head { font-weight: bold; }\n"
		".la-footer { background: " + card + "; border-top: 1px solid " + line + "; padding: 10px 16px; }\n";
	gtk_css_provider_load_from_string(css, c.c_str());
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);

	GtkWidget* outer = Dir(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
	gtk_window_set_child(GTK_WINDOW(g_win), outer);
	GtkWidget* sc = Dir(gtk_scrolled_window_new());
	gtk_widget_set_vexpand(sc, TRUE);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sc), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_box_append(GTK_BOX(outer), sc);
	GtkWidget* body = Dir(gtk_box_new(GTK_ORIENTATION_VERTICAL, 8));
	for (auto f : {gtk_widget_set_margin_start, gtk_widget_set_margin_end}) f(body, 28);
	gtk_widget_set_margin_top(body, 24);
	gtk_widget_set_margin_bottom(body, 24);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sc), body);

	auto add = [&](GtkWidget* w) { gtk_box_append(GTK_BOX(body), w); };
	auto esc = [](const std::string& s) { gchar* e = g_markup_escape_text(s.c_str(), -1); std::string r = e; g_free(e); return r; };
	add(Label(esc(u(L.title)), "la-title"));
	add(Label(esc(u(L.subtitle)), "la-sub"));
	add(Label(Markup(u(L.helpKeyLinux), dark), "la-tip"));   // 放在最前面：每次看说明都知道怎么再调出来
	add(Label(esc(u(L.hBasic)), "la-h"));
	for (int i = 0; i < 4; i++) add(Label(std::to_string(i + 1) + ".  " + Markup(u(L.basic[i]), dark)));
	add(Label(Markup(u(L.tipSelection), dark), "la-tip"));

	add(Label(esc(u(L.hInsert)), "la-h"));
	GtkWidget* grid = Dir(gtk_grid_new());
	const wchar_t* head[3] = {L.colKey, L.colInserts, L.colFor};
	for (int c = 0; c < 3; c++) {
		GtkWidget* h = Label(esc(u(head[c])), "la-cell");
		gtk_widget_add_css_class(h, "la-head");
		gtk_widget_set_hexpand(h, TRUE);
		gtk_grid_attach(GTK_GRID(grid), h, c, 0, 1, 1);
	}
	int row = 1;
	for (int r = 0; r < 6; r++) {
		if (r == 3) continue;   // Word / PowerPoint 原生公式：Linux 上没有
		const HelpRow& hr = L.rows[r];
		const wchar_t* cells[3] = {hr.keys, hr.inserts, hr.forWhat};
		for (int c = 0; c < 3; c++) gtk_grid_attach(GTK_GRID(grid), Label(Markup(u(cells[c]), dark), "la-cell"), c, row, 1, 1);
		row++;
	}
	add(grid);

	add(Label(esc(u(L.hIme)), "la-h"));
	for (const wchar_t* s : L.ime) add(Label("•  " + Markup(u(s), dark)));
	add(Label(esc(u(L.hBox)), "la-h"));
	add(Label("•  " + Markup(u(L.box[0]), dark)));
	add(Label("•  " + Markup(u(L.box[1]), dark)));
	add(Label("•  " + Markup(u(L.box[2]), dark)));
	add(Label(esc(u(L.hSafety)), "la-h"));
	std::string tray = u(L.trayLinux);
	for (auto [k, v] : {std::pair<const char*, const wchar_t*>{"%1", L.autostartLinux}, {"%2", L.trayQuit}})
		if (size_t p = tray.find(k); p != std::string::npos) tray.replace(p, 2, WideToUtf8(v));
	add(Label("•  " + Markup(tray, dark)));
	add(Label("•  " + Markup(u(L.safety[1]), dark)));
	add(Label("•  " + Markup(u(L.safety[2]), dark)));

	g_check = nullptr;
	if (welcome) {
		GtkWidget* foot = Dir(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12));
		gtk_widget_add_css_class(foot, "la-footer");
		g_check = Dir(gtk_check_button_new_with_label(u(L.dontShow).c_str()));
		gtk_widget_set_hexpand(g_check, TRUE);
		gtk_box_append(GTK_BOX(foot), g_check);
		GtkWidget* btn = Dir(gtk_button_new_with_label(u(L.startUsing).c_str()));
		gtk_widget_add_css_class(btn, "suggested-action");
		g_signal_connect_swapped(btn, "clicked", G_CALLBACK(gtk_window_destroy), g_win);
		gtk_box_append(GTK_BOX(foot), btn);
		gtk_box_append(GTK_BOX(outer), foot);
		gtk_window_set_default_widget(GTK_WINDOW(g_win), btn);
	}
	// Esc 关闭；欢迎模式下 Enter 也能关
	GtkEventController* k = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(k, GTK_PHASE_CAPTURE);   // 先于复选框处理：Enter 始终是「开始使用」
	g_signal_connect(k, "key-pressed", G_CALLBACK(+[](GtkEventControllerKey*, guint kv, guint, GdkModifierType, gpointer) -> gboolean {
		if (kv == GDK_KEY_Escape || (g_check && (kv == GDK_KEY_Return || kv == GDK_KEY_KP_Enter))) { gtk_window_destroy(GTK_WINDOW(g_win)); return TRUE; }
		return FALSE;
	}), nullptr);
	gtk_widget_add_controller(g_win, k);
	g_signal_connect(g_win, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer) {
		SaveChoice();   // 点按钮、按 Enter / Esc、点 × 都按当前勾选状态保存
		g_win = nullptr;
		g_check = nullptr;
	}), nullptr);
	gtk_window_present(GTK_WINDOW(g_win));
	if (g_check) gtk_widget_grab_focus(g_check);   // 焦点放在复选框：空格勾选，Enter 开始使用
}

}  // namespace la
