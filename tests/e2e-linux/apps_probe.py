#!/usr/bin/env python3
# 真实软件验收：在无头 GNOME 里逐个打开用户真正用的软件，走「Alt+L → 打公式 → 上屏」，检查插进去的内容。
# 读结果：编辑器 Ctrl+S 存盘读文件；终端里跑 cat > 文件；浏览器把 textarea 内容写进页面标题（Chrome 经调试接口、Firefox 经无障碍接口读窗口名）。
import json, os, re, shutil, subprocess, sys, time, urllib.request
import gi
gi.require_version("Atspi", "2.0")
from gi.repository import Atspi, Gio, GLib

D = os.environ["LA_E2E"]
EXE = os.environ["LA_EXE"]
OUT = os.path.join(D, "apps.txt")
LOG = os.path.join(os.environ["XDG_STATE_HOME"], "LatexAnywhere", "log.txt")
bus = Gio.bus_get_sync(Gio.BusType.SESSION)
RD = "org.gnome.Mutter.RemoteDesktop"
sess = bus.call_sync(RD, "/org/gnome/Mutter/RemoteDesktop", RD, "CreateSession", None, GLib.VariantType("(o)"), 0, 3000).unpack()[0]
bus.call_sync(RD, sess, RD + ".Session", "Start", None, None, 0, 3000)
passed = failed = 0


def R(ok, name, detail=""):
    global passed, failed
    if ok: passed += 1
    else: failed += 1
    line = ("✓ " if ok else "✗ ") + name + ("  " + detail if detail else "")
    print(line, flush=True)
    with open(OUT, "a", encoding="utf-8") as f: f.write(line + "\n")


def ks(sym, down): bus.call_sync(RD, sess, RD + ".Session", "NotifyKeyboardKeysym", GLib.Variant("(ub)", (sym, down)), None, 0, 3000)


NAMED = {"Tab": 0xff09, "Enter": 0xff0d, "Esc": 0xff1b, "BS": 0xff08, "End": 0xff57, "Home": 0xff50, "Left": 0xff51, "Right": 0xff53}
MODS = {"S": 0xffe1, "C": 0xffe3, "A": 0xffe9}


def keys(s, gap=0.03):
    for m in re.finditer(r"\{([^}]+)\}|(.)", s, re.S):
        if m.group(1):
            parts = m.group(1).split("-")
            for x in parts[:-1]: ks(MODS[x], True)
            k = parts[-1]
            sym = NAMED.get(k, ord(k) if len(k) == 1 else 0)
            ks(sym, True); ks(sym, False)
            for x in reversed(parts[:-1]): ks(MODS[x], False)
        else:
            o = ord(m.group(2)); sym = o if o < 0x7f else 0x01000000 + o
            ks(sym, True); ks(sym, False)
        time.sleep(gap)


def wait(fn, t=6, step=0.1):
    t0 = time.time()
    while time.time() - t0 < t:
        v = fn()
        if v: return v
        time.sleep(step)
    return fn()


def windows():
    out = []
    d = Atspi.get_desktop(0)
    for i in range(d.get_child_count()):
        a = d.get_child_at_index(i)
        try:
            for j in range(a.get_child_count()): out.append((a.get_name(), a.get_child_at_index(j).get_name()))
        except Exception: pass
    return out


def shell_titles():
    try:
        r = bus.call_sync("org.gnome.Shell.Introspect", "/org/gnome/Shell/Introspect", "org.gnome.Shell.Introspect", "GetWindows",
                          None, None, 0, 3000).unpack()[0]
        return [v.get("title", "") for v in r.values()]
    except GLib.Error:
        return []


def box_shown(): return any(app == "latexanywhere" and w == "LatexAnywhere" for app, w in windows())


def log_tail(n=5):
    try: return open(LOG, encoding="utf-8").read().splitlines()[-n:]
    except FileNotFoundError: return []


