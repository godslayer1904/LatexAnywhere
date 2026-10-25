#!/usr/bin/env python3
# 测试用的托盘宿主：扮演 GNOME 顶栏的 StatusNotifierWatcher，记录注册上来的托盘图标（写进 $LA_E2E/sni.txt）。
# 无头 GNOME Shell 没开 AppIndicator 扩展，没有它程序的托盘就无处可挂。
import os
from gi.repository import Gio, GLib

D = os.environ["LA_E2E"]
XML = """<node><interface name="org.kde.StatusNotifierWatcher">
<method name="RegisterStatusNotifierItem"><arg type="s" direction="in"/></method>
<method name="RegisterStatusNotifierHost"><arg type="s" direction="in"/></method>
<property name="RegisteredStatusNotifierItems" type="as" access="read"/>
<property name="IsStatusNotifierHostRegistered" type="b" access="read"/>
<property name="ProtocolVersion" type="i" access="read"/>
<signal name="StatusNotifierItemRegistered"><arg type="s"/></signal>
<signal name="StatusNotifierHostRegistered"/>
</interface></node>"""
items = []


def call(conn, sender, path, iface, method, params, inv):
    if method == "RegisterStatusNotifierItem":
        svc = params.unpack()[0]   # 可能是对象路径，也可能是总线名
        full = sender + "|" + svc if svc.startswith("/") else svc + "|/StatusNotifierItem"
        items.append(full)
        with open(os.path.join(D, "sni.txt"), "a") as f:
            f.write(full + "\n")
        conn.emit_signal(None, "/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", "StatusNotifierItemRegistered", GLib.Variant("(s)", (full,)))
    inv.return_value(None)


def prop(conn, sender, path, iface, name):
    return {"RegisteredStatusNotifierItems": GLib.Variant("as", items), "IsStatusNotifierHostRegistered": GLib.Variant("b", True),
            "ProtocolVersion": GLib.Variant("i", 0)}[name]


def acquired(conn, name):
    node = Gio.DBusNodeInfo.new_for_xml(XML)
    conn.register_object("/StatusNotifierWatcher", node.interfaces[0], call, prop, None)


Gio.bus_own_name(Gio.BusType.SESSION, "org.kde.StatusNotifierWatcher", 0, acquired, None, None)
GLib.MainLoop().run()
