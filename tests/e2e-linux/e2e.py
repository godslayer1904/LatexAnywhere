#!/usr/bin/env python3
# Linux 端到端测试驱动：在无头 GNOME Shell 里模拟真人按键（Mutter 远程桌面接口），
# 检查公式最终插进目标程序（target.py）的内容。用例与 Windows 版 tests/e2e/e2e.ahk 对应。
import json, os, re, signal, subprocess, sys, time
import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi, Gio, GLib

D = os.environ["LA_E2E"]
EXE = os.environ["LA_EXE"]
OUT = os.path.join(D, "out.txt")
LOG = os.path.join(os.environ["XDG_STATE_HOME"], "LatexAnywhere", "log.txt")
CFG = os.path.join(os.environ["XDG_CONFIG_HOME"], "LatexAnywhere")
passed = failed = 0


def R(ok, name, detail=""):
    global passed, failed
    if ok: passed += 1
    else: failed += 1
    line = ("✓ " if ok else "✗ ") + name + ("  " + detail if detail else "")
    print(line, flush=True)
    with open(OUT, "a", encoding="utf-8") as f:
        f.write(line + "\n")


def wait(fn, timeout=5.0, step=0.02):
    t = time.time()
    while time.time() - t < timeout:
        v = fn()
        if v: return v
        time.sleep(step)
    return fn()


# ---------- 模拟按键 ----------
bus = Gio.bus_get_sync(Gio.BusType.SESSION)
RD = "org.gnome.Mutter.RemoteDesktop"
sess = bus.call_sync(RD, "/org/gnome/Mutter/RemoteDesktop", RD, "CreateSession", None, GLib.VariantType("(o)"), 0, 3000).unpack()[0]
bus.call_sync(RD, sess, RD + ".Session", "Start", None, None, 0, 3000)


def ks(sym, down):
    bus.call_sync(RD, sess, RD + ".Session", "NotifyKeyboardKeysym", GLib.Variant("(ub)", (sym, down)), None, 0, 3000)


NAMED = {"Tab": 0xff09, "Enter": 0xff0d, "Esc": 0xff1b, "BS": 0xff08, "Left": 0xff51, "Right": 0xff53, "Home": 0xff50, "End": 0xff57}
MODS = {"S": 0xffe1, "C": 0xffe3, "A": 0xffe9}


def keysym(c):
    o = ord(c)
    return o if 0x20 <= o <= 0x7e else 0x01000000 + o


def type_keys(s, gap=0.012):
    """{Tab} {S-Enter} {C-Enter} {A-Enter} {C-S-Enter} 或普通字符"""
    for m in re.finditer(r"\{([^}]+)\}|(.)", s, re.S):
        if m.group(1):
            parts = m.group(1).split("-")
            mods, key = parts[:-1], parts[-1]
            for x in mods: ks(MODS[x], True)
            sym = NAMED.get(key, keysym(key) if len(key) == 1 else None)
            ks(sym, True); ks(sym, False)
            for x in reversed(mods): ks(MODS[x], False)
        else:
            sym = keysym(m.group(2))
            ks(sym, True); ks(sym, False)
        time.sleep(gap)


# ---------- 截图（无头 GNOME 以 --unsafe-mode 启动才允许） ----------
def shot(name):
    try:
        bus.call_sync("org.gnome.Shell.Screenshot", "/org/gnome/Shell/Screenshot", "org.gnome.Shell.Screenshot", "Screenshot",
                      GLib.Variant("(bbs)", (False, False, os.path.join(D, f"shot_{name}.png"))), None, 0, 5000)
    except GLib.Error as e:
        print("截图失败", name, e.message, flush=True)


# ---------- 目标程序与公式框状态 ----------
def cmd(line):
    with open(os.path.join(D, "cmd"), "a", encoding="utf-8") as f:
        f.write(line + "\n")


def target():
    try:
        return open(os.path.join(D, "target.txt"), encoding="utf-8").read()
    except FileNotFoundError:
        return None


def set_target(text, sel=None):
    cmd("set " + json.dumps({"text": text, "sel": sel or [len(text), len(text)]}, ensure_ascii=False))
    wait(lambda: target() == text, 3)
    time.sleep(0.1)


def app_windows():
    """latexanywhere 的可见窗口标题列表（AT-SPI）"""
    out = []
    desk = Atspi.get_desktop(0)
    for i in range(desk.get_child_count()):
        a = desk.get_child_at_index(i)
        try:
            if not a or a.get_name() != "latexanywhere": continue
            for j in range(a.get_child_count()):
                w = a.get_child_at_index(j)
                st = w.get_state_set()
                if st.contains(Atspi.StateType.SHOWING) or st.contains(Atspi.StateType.VISIBLE):
                    out.append(w.get_name())
        except GLib.Error:
            pass
    return out


