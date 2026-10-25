#include "tray.h"

#include <gio/gio.h>
#include <librsvg/rsvg.h>
#include <unistd.h>
#include <cstdint>

#include "util.h"

namespace la {

namespace {
const char kXml[] = R"XML(<node>
<interface name="org.kde.StatusNotifierItem">
 <method name="Activate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
 <method name="SecondaryActivate"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
 <method name="ContextMenu"><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
 <method name="Scroll"><arg type="i" direction="in"/><arg type="s" direction="in"/></method>
 <property name="Category" type="s" access="read"/><property name="Id" type="s" access="read"/>
 <property name="Title" type="s" access="read"/><property name="Status" type="s" access="read"/>
 <property name="WindowId" type="i" access="read"/><property name="IconName" type="s" access="read"/>
 <property name="IconThemePath" type="s" access="read"/><property name="IconPixmap" type="a(iiay)" access="read"/>
 <property name="OverlayIconName" type="s" access="read"/><property name="OverlayIconPixmap" type="a(iiay)" access="read"/>
 <property name="AttentionIconName" type="s" access="read"/><property name="AttentionIconPixmap" type="a(iiay)" access="read"/>
 <property name="AttentionMovieName" type="s" access="read"/><property name="ToolTip" type="(sa(iiay)ss)" access="read"/>
 <property name="ItemIsMenu" type="b" access="read"/><property name="Menu" type="o" access="read"/>
 <signal name="NewTitle"/><signal name="NewIcon"/><signal name="NewToolTip"/><signal name="NewStatus"><arg type="s"/></signal>
</interface>
<interface name="com.canonical.dbusmenu">
 <method name="GetLayout"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="as" direction="in"/>
  <arg type="u" direction="out"/><arg type="(ia{sv}av)" direction="out"/></method>
 <method name="GetGroupProperties"><arg type="ai" direction="in"/><arg type="as" direction="in"/><arg type="a(ia{sv})" direction="out"/></method>
 <method name="GetProperty"><arg type="i" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="out"/></method>
 <method name="Event"><arg type="i" direction="in"/><arg type="s" direction="in"/><arg type="v" direction="in"/><arg type="u" direction="in"/></method>
 <method name="EventGroup"><arg type="a(isvu)" direction="in"/><arg type="ai" direction="out"/></method>
 <method name="AboutToShow"><arg type="i" direction="in"/><arg type="b" direction="out"/></method>
 <method name="AboutToShowGroup"><arg type="ai" direction="in"/><arg type="ai" direction="out"/><arg type="ai" direction="out"/></method>
 <property name="Version" type="u" access="read"/><property name="TextDirection" type="s" access="read"/>
 <property name="Status" type="s" access="read"/><property name="IconThemePath" type="as" access="read"/>
 <signal name="ItemsPropertiesUpdated"><arg type="a(ia{sv})"/><arg type="a(ias)"/></signal>
 <signal name="LayoutUpdated"><arg type="u"/><arg type="i"/></signal>
 <signal name="ItemActivationRequested"><arg type="i"/><arg type="u"/></signal>
</interface></node>)XML";
}  // namespace

struct Tray::Impl {
	GDBusConnection* bus = nullptr;
	GDBusNodeInfo* node = nullptr;
	std::string svc, iconName, iconDir, title;
	GVariant* pixmaps = nullptr;
	std::vector<TrayItem> items;
	guint32 rev = 1;
	bool hidden = false;