def formula(commit, body="x^2{Tab}"):
    """Alt+L（经 GNOME 快捷键的等价调用）→ 打公式 → 上屏；返回框是否正常弹出收起"""
    subprocess.Popen([EXE, "--toggle"])
    if not wait(box_shown, 4): return False
    time.sleep(0.25)
    keys(body)
    time.sleep(0.15)
    keys(commit)
    hidden = bool(wait(lambda: not box_shown(), 4))
    time.sleep(0.7)   # 程序在框收起 150ms 后才粘贴：等粘贴落地再读，否则存盘、回车会抢在粘贴前面
    return hidden


# ---------- 各软件的打开方式和读取方式 ----------
class App:
    def __init__(self, name): self.name, self.proc = name, None
    def stop(self):
        if self.proc:
            self.proc.terminate()
            try: self.proc.wait(5)
            except Exception: self.proc.kill()


class Editor(App):   # GNOME 文本编辑器 / gedit：存盘读文件
    def __init__(self, name, cmd):
        super().__init__(name); self.cmd = cmd; self.file = os.path.join(D, f"{cmd}.txt")
    def start(self):
        open(self.file, "w").close()
        self.proc = subprocess.Popen([self.cmd, self.file], stdout=subprocess.DEVNULL, stderr=open(os.path.join(D, f"{self.cmd}.log"), "w"))
        time.sleep(4)
    def reset(self, text): keys("{C-a}{BS}" + text)
    def read(self):
        keys("{C-s}"); time.sleep(0.8)
        return open(self.file, encoding="utf-8").read().rstrip("\n")
    can_select = True


class Terminal(App):   # 终端：cat 接收一行，回车后写进文件
    def __init__(self, name, cmd, args):
        super().__init__(name); self.cmd = cmd; self.args = args; self.file = os.path.join(D, f"{cmd}.txt")
    def start(self):
        if os.path.exists(self.file): os.remove(self.file)
        sh = f"stty -echoctl; exec cat > {self.file}"
        typed = self.cmd == "ptyxis"   # Ptyxis 在测试会话里跑不了 -x 命令：开好后在它的 shell 里输入
        cmdline = self.args + ([] if typed else ["--", "bash", "-c", sh])
        env = dict(os.environ)
        if self.cmd == "ptyxis":
            env["WAYLAND_DISPLAY"] = os.path.join(os.environ["XDG_RUNTIME_DIR"], os.environ["WAYLAND_DISPLAY"])
            env["XDG_RUNTIME_DIR"] = f"/run/user/{os.getuid()}"
        self.proc = subprocess.Popen([self.cmd] + cmdline, env=env,
                                     stdout=subprocess.DEVNULL, stderr=open(os.path.join(D, f"{self.cmd}.log"), "w"))
        time.sleep(4)
        if typed:
            keys(sh + "{Enter}"); time.sleep(1)
        self.lines = 0
    def reset(self, text): keys(text)
    def read(self):
        keys("{Enter}"); time.sleep(0.6)
        try: ls = open(self.file, encoding="utf-8").read().split("\n")
        except FileNotFoundError: return None
        line = ls[self.lines] if len(ls) > self.lines else None
        self.lines += 1
        return line
    can_select = False


# 本地页面（Firefox 不许命令行打开顶层 data: 网址）：textarea 内容写进页面标题
PAGE_FILE = os.path.join(D, "page.html")
open(PAGE_FILE, "w", encoding="utf-8").write("<meta charset=utf-8><title>V:</title><textarea autofocus oninput=\"document.title='V:'+this.value\"></textarea>")
PAGE = "file://" + PAGE_FILE


class Chrome(App):
    def start(self):
        prof = os.path.join(D, "chrome-prof"); shutil.rmtree(prof, ignore_errors=True)
        self.proc = subprocess.Popen(["google-chrome", f"--user-data-dir={prof}", "--no-first-run", "--no-default-browser-check", "--password-store=basic",
                                      "--disable-gpu", "--remote-debugging-port=9223", "--ozone-platform=wayland", "--new-window", PAGE],
                                     stdout=subprocess.DEVNULL, stderr=open(os.path.join(D, "chrome.log"), "w"))
        wait(lambda: self.title() is not None, 20); time.sleep(1.5)
    def title(self):
        try:
            for p in json.load(urllib.request.urlopen("http://127.0.0.1:9223/json", timeout=1)):
                if p.get("type") == "page" and p.get("title", "").startswith("V:"): return p["title"][2:]
        except Exception: return None
    def reset(self, text): keys("{C-a}{BS}" + text); time.sleep(0.3)
    def read(self): time.sleep(0.5); return self.title()
    can_select = True