def box_shown(): return "LatexAnywhere" in app_windows()


def log_lines():
    try:
        return open(LOG, encoding="utf-8").read().splitlines()
    except FileNotFoundError:
        return []


def pids(worker=None):
    r = subprocess.run(["pgrep", "-f", "-x", re.escape(EXE) + r"( --foreground| --worker.*)"], capture_output=True, text=True)
    ps = [int(p) for p in r.stdout.split()]
    if worker is None: return ps
    res = []
    for p in ps:
        try:
            c = open(f"/proc/{p}/cmdline").read()
            if ("--worker" in c) == worker: res.append(p)
        except FileNotFoundError:
            pass
    return res


def anon_mb(pid):
    try:
        s = open(f"/proc/{pid}/status").read()
        return int(re.search(r"RssAnon:\s+(\d+)", s).group(1)) / 1024
    except Exception:
        return 0


def start_app():
    subprocess.Popen([EXE, "--foreground"], stdout=subprocess.DEVNULL, stderr=open(os.path.join(D, "app.stderr.txt"), "a"))
    ok = wait(lambda: pids(True) and any("就绪" in l for l in log_lines()[-30:]), 15)
    time.sleep(0.8)
    return ok


def quit_app():
    subprocess.run([EXE, "--quit"], timeout=10)
    wait(lambda: not pids(), 8)


def restart_app():
    quit_app()
    time.sleep(0.5)
    start_app()


def toggle():
    cmd("toggle")


def test_case(name, keys, commit, expected, sel_text=None, shot_name=None):
    set_target(sel_text or "", [0, len(sel_text)] if sel_text else None)
    t0 = time.time()
    toggle()
    if not wait(box_shown, 4):
        R(False, name, "公式框没有弹出")
        return None
    lat = (time.time() - t0) * 1000
    time.sleep(0.15)
    type_keys(keys)
    time.sleep(0.1)
    if shot_name:
        time.sleep(0.4)
        shot(shot_name)
    type_keys(commit)
    hidden = wait(lambda: not box_shown(), 4)
    got = wait(lambda: (target() if target() != (sel_text or "") or expected == (sel_text or "") else None), 3) or target()
    time.sleep(0.2)
    got = target()
    R(hidden and (expected == "__ANY__" or got == expected), name, f"期望 [{expected}] 得到 [{got}]" + ("" if hidden else "（框没收起）"))
    return lat, got


# ================= 用例 =================
open(OUT, "w").close()
time.sleep(1)
type_keys("{Esc}")   # GNOME Shell 启动后停在「概览」，先退出概览，窗口才能拿到键盘
time.sleep(0.8)
cmd("present")
wait(lambda: target() is not None, 5)

# ---- 首次启动欢迎窗口 ----
start_app()
HELP = "LatexAnywhere 使用说明"
R(wait(lambda: HELP in app_windows(), 8), "首次启动弹出欢迎窗口", str(app_windows()))
time.sleep(0.8)
shot("welcome")
type_keys("{Enter}")   # 不勾选直接关
wait(lambda: HELP not in app_windows(), 4)
restart_app()
w2 = wait(lambda: HELP in app_windows(), 8)
R(w2, "没勾选「不再显示」：再次启动仍弹出")


def find_role(acc, role, depth=0):
    if not acc or depth > 30: return None
    try:
        if acc.get_role() == role: return acc
        for i in range(acc.get_child_count()):
            r = find_role(acc.get_child_at_index(i), role, depth + 1)
            if r: return r
    except GLib.Error:
        pass
    return None


def help_window():
    desk = Atspi.get_desktop(0)
    for i in range(desk.get_child_count()):
        a = desk.get_child_at_index(i)
        if a and a.get_name() == "latexanywhere":
            for j in range(a.get_child_count()):
                w = a.get_child_at_index(j)
                if w.get_name() == HELP: return w
    return None


if w2:
    w = help_window()
    time.sleep(0.3)
    c = find_role(help_window(), Atspi.Role.CHECK_BOX)
    if c: print("复选框状态：", [s.value_nick for s in c.get_state_set().get_states()], flush=True)
    type_keys(" ")       # 初始焦点在复选框上：空格勾选
    time.sleep(0.3)
    type_keys("{Enter}")   # 开始使用（关窗口时按当前勾选状态保存）
    wait(lambda: HELP not in app_windows(), 4)
    R(os.path.exists(os.path.join(CFG, "hide-welcome")), "勾选状态已保存")