	TrayItem* Find(std::vector<TrayItem>& v, int id) {
		for (TrayItem& it : v) {
			if (it.id == id) return &it;
			if (TrayItem* c = Find(it.children, id)) return c;
		}
		return nullptr;
	}
	GVariant* Props(const TrayItem* it) {
		GVariantBuilder b;
		g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
		if (!it) {   // 根节点
			g_variant_builder_add(&b, "{sv}", "children-display", g_variant_new_string("submenu"));
			return g_variant_builder_end(&b);
		}
		if (it->kind == TrayItem::Separator) {
			g_variant_builder_add(&b, "{sv}", "type", g_variant_new_string("separator"));
			return g_variant_builder_end(&b);
		}
		g_variant_builder_add(&b, "{sv}", "label", g_variant_new_string(it->label.c_str()));
		g_variant_builder_add(&b, "{sv}", "enabled", g_variant_new_boolean(TRUE));
		if (it->kind == TrayItem::Check || it->kind == TrayItem::Radio) {
			g_variant_builder_add(&b, "{sv}", "toggle-type", g_variant_new_string(it->kind == TrayItem::Check ? "checkmark" : "radio"));
			g_variant_builder_add(&b, "{sv}", "toggle-state", g_variant_new_int32(it->on ? 1 : 0));
		}
		if (!it->children.empty()) g_variant_builder_add(&b, "{sv}", "children-display", g_variant_new_string("submenu"));
		return g_variant_builder_end(&b);
	}
	GVariant* Layout(const TrayItem* it, int id, int depth) {
		GVariantBuilder kids;
		g_variant_builder_init(&kids, G_VARIANT_TYPE("av"));
		const std::vector<TrayItem>& v = it ? it->children : items;
		if (depth != 0)
			for (const TrayItem& c : v) g_variant_builder_add(&kids, "v", Layout(&c, c.id, depth < 0 ? -1 : depth - 1));
		return g_variant_new("(i@a{sv}av)", id, Props(it), &kids);
	}
	void Number(std::vector<TrayItem>& v, int& next) {
		for (TrayItem& it : v) { it.id = next++; Number(it.children, next); }
	}
	void Click(int id) {
		TrayItem* it = Find(items, id);
		if (!it || !it->activate) return;
		auto* f = new std::function<void()>(it->activate);
		// 放到下一轮主循环执行：处理函数可能重建菜单，不能在遍历菜单时改它
		g_idle_add([](gpointer p) -> gboolean {
			auto* f = static_cast<std::function<void()>*>(p);
			(*f)();
			delete f;
			return G_SOURCE_REMOVE;
		}, f);
	}
	void Register() {
		g_dbus_connection_call(bus, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem",
			g_variant_new("(s)", svc.c_str()), nullptr, G_DBUS_CALL_FLAGS_NO_AUTO_START, 3000, nullptr,
			[](GObject* src, GAsyncResult* res, gpointer) {
				GError* e = nullptr;
				if (GVariant* r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &e)) { g_variant_unref(r); Log("托盘图标已注册"); }
				if (e) { Log("托盘注册失败：%s", e->message); g_clear_error(&e); }
			}, nullptr);
	}
};

static void OnCall(GDBusConnection*, const char*, const char*, const char* iface, const char* method, GVariant* params, GDBusMethodInvocation* inv, gpointer data) {
	auto* p = static_cast<Tray::Impl*>(data);
	if (!g_strcmp0(iface, "org.kde.StatusNotifierItem")) { g_dbus_method_invocation_return_value(inv, nullptr); return; }   // 左键 / 右键都由宿主弹菜单
	if (!g_strcmp0(method, "GetLayout")) {
		gint parent, depth;
		g_variant_get(params, "(ii@as)", &parent, &depth, nullptr);
		TrayItem* it = parent == 0 ? nullptr : p->Find(p->items, parent);
		g_dbus_method_invocation_return_value(inv, g_variant_new("(u@(ia{sv}av))", p->rev, p->Layout(it, parent, depth)));
	} else if (!g_strcmp0(method, "GetGroupProperties")) {
		GVariantIter* ids;
		g_variant_get(params, "(ai@as)", &ids, nullptr);
		GVariantBuilder b;
		g_variant_builder_init(&b, G_VARIANT_TYPE("a(ia{sv})"));
		gint id;
		while (g_variant_iter_next(ids, "i", &id)) {
			if (id == 0) g_variant_builder_add(&b, "(i@a{sv})", 0, p->Props(nullptr));
			else if (TrayItem* it = p->Find(p->items, id)) g_variant_builder_add(&b, "(i@a{sv})", id, p->Props(it));
		}
		g_variant_iter_free(ids);
		g_dbus_method_invocation_return_value(inv, g_variant_new("(a(ia{sv}))", &b));
	} else if (!g_strcmp0(method, "GetProperty")) {
		gint id;
		const char* name;
		g_variant_get(params, "(i&s)", &id, &name);
		GVariant* props = p->Props(id == 0 ? nullptr : p->Find(p->items, id));
		GVariant* v = g_variant_lookup_value(props, name, nullptr);
		g_variant_unref(g_variant_ref_sink(props));
		if (v) { g_dbus_method_invocation_return_value(inv, g_variant_new("(v)", v)); g_variant_unref(v); }
		else g_dbus_method_invocation_return_dbus_error(inv, "com.canonical.dbusmenu.UnknownProperty", name);
	} else if (!g_strcmp0(method, "Event")) {
		gint id;
		const char* ev;
		g_variant_get(params, "(i&svu)", &id, &ev, nullptr, nullptr);
		if (!g_strcmp0(ev, "clicked")) p->Click(id);
		g_dbus_method_invocation_return_value(inv, nullptr);
	} else if (!g_strcmp0(method, "EventGroup")) {
		GVariantIter* it;
		g_variant_get(params, "(a(isvu))", &it);
		gint id;
		const char* ev;
		GVariant* d;
		guint32 ts;
		while (g_variant_iter_next(it, "(i&svu)", &id, &ev, &d, &ts)) {
			if (!g_strcmp0(ev, "clicked")) p->Click(id);
			g_variant_unref(d);
		}
		g_variant_iter_free(it);
		g_dbus_method_invocation_return_value(inv, g_variant_new("(@ai)", g_variant_new_array(G_VARIANT_TYPE_INT32, nullptr, 0)));
	} else if (!g_strcmp0(method, "AboutToShow")) {
		g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", FALSE));
	} else if (!g_strcmp0(method, "AboutToShowGroup")) {
		g_dbus_method_invocation_return_value(inv, g_variant_new("(@ai@ai)", g_variant_new_array(G_VARIANT_TYPE_INT32, nullptr, 0),
			g_variant_new_array(G_VARIANT_TYPE_INT32, nullptr, 0)));
	} else {
		g_dbus_method_invocation_return_dbus_error(inv, "org.freedesktop.DBus.Error.UnknownMethod", method);
	}
}