class Firefox(App):
    def start(self):
        prof = os.path.join(D, "ff-prof"); shutil.rmtree(prof, ignore_errors=True); os.makedirs(prof)
        with open(os.path.join(prof, "user.js"), "w") as f:   # 关掉首次启动的欢迎页、默认浏览器提示等，免得抢焦点
            for k, v in {"browser.aboutwelcome.enabled": "false", "browser.shell.checkDefaultBrowser": "false",
                         "browser.startup.homepage_override.mstone": '"ignore"', "startup.homepage_welcome_url": '""',
                         "datareporting.policy.dataSubmissionPolicyBypassNotification": "true", "toolkit.telemetry.reportingpolicy.firstRun": "false",
                         "browser.tabs.warnOnClose": "false", "browser.sessionstore.resume_from_crash": "false",
                         "trailhead.firstrun.didSeeAboutWelcome": "true", "browser.uitour.enabled": "false"}.items():
                f.write(f'user_pref("{k}", {v});\n')
        env = dict(os.environ, MOZ_ENABLE_WAYLAND="1", MOZ_DISABLE_CONTENT_SANDBOX="1", MOZ_DISABLE_RDD_SANDBOX="1", MOZ_DISABLE_SOCKET_PROCESS_SANDBOX="1", MOZ_DISABLE_GPU_SANDBOX="1", MOZ_DISABLE_UTILITY_SANDBOX="1")   # Ubuntu 禁止非特权用户命名空间，原版 Firefox 的沙箱起不来（仅测试环境）
        # snap 版 Firefox 不肯在测试会话里启动（不属于 snap 的 cgroup）：用 Mozilla 官方原版，内核相同
        ff = os.path.expanduser("~/.cache/la-test-firefox/firefox/firefox")
        self.proc = subprocess.Popen([ff if os.path.exists(ff) else "firefox", "--no-remote", "--profile", prof, "--new-window", PAGE], env=env,
                                     stdout=subprocess.DEVNULL, stderr=open(os.path.join(D, "firefox.log"), "w"))
        wait(lambda: self.title() is not None, 30); time.sleep(2)
    def title(self):   # 经 GNOME Shell 的窗口查询接口读标题（--unsafe-mode 下可用）
        for w in shell_titles():
            if w.startswith("V:"): return re.sub(r" — Mozilla Firefox$| - Mozilla Firefox$", "", w)[2:]
        return None
    def reset(self, text): keys("{C-a}{BS}" + text); time.sleep(0.3)
    def read(self): time.sleep(0.6); return self.title()
    can_select = True


def run(app):
    print(f"—— {app.name} ——", flush=True)
    app.start()
    app.reset("ab")
    first = app.read()
    if first != "ab":
        R(False, f"{app.name}：测试环境就绪（直接打字）", f"得到 [{first}]")
        app.stop(); return
    cases = [("{Enter}", "x^{2}", "Enter 上屏 LaTeX"), ("{C-Enter}", "x²", "Ctrl+Enter 上屏 Unicode"), ("{A-Enter}", "$x^{2}$", "Alt+Enter 带 $")]
    for commit, want, label in cases:
        app.reset("ab")
        ok = formula(commit)
        got = app.read()
        R(ok and got == "ab" + want, f"{app.name}：{label}", f"期望 [ab{want}] 得到 [{got}]" + ("" if ok else "（框没正常弹出/收起）"))
        if os.environ.get("LA_DIAG") and got != "ab" + want:
            app.reset(""); keys("z"); print("  诊断：上屏失败后补打 z →", repr(app.read()), flush=True)
            keys("{C-v}"); print("  诊断：再按 Ctrl+V →", repr(app.read()), flush=True)
            keys("{S-Insert}" if False else ""); ks(0xffe1, True); ks(0xff63, True); ks(0xff63, False); ks(0xffe1, False)
            print("  诊断：再按 Shift+Insert →", repr(app.read()), flush=True)
            break
    if app.can_select:
        app.reset("a+b")
        keys("{C-a}"); time.sleep(0.3)
        ok = formula("{Enter}", "{End}{Left}+c")   # 同 Windows 版：光标移到收尾 $ 之前再加
        got = app.read()
        sel = next((l for l in reversed(log_tail(12)) if "弹框" in l), "")
        R(ok and got == "a+b+c", f"{app.name}：选中后替换", f"得到 [{got}]；{sel[sel.find('选区'):] if '选区' in sel else ''}")
        # 选中过的文字已经用掉、现在没有选中：再唤起不能把旧内容带进来（浏览器的主选区不会自己清空）
        app.reset("q")
        ok = formula("{Enter}")
        got = app.read()
        R(ok and got == "qx^{2}", f"{app.name}：没选中时不带入旧内容", f"期望 [qx^{{2}}] 得到 [{got}]")
    app.stop()
    time.sleep(1)