restart_app()
w3 = wait(lambda: HELP in app_windows(), 4)
R(not w3, "勾选后再次启动不再弹出")
cmd("present")
time.sleep(0.3)

# ---- 托盘菜单（经 dbusmenu 真实点击） ----
def sni():
    try:
        lines = open(os.path.join(D, "sni.txt")).read().split()
    except FileNotFoundError:
        return None
    return lines[-1].split("|") if lines else None


def menu_items():
    """[(id, label, 子菜单项列表)]，跳过分隔线"""
    s = sni()
    if not s: return []
    dest, path = s
    mp = bus.call_sync(dest, path, "org.freedesktop.DBus.Properties", "Get", GLib.Variant("(ss)", ("org.kde.StatusNotifierItem", "Menu")),
                       None, 0, 3000).unpack()[0]
    def walk(node):
        out = []
        for ch in node[2]:
            cid, props, kids = ch
            if props.get("type") == "separator" or props.get("visible") is False: continue
            if kids or props.get("children-display") == "submenu":
                try: bus.call_sync(dest, mp, "com.canonical.dbusmenu", "AboutToShow", GLib.Variant("(i)", (cid,)), None, 0, 3000)
                except GLib.Error: pass
            out.append((cid, props.get("label", "").replace("_", ""), walk(ch), props.get("toggle-state")))
        return out
    lay = bus.call_sync(dest, mp, "com.canonical.dbusmenu", "GetLayout", GLib.Variant("(iias)", (0, -1, [])), None, 0, 3000).unpack()[1]
    menu_items.dest, menu_items.path = dest, mp
    return walk(lay)


def click(item_id):
    bus.call_sync(menu_items.dest, menu_items.path, "com.canonical.dbusmenu", "Event",
                  GLib.Variant("(isvu)", (item_id, "clicked", GLib.Variant("i", 0), 0)), None, 0, 3000)
    time.sleep(0.6)


def help_title():
    """从托盘打开说明窗口，读标题后关掉"""
    items = menu_items()
    if not items: return "(没有托盘)"
    before = set(app_windows())
    click(items[0][0])
    t = wait(lambda: [w for w in app_windows() if w not in before and w != "LatexAnywhere"], 4)
    title = t[0] if t else "(没打开)"
    time.sleep(0.3)
    type_keys("{Esc}")
    wait(lambda: title not in app_windows(), 3)
    cmd("present")
    time.sleep(0.3)
    return title


def lang_file():
    try:
        return open(os.path.join(CFG, "language")).read().strip()
    except FileNotFoundError:
        return "(无)"


items = wait(menu_items, 6) or []
labels = [i[1] for i in items]
R(len(items) == 4 and len(items[2][2]) == 11, "托盘图标已注册，菜单齐全", f"{labels}，语言子菜单 {len(items[2][2]) if len(items) > 2 else 0} 项")
if len(items) == 4:
    sys_title = help_title()
    langs = menu_items()[2][2]
    ja = next((x for x in langs if x[1] == "日本語"), None)
    if ja: click(ja[0])
    ja_title = help_title()
    R(lang_file() == "ja" and ja_title == "LatexAnywhere の使い方", "托盘选「日本語」立即生效并保存", f"设置 {lang_file()}，标题 {ja_title}")
    R("言語" in [i[1] for i in menu_items()], "菜单文字跟着换成日文", str([i[1] for i in menu_items()]))
    restart_app()
    R(help_title() == "LatexAnywhere の使い方", "重启后仍是日文")
    click(menu_items()[2][2][0][0])   # 跟随系统
    t = help_title()
    R(lang_file() == "" and t == sys_title, "选「跟随系统」恢复系统语言", f"设置 [{lang_file()}]，标题 {t}")
    auto = os.path.join(os.environ["XDG_CONFIG_HOME"], "autostart", "latexanywhere.desktop")
    a0 = os.path.exists(auto)
    click(menu_items()[1][0])
    a1 = os.path.exists(auto)
    st = menu_items()[1][3]
    click(menu_items()[1][0])
    a2 = os.path.exists(auto)
    R(a1 != a0 and a2 == a0 and st == (1 if a1 else 0), "托盘开关「登录时自动启动」", f"{a0} → {a1}（勾选 {st}）→ {a2}")
cmd("present")
time.sleep(0.3)