static GVariant* OnGet(GDBusConnection*, const char*, const char*, const char* iface, const char* name, GError**, gpointer data) {
	auto* p = static_cast<Tray::Impl*>(data);
	auto s = [](const std::string& v) { return g_variant_new_string(v.c_str()); };
	auto empty = [] { return g_variant_new_array(G_VARIANT_TYPE("(iiay)"), nullptr, 0); };
	if (!g_strcmp0(iface, "com.canonical.dbusmenu")) {
		if (!g_strcmp0(name, "Version")) return g_variant_new_uint32(3);
		if (!g_strcmp0(name, "TextDirection")) return s("ltr");
		if (!g_strcmp0(name, "Status")) return s("normal");
		if (!g_strcmp0(name, "IconThemePath")) return g_variant_new_strv(nullptr, 0);
		return nullptr;
	}
	if (!g_strcmp0(name, "Category")) return s("ApplicationStatus");
	if (!g_strcmp0(name, "Id")) return s("latexanywhere");
	if (!g_strcmp0(name, "Title")) return s(p->title);
	if (!g_strcmp0(name, "Status")) return s(p->hidden ? "Passive" : "Active");
	if (!g_strcmp0(name, "WindowId")) return g_variant_new_int32(0);
	if (!g_strcmp0(name, "IconName")) return s(p->iconName);
	if (!g_strcmp0(name, "IconThemePath")) return s(p->iconDir);
	if (!g_strcmp0(name, "IconPixmap")) return p->pixmaps ? g_variant_ref(p->pixmaps) : empty();
	if (!g_strcmp0(name, "OverlayIconName") || !g_strcmp0(name, "AttentionIconName") || !g_strcmp0(name, "AttentionMovieName")) return s("");
	if (!g_strcmp0(name, "OverlayIconPixmap") || !g_strcmp0(name, "AttentionIconPixmap")) return empty();
	if (!g_strcmp0(name, "ToolTip")) return g_variant_new("(s@a(iiay)ss)", p->iconName.c_str(), empty(), p->title.c_str(), "");
	if (!g_strcmp0(name, "ItemIsMenu")) return g_variant_new_boolean(TRUE);
	if (!g_strcmp0(name, "Menu")) return g_variant_new_object_path("/MenuBar");
	return nullptr;
}

