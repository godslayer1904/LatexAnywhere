#!/usr/bin/env python3
# Chrome 上屏复现：Chrome 里放一个 textarea，内容变化写进页面标题；经 AT-SPI 读窗口标题拿到内容。
# 用法（在 run.sh 同样的无头环境里）：chrome_probe.py wayland|x11
import os, re, subprocess, sys, time
import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi, Gio, GLib

EXE = os.environ["LA_EXE"]
mode = sys.argv[1]
bus = Gio.bus_get_sync(Gio.BusType.SESSION)
RD = "org.gnome.Mutter.RemoteDesktop"
sess = bus.call_sync(RD, "/org/gnome/Mutter/RemoteDesktop", RD, "CreateSession", None, GLib.VariantType("(o)"), 0, 3000).unpack()[0]
bus.call_sync(RD, sess, RD + ".Session", "Start", None, None, 0, 3000)


def ks(sym, down): bus.call_sync(RD, sess, RD + ".Session", "NotifyKeyboardKeysym", GLib.Variant("(ub)", (sym, down)), None, 0, 3000)


NAMED = {"Tab": 0xff09, "Enter": 0xff0d, "Esc": 0xff1b}
MODS = {"A": 0xffe9, "S": 0xffe1, "C": 0xffe3}


def keys(s):
    for m in re.finditer(r"\{([^}]+)\}|(.)", s):
        if m.group(1):
            parts = m.group(1).split("-")
            for x in parts[:-1]: ks(MODS[x], True)
            k = parts[-1]
            sym = NAMED.get(k, ord(k) if len(k) == 1 else 0)
            ks(sym, True); ks(sym, False)
            for x in reversed(parts[:-1]): ks(MODS[x], False)
        else:
            ks(ord(m.group(2)), True); ks(ord(m.group(2)), False)
        time.sleep(0.02)


def titles():
    out = []
    d = Atspi.get_desktop(0)
    for i in range(d.get_child_count()):
        a = d.get_child_at_index(i)
        try:
            for j in range(a.get_child_count()):
                out.append(a.get_child_at_index(j).get_name())
        except Exception:
            pass
    return out


def wait(fn, t=8):
    t0 = time.time()
    while time.time() - t0 < t:
        v = fn()
        if v: return v
        time.sleep(0.1)
    return fn()


def value():
    import json, urllib.request
    try:
        for p in json.load(urllib.request.urlopen("http://127.0.0.1:9223/json", timeout=1)):
            if p.get("type") == "page" and p.get("title", "").startswith("V:"): return p["title"][2:]
    except Exception:
        return None
    return None


time.sleep(4)
keys("{Esc}")   # 退出概览
time.sleep(1)
html = "data:text/html,<title>V:</title><textarea autofocus oninput=\"document.title='V:'+this.value\"></textarea>"
prof = f"/tmp/la-e2e/chrome-{mode}"
args = ["google-chrome", f"--user-data-dir={prof}", "--no-first-run", "--no-default-browser-check", "--force-renderer-accessibility",
        "--disable-gpu", "--password-store=basic", "--remote-debugging-port=9223", f"--ozone-platform={mode}", "--new-window", html]
ch = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=open(f"/tmp/la-e2e/chrome-{mode}.log", "w"))
ok = wait(lambda: value() is not None, 20)
print("Chrome 窗口：", ok is not None, [t for t in titles() if t][:6], flush=True)
time.sleep(1.5)
keys("ab")
print("直接打字后：", repr(wait(lambda: value() if value() == "ab" else None, 3) or value()), flush=True)
subprocess.Popen([EXE, "--toggle"])
time.sleep(1.2)
print("框是否弹出：", "LatexAnywhere" in titles(), flush=True)
keys("x^2{Tab}")
time.sleep(0.3)
keys("{Enter}")
got = wait(lambda: value() if value() not in (None, "ab") else None, 4)
print(f"[{mode}] 上屏后 textarea：", repr(value()), flush=True)
after = value() or ""
ok = after.startswith("ab") and len(after) > 2
print(("✓" if ok else "✗") + f" Chrome（{mode}）上屏", f"得到 [{after}]", flush=True)
ch.terminate()
bus.call_sync(RD, sess, RD + ".Session", "Stop", None, None, 0, 3000)
sys.exit(0 if ok else 1)