# ---- 公式输入与上屏 ----
lats = []
for args in [
    ("x^2 Tab 后上屏（LaTeX）", "x^2{Tab}", "{Enter}", "x^{2}"),
    ("分式 Tab 跳位", "//a{Tab}b{Tab}", "{Enter}", "\\frac{a}{b}", None, "frac"),
    ("希腊字母", "@a+@b", "{Enter}", "\\alpha+\\beta"),
    ("矩阵 Tab、Enter、Shift+Enter 退出", "pmat1{Tab}2{Enter}3{Tab}4{S-Enter}", "{Enter}", "\\begin{pmatrix}1 & 2 \\\\  3 & 4\\end{pmatrix}", None, "pmatrix"),
    ("求和占位", "sum{Tab}k{Tab}0{Tab}n{Tab}", "{Enter}", "\\sum_{k=0}^{n}"),
    ("Ctrl+Enter 转 Unicode", "x^2{Tab}+@a", "{C-Enter}", "x²+α"),
    ("Alt+Enter 保留 $", "x^2{Tab}", "{A-Enter}", "$x^{2}$"),
    ("Esc 取消不插入", "x^2", "{Esc}", ""),
]:
    r = test_case(*args)
    if r: lats.append(r[0])
r = test_case("Ctrl+Shift+Enter 出 MathML", "x^2{Tab}", "{C-S-Enter}", "__ANY__")
R(bool(r) and r[1].startswith("<math") and "<msup>" in r[1], "MathML 内容正确", (r[1] if r else "")[:60])

# ---- 选区带入并替换（AT-SPI 读选区） ----
set_target("a+b", [0, 3])   # 同 Windows 版：全选 a+b 再唤起，框里改成 a+b+c
toggle()
if wait(box_shown, 4):
    time.sleep(0.2)
    type_keys("{End}{Left}+c")
    type_keys("{Enter}")
    wait(lambda: not box_shown(), 4)
    time.sleep(0.3)
R(target() == "a+b+c", "选中带入并替换", f"得到 [{target()}]")
R(any("AT-SPI" in l for l in log_lines()[-20:]), "选区经 AT-SPI 读取", next((l for l in reversed(log_lines()) if "弹框" in l), ""))

# ---- 输入法（IBus 智能拼音，GNOME 默认） ----
def ibus_engine(name=None):
    r = subprocess.run(["ibus", "engine"] + ([name] if name else []), capture_output=True, text=True, timeout=5)
    return r.stdout.strip()


ibus_engine("libpinyin")
time.sleep(1.5)
set_target("")
cmd("present")
time.sleep(0.3)
type_keys("ni ")
cn = wait(lambda: target() == "你", 3)
R(bool(cn), "测试环境：智能拼音可用（ni 空格 → 你）", f"引擎 {ibus_engine()}，得到 [{target()}]")
if cn:
    test_case("中文输入法开着时进框自动切英文", "x^2{Tab}", "{Enter}", "x^{2}")
    time.sleep(0.6)
    R(ibus_engine() == "libpinyin", "离开公式框后切回智能拼音", ibus_engine())
    set_target("")
    type_keys("ni ")
    R(bool(wait(lambda: target() == "你", 3)), "回到原输入框仍能打中文", f"[{target()}]")
    test_case("Esc 取消后同样切回", "x", "{Esc}", "")
    time.sleep(0.6)
    R(ibus_engine() == "libpinyin", "Esc 后切回智能拼音", ibus_engine())
ibus_engine("xkb:us::eng")
time.sleep(0.5)

# ---- 上屏后剪贴板原样 ----
def get_clip():
    p = os.path.join(D, "clip.txt")
    if os.path.exists(p): os.remove(p)
    cmd("getclip")
    return wait(lambda: os.path.exists(p) and open(p, encoding="utf-8").read(), 3) or ""


set_target("")
cmd("setclip 原来的剪贴板内容 ABC")
time.sleep(0.3)
test_case("剪贴板保护用例", "//1{Tab}2{Tab}", "{Enter}", "\\frac{1}{2}")
time.sleep(0.8)
cb = get_clip()
R(cb == "原来的剪贴板内容 ABC", "上屏后剪贴板原样", f"现在是 [{cb}]")

# ---- 连按 Alt+L ----
set_target("")
for _ in range(6):
    toggle()
    time.sleep(0.12)
time.sleep(0.6)
if box_shown():
    type_keys("{Esc}")
    wait(lambda: not box_shown(), 3)
R(len(pids(True)) == 1 and len(pids(False)) == 1, "连按 6 次后进程正常（守护 + 工作）", f"工作 {pids(True)} 守护 {pids(False)}")
test_case("连按之后照常可用", "x^2{Tab}", "{Enter}", "x^{2}")