// 图标画成几种尺寸的位图（ARGB32、网络字节序），宿主找不到主题图标时用
static GVariant* Pixmaps(const std::string& svg) {
	GVariantBuilder b;
	g_variant_builder_init(&b, G_VARIANT_TYPE("a(iiay)"));
	RsvgHandle* h = rsvg_handle_new_from_data(reinterpret_cast<const guint8*>(svg.data()), svg.size(), nullptr);
	if (h) {
		for (int sz : {16, 22, 24, 32, 48, 64}) {
			cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sz, sz);
			cairo_t* cr = cairo_create(s);
			RsvgRectangle vp{0, 0, (double)sz, (double)sz};
			rsvg_handle_render_document(h, cr, &vp, nullptr);
			cairo_destroy(cr);
			cairo_surface_flush(s);
			const unsigned char* px = cairo_image_surface_get_data(s);
			int stride = cairo_image_surface_get_stride(s);
			std::string out;
			out.reserve(sz * sz * 4);
			for (int y = 0; y < sz; y++)
				for (int x = 0; x < sz; x++) {
					uint32_t v = *reinterpret_cast<const uint32_t*>(px + y * stride + x * 4);   // 预乘的本机字节序 ARGB
					unsigned a = v >> 24, r = v >> 16 & 255, g = v >> 8 & 255, bl = v & 255;
					if (a && a < 255) { r = r * 255 / a; g = g * 255 / a; bl = bl * 255 / a; }
					out += (char)a; out += (char)r; out += (char)g; out += (char)bl;
				}
			cairo_surface_destroy(s);
			g_variant_builder_add(&b, "(ii@ay)", sz, sz, g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, out.data(), out.size(), 1));
		}
		g_object_unref(h);
	}
	return g_variant_ref_sink(g_variant_builder_end(&b));
}

bool Tray::Create(const std::string& iconName, const std::string& iconDir, const std::string& svg) {
	p_ = new Impl;
	p_->iconName = iconName;
	p_->iconDir = iconDir;
	p_->pixmaps = Pixmaps(svg);
	GError* e = nullptr;
	p_->bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &e);
	if (!p_->bus) { Log("托盘：连不上会话总线：%s", e ? e->message : "?"); g_clear_error(&e); return false; }
	p_->node = g_dbus_node_info_new_for_xml(kXml, &e);
	static const GDBusInterfaceVTable vt = {OnCall, OnGet, nullptr, {}};
	if (!g_dbus_connection_register_object(p_->bus, "/StatusNotifierItem", p_->node->interfaces[0], &vt, p_, nullptr, &e) ||
		!g_dbus_connection_register_object(p_->bus, "/MenuBar", p_->node->interfaces[1], &vt, p_, nullptr, &e)) {
		Log("托盘：导出对象失败：%s", e ? e->message : "?");
		g_clear_error(&e);
		return false;
	}
	p_->svc = "org.kde.StatusNotifierItem-" + std::to_string(getpid()) + "-1";
	g_bus_own_name_on_connection(p_->bus, p_->svc.c_str(), G_BUS_NAME_OWNER_FLAGS_NONE, nullptr, nullptr, nullptr, nullptr);
	// 顶栏（GNOME Shell 扩展）出现或重启时都重新登记
	g_bus_watch_name_on_connection(p_->bus, "org.kde.StatusNotifierWatcher", G_BUS_NAME_WATCHER_FLAGS_NONE,
		[](GDBusConnection*, const char*, const char*, gpointer d) { static_cast<Impl*>(d)->Register(); }, nullptr, p_, nullptr);
	return true;
}

void Tray::SetMenu(std::vector<TrayItem> items) {
	if (!p_) return;
	int next = 1;
	p_->Number(items, next);
	p_->items = std::move(items);
	p_->rev++;
	g_dbus_connection_emit_signal(p_->bus, nullptr, "/MenuBar", "com.canonical.dbusmenu", "LayoutUpdated", g_variant_new("(ui)", p_->rev, 0), nullptr);
}

void Tray::SetTooltip(const std::string& title) {
	if (!p_) return;
	p_->title = title;
	g_dbus_connection_emit_signal(p_->bus, nullptr, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewTitle", nullptr, nullptr);
	g_dbus_connection_emit_signal(p_->bus, nullptr, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewToolTip", nullptr, nullptr);
}

void Tray::Hide() {
	if (!p_) return;
	p_->hidden = true;
	g_dbus_connection_emit_signal(p_->bus, nullptr, "/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewStatus", g_variant_new("(s)", "Passive"), nullptr);
	g_dbus_connection_flush_sync(p_->bus, nullptr, nullptr);
}

}  // namespace la