open(OUT, "w").close()
time.sleep(3); keys("{Esc}"); time.sleep(1)   # 退出概览
only = sys.argv[1:]
if only == ["PRIMARY"]:   # 诊断：浏览器取消选中后主选区还在不在
    for b in (Chrome("Chrome"), Firefox("Firefox")):
        b.start()
        for step, prep in [("选中 a+b", "{C-a}{BS}a+b{C-a}"), ("按 End 取消选中", "{End}"), ("再打字 q", "q")]:
            keys(prep); time.sleep(0.4)
            subprocess.Popen([EXE, "--toggle"]); wait(box_shown, 4); time.sleep(0.6); keys("{Esc}"); wait(lambda: not box_shown(), 3); time.sleep(0.4)
            print(f"{b.name}｜{step} →", next((l[l.find('主选区'):] for l in reversed(log_tail(8)) if "主选区" in l), "无记录"), flush=True)
        b.stop(); time.sleep(1)
    sys.exit(0)
apps = [Editor("GNOME 文本编辑器", "gnome-text-editor"), Editor("gedit", "gedit"),
        Terminal("Ptyxis 终端", "ptyxis", ["-s"]), Terminal("GNOME 终端", "gnome-terminal", ["--wait"]),
        Chrome("Chrome"), Firefox("Firefox")]
for a in apps:
    if only and not any(o in a.name or o == a.__class__.__name__ for o in only): continue
    if shutil.which(getattr(a, "cmd", {"Chrome": "google-chrome", "Firefox": "firefox"}.get(a.__class__.__name__, ""))) is None: continue
    try: run(a)
    except Exception as e:
        R(False, f"{a.name}：测试出错", repr(e)[:150]); a.stop()
# Chrome + 智能拼音开着
if not only or "IME" in only:
    subprocess.run(["ibus", "engine", "libpinyin"], capture_output=True, timeout=5); time.sleep(1)
    c = Chrome("Chrome（智能拼音开着）")
    try:
        c.start(); keys("{C-a}{BS}"); time.sleep(0.3)
        keys("ni "); cn = wait(lambda: c.title() == "你", 3)
        if cn:
            ok = formula("{Enter}"); got = c.read()
            R(ok and got == "你x^{2}", "Chrome（智能拼音开着）：进框自动切英文、上屏", f"得到 [{got}]")
            keys("ni "); time.sleep(1); got2 = c.title()
            R(got2 == "你x^{2}你", "Chrome（智能拼音开着）：回来仍是中文", f"得到 [{got2}]")
        else:
            R(False, "Chrome（智能拼音开着）：测试环境就绪", f"[{c.title()}]")
    except Exception as e:
        R(False, "Chrome（智能拼音）：测试出错", repr(e)[:150])
    c.stop()
    subprocess.run(["ibus", "engine", "xkb:us::eng"], capture_output=True, timeout=5)
s = f"通过 {passed}，失败 {failed}"
print(s); open(OUT, "a").write(s + "\n")
bus.call_sync(RD, sess, RD + ".Session", "Stop", None, None, 0, 3000)
sys.exit(0 if failed == 0 else 1)