# ---- 延迟与内存 ----
if lats: R(True, "弹框延迟（含取选区，含测试轮询）", f"平均 {sum(lats) / len(lats):.0f} ms，最大 {max(lats):.0f} ms，共 {len(lats)} 次")
g, w = pids(False), pids(True)
R(bool(g and w), "内存", f"守护进程 {anon_mb(g[0]) if g else 0:.1f} MB；工作进程 {anon_mb(w[0]) if w else 0:.1f} MB（匿名常驻）")

# ---- 崩溃自动重启 ----
old = pids(True)
t0 = time.time()
if old: os.kill(old[0], signal.SIGKILL)
nw = wait(lambda: [p for p in pids(True) if p not in old] and any("就绪" in l for l in log_lines()[-5:]), 8)
R(bool(nw), "工作进程被强杀后自动重启", f"用时 {(time.time() - t0) * 1000:.0f} ms（旧 {old} → 新 {pids(True)}）")
time.sleep(0.8)
test_case("重启后照常可用", "x^2{Tab}", "{Enter}", "x^{2}")

# ---- 浸泡 ----
N = int(os.environ.get("LA_SOAK", "300"))
w = pids(True)
m0 = anon_mb(w[0]) if w else 0
fails, samples = 0, []
for i in range(1, N + 1):
    set_target("")
    toggle()
    if not wait(box_shown, 4): fails += 1; continue
    time.sleep(0.05)
    type_keys("x^2{Tab}{Enter}", gap=0.004)
    wait(lambda: not box_shown(), 3)
    if not wait(lambda: target() == "x^{2}", 2): fails += 1
    if i % 50 == 0: samples.append(f"{i}:{anon_mb(w[0]):.1f}MB")
m1 = anon_mb(w[0]) if w else 0
R(fails == 0, f"浸泡 {N} 次全部正确", f"失败 {fails} 次")
R(m1 - m0 < 10, f"浸泡 {N} 次内存不持续增长", f"开始 {m0:.1f} MB → 结束 {m1:.1f} MB；采样 {' '.join(samples)}")

# ---- Alt+Shift+L 打开使用说明 ----
cmd("help")
R(bool(wait(lambda: HELP in app_windows(), 5)), "Alt+Shift+L 打开使用说明", str(app_windows()))
time.sleep(0.5)
type_keys("{Esc}")
wait(lambda: HELP not in app_windows(), 3)
cmd("present")
time.sleep(0.3)

# ---- 快捷键注册（用文件版 GSettings，看程序写进去的绑定） ----
quit_app()
kf = os.path.join(D, "gsettings-keyfile")
env = dict(os.environ, GSETTINGS_BACKEND="keyfile")
env.pop("LA_NO_SHORTCUTS", None)
os.makedirs(os.path.join(os.environ["XDG_CONFIG_HOME"], "glib-2.0", "settings"), exist_ok=True)
subprocess.Popen([EXE, "--foreground"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
wait(lambda: pids(True) and any("Alt+L 已注册" in l for l in log_lines()[-30:]), 15)
time.sleep(1)
kft = ""
try:
    kft = open(os.path.join(os.environ["XDG_CONFIG_HOME"], "glib-2.0", "settings", "keyfile"), encoding="utf-8").read()
except FileNotFoundError:
    pass
binds = {k: ("<Alt>l" if k == "toggle" else "<Shift><Alt>l" if k == "help" else "<Super><Alt>l") for k in ("toggle", "help", "quit")}
ok = all(f"latexanywhere-{k}" in kft for k in binds) and all(b in kft for b in binds.values()) and "--help" in kft
R(ok, "注册了 Alt+L / Alt+Shift+L / Super+Alt+L 三个快捷键", " ".join(re.findall(r"binding='([^']*)'", kft)))
subprocess.run([EXE, "--quit"], env=env, timeout=10)
wait(lambda: not pids(), 6)
start_app()

# ---- 托盘「退出」、紧急退出 ----
it = menu_items()
if it: click(it[-1][0])
R(wait(lambda: not pids(), 5), "托盘「退出」（守护进程一并退出）", f"剩余进程 {pids()}")
start_app()
subprocess.run([EXE, "--quit"], timeout=10)
R(wait(lambda: not pids(), 5), "紧急退出 Super+Alt+L（守护进程一并退出）", f"剩余进程 {pids()}")

summary = f"通过 {passed}，失败 {failed}"
print(summary)
with open(OUT, "a", encoding="utf-8") as f:
    f.write(summary + "\n")
bus.call_sync(RD, sess, RD + ".Session", "Stop", None, None, 0, 3000)
sys.exit(0 if failed == 0 else 1)
