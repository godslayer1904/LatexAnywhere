#!/usr/bin/env python3
# 端到端测试的目标程序：一个普通的 GTK4 输入框，相当于用户正在打字的软件。
# 内容一变就写进 $LA_E2E/target.txt；驱动脚本往 $LA_E2E/cmd 追加命令来控制它：
#   set <json>   设置内容和选区：{"text": "...", "sel": [起, 止]}
#   toggle       像 GNOME 快捷键那样，带激活令牌启动「latexanywhere --toggle」
#   present      把自己切回前台
import json, os, subprocess, sys
import gi
gi.require_version("Gtk", "4.0")
gi.require_version("Gdk", "4.0")
from gi.repository import Gtk, Gdk, GLib

D = os.environ["LA_E2E"]
EXE = os.environ["LA_EXE"]
OUT, CMD = os.path.join(D, "target.txt"), os.path.join(D, "cmd")
pos = 0


def write(buf):
    s, e = buf.get_bounds()
    t = buf.get_text(s, e, True)
    with open(OUT + ".tmp", "w", encoding="utf-8") as f:
        f.write(t)
    os.replace(OUT + ".tmp", OUT)


def activate(app):
    win = Gtk.ApplicationWindow(application=app, title="LA 目标程序")
    win.set_default_size(700, 300)
    view = Gtk.TextView()
    buf = view.get_buffer()
    buf.connect("changed", write)
    win.set_child(view)
    kc = Gtk.EventControllerKey()
    kc.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
    kc.connect("key-pressed", lambda c, kv, code, st: print(f"key {Gdk.keyval_name(kv)} code={code} state={int(st)}", flush=True) or False)
    view.add_controller(kc)
    win.connect("notify::is-active", lambda w, p: print("active", w.is_active(), flush=True))
    win.present()
    view.grab_focus()
    write(buf)

    def poll():
        global pos
        try:
            with open(CMD, encoding="utf-8") as f:
                f.seek(pos)
                lines = f.read()
                pos = f.tell()
        except FileNotFoundError:
            return True
        for line in lines.splitlines():
            op, _, arg = line.partition(" ")
            if op == "set":
                a = json.loads(arg)
                buf.set_text(a["text"])
                s, e = a.get("sel", [len(a["text"])] * 2)
                buf.select_range(buf.get_iter_at_offset(e), buf.get_iter_at_offset(s))
                view.grab_focus()
            elif op == "toggle":
                ctx = win.get_display().get_app_launch_context()
                token = ctx.get_startup_notify_id(None, []) or ""
                env = dict(os.environ, XDG_ACTIVATION_TOKEN=token)
                subprocess.Popen([EXE, "--toggle"], env=env)
            elif op == "setclip":
                win.get_clipboard().set(arg)
            elif op == "getclip":
                def done(cb, res):
                    try: t = cb.read_text_finish(res) or ""
                    except Exception as e: t = "(读取失败 %s)" % e
                    with open(os.path.join(D, "clip.txt"), "w", encoding="utf-8") as f: f.write(t)
                win.get_clipboard().read_text_async(None, done)
            elif op == "present":
                win.present()
                view.grab_focus()
        return True

    GLib.timeout_add(20, poll)


app = Gtk.Application(application_id="io.github.latexanywhere.E2ETarget")
app.connect("activate", activate)
sys.exit(app.run([]))
