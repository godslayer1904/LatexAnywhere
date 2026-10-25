# LatexAnywhere 快速原型（Python）：Alt+L 在光标处弹出公式框，框里是原版 Latex Suite（www/ 下的页面），
# Enter 插回原输入框，Shift+Enter 换行，Esc 取消；选中内容时带进框里，改完替换选区。
import ctypes
import ctypes.wintypes as wt
import json
import os
import threading
import time

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

HERE = os.path.dirname(os.path.abspath(__file__))
LOG = open(os.path.join(HERE, f"log-{os.environ.get('COMPUTERNAME', 'pc')}.txt"), "a", encoding="utf-8")   # 按主机分开：两台机器经 Syncthing 同步，同一文件会冲突
def print(*a, **k):                     # pythonw 没有控制台：日志写文件（超过 1MB 清空重写）
    if LOG.tell() > 1_000_000:
        LOG.seek(0); LOG.truncate()
    LOG.write(time.strftime("%H:%M:%S ") + " ".join(str(x) for x in a) + "\n")
    LOG.flush()
print("启动：pid", os.getpid())
try:
    import webview
except Exception:
    import traceback
    print("启动失败（加载 pywebview）：", traceback.format_exc())
    raise SystemExit(1)
W, H0 = 420, 60
MOD_ALT, MOD_NOREPEAT, VK_L = 0x0001, 0x4000, 0x4C
WM_HOTKEY = 0x0312
CF_UNICODETEXT = 13

# ---------- 键盘与剪贴板 ----------
class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD), ("time", wt.DWORD), ("dwExtraInfo", ctypes.c_size_t)]

class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [("ki", KEYBDINPUT), ("pad", ctypes.c_byte * 32)]
    _anonymous_ = ("u",)
    _fields_ = [("type", wt.DWORD), ("u", _U)]

def send_keys(seq):
    """seq: [(vk, up)]，一次 SendInput 原子发出"""
    arr = (INPUT * len(seq))()
    for i, (vk, up) in enumerate(seq):
        arr[i].type = 1
        arr[i].ki = KEYBDINPUT(vk, 0, 2 if up else 0, 0, 0)
    user32.SendInput(len(seq), arr, ctypes.sizeof(INPUT))

def chord(mod, key):
    send_keys([(mod, False), (key, False), (key, True), (mod, True)])

def key_down(vk):
    return user32.GetAsyncKeyState(vk) & 0x8000

def wait_mods_released(timeout=0.8):
    t = time.time()
    while time.time() - t < timeout and any(key_down(v) for v in (0x12, 0x11, 0x10, 0x5B, 0x5C)):
        time.sleep(0.01)

user32.GetClipboardData.restype = wt.HANDLE
user32.SetClipboardData.argtypes = [wt.UINT, wt.HANDLE]
user32.SetClipboardData.restype = wt.HANDLE
kernel32.GlobalLock.argtypes = [wt.HGLOBAL]
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalUnlock.argtypes = [wt.HGLOBAL]
kernel32.GlobalAlloc.argtypes = [wt.UINT, ctypes.c_size_t]
kernel32.GlobalAlloc.restype = wt.HGLOBAL
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.FindWindowW.restype = wt.HWND
user32.GetDpiForWindow.argtypes = [wt.HWND]
user32.GetKeyboardLayout.argtypes = [wt.DWORD]
user32.GetKeyboardLayout.restype = ctypes.c_size_t
user32.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user32.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.c_void_p]
user32.AttachThreadInput.argtypes = [wt.DWORD, wt.DWORD, wt.BOOL]
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.c_void_p]
user32.AttachThreadInput.argtypes = [wt.DWORD, wt.DWORD, wt.BOOL]
user32.FindWindowExW.argtypes = [wt.HWND, wt.HWND, wt.LPCWSTR, wt.LPCWSTR]
user32.FindWindowExW.restype = wt.HWND
user32.PhysicalToLogicalPointForPerMonitorDPI.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]
user32.SetWindowPos.argtypes = [wt.HWND, wt.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wt.UINT]
user32.GetForegroundWindow.restype = wt.HWND

def clip_get():
    for _ in range(10):
        if user32.OpenClipboard(0):
            break
        time.sleep(0.02)
    else:
        return None
    try:
        h = user32.GetClipboardData(CF_UNICODETEXT)
        if not h:
            return ""
        p = kernel32.GlobalLock(h)
        s = ctypes.wstring_at(p)
        kernel32.GlobalUnlock(h)
        return s
    finally:
        user32.CloseClipboard()

def clip_set(s):
    for _ in range(10):
        if user32.OpenClipboard(0):
            break
        time.sleep(0.02)
    else:
        return False
    try:
        user32.EmptyClipboard()
        if s is None:
            return True
        data = ctypes.create_unicode_buffer(s)
        h = kernel32.GlobalAlloc(0x0042, ctypes.sizeof(data))
        p = kernel32.GlobalLock(h)
        ctypes.memmove(p, data, ctypes.sizeof(data))
        kernel32.GlobalUnlock(h)
        user32.SetClipboardData(CF_UNICODETEXT, h)
        return True
    finally:
        user32.CloseClipboard()

def grab_selection():
    """Ctrl+C 取选区，取完恢复剪贴板（原型只保留文字内容）"""
    saved = clip_get()
    clip_set(None)
    seq0 = user32.GetClipboardSequenceNumber()
    # 用户还按着 Alt（Alt+L）：先补屏蔽键防止激活菜单，再补 Alt 抬起，然后马上 Ctrl+C——不用等用户松手
    send_keys([(0xFF, False), (0xFF, True), (0x12, True), (0xA4, True), (0xA5, True),
               (0x11, False), (0x43, False), (0x43, True), (0x11, True)])
    t = time.time()
    while time.time() - t < 0.1 and user32.GetClipboardSequenceNumber() == seq0:   # 没选中时只能等超时；常见软件响应复制在 50ms 内
        time.sleep(0.01)
    got = clip_get() if user32.GetClipboardSequenceNumber() != seq0 else ""
    clip_set(saved)
    if not got or got.endswith("\n") or len(got) > 4000:   # 没选中时有的编辑器会复制整行
        return ""
    return got.replace("\r\n", "\n")

def paste_text(s, settle=0.3, after=None):
    """经剪贴板粘贴。上屏的内容留在剪贴板里，不恢复原内容：万一目标程序没接住，用户还能自己再 Ctrl+V 一次。
    settle：按 Ctrl+V 后等多久再做 after（Word/PowerPoint 转换 MathML 慢，实测）。after：粘贴完成后要做的事"""
    clip_set(s)
    time.sleep(0.03)
    # 用户上屏时往往还按着 Ctrl+Shift（Ctrl+Shift+Enter）或 Alt（Alt+Enter）：直接发 Ctrl+V 会变成
    # Ctrl+Shift+V（Office 的「粘贴格式」，什么都贴不进去）或 Ctrl+Alt+V（选择性粘贴）。先补屏蔽键和 Shift/Alt 抬起
    send_keys([(0xFF, False), (0xFF, True),
               (0x10, True), (0xA0, True), (0xA1, True), (0x12, True), (0xA4, True), (0xA5, True),
               (0x11, False), (0x56, False), (0x56, True), (0x11, True)])
    time.sleep(settle)
    if after:
        after()

# ---------- 光标位置 ----------
class GUITHREADINFO(ctypes.Structure):
    _fields_ = [("cbSize", wt.DWORD), ("flags", wt.DWORD), ("hwndActive", wt.HWND), ("hwndFocus", wt.HWND),
                ("hwndCapture", wt.HWND), ("hwndMenuOwner", wt.HWND), ("hwndMoveSize", wt.HWND),
                ("hwndCaret", wt.HWND), ("rcCaret", wt.RECT)]

def caret_pos():
    gti = GUITHREADINFO(cbSize=ctypes.sizeof(GUITHREADINFO))
    if user32.GetGUIThreadInfo(0, ctypes.byref(gti)) and gti.hwndCaret:
        pt = wt.POINT(gti.rcCaret.left, gti.rcCaret.bottom)
        user32.ClientToScreen(gti.hwndCaret, ctypes.byref(pt))
        if pt.x or pt.y:
            return pt.x, pt.y
    acc = acc_caret()                       # Electron（Claude、VS Code……）、浏览器：系统光标为空，用 MSAA 光标对象
    if acc:
        return acc
    pt = wt.POINT()
    user32.GetCursorPos(ctypes.byref(pt))
    return pt.x, pt.y + 20

_OLEACC = None
def acc_caret():
    global _OLEACC
    try:
        import comtypes, comtypes.client, comtypes.automation
        from ctypes import POINTER, byref, c_long
        comtypes.CoInitialize()
        if _OLEACC is None:                      # 辅助功能组件只加载一次（每次加载要几十到上百毫秒）
            _OLEACC = comtypes.client.GetModule("oleacc.dll")
        oleacc = _OLEACC
        gti = GUITHREADINFO(cbSize=ctypes.sizeof(GUITHREADINFO))
        user32.GetGUIThreadInfo(0, ctypes.byref(gti))
        hwnd = gti.hwndFocus or user32.GetForegroundWindow()
        acc = POINTER(oleacc.IAccessible)()
        if ctypes.oledll.oleacc.AccessibleObjectFromWindow(hwnd, -8, byref(oleacc.IAccessible._iid_), byref(acc)) != 0 or not acc:
            return None
        l, t, w, h = acc.accLocation(0)          # comtypes：输出参数作为返回值
        if l or t:
            # Chromium 给的是物理像素；本程序按逻辑坐标摆窗口（高分屏缩放下两者不同），换算一下
            return l, t + h
    except Exception as e:
        print("MSAA 光标读取失败：", e)
    return None

imm32 = ctypes.WinDLL("imm32")
imm32.ImmGetDefaultIMEWnd.argtypes = [wt.HWND]
imm32.ImmGetDefaultIMEWnd.restype = wt.HWND
user32.SendMessageTimeoutW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM, wt.UINT, wt.UINT, ctypes.c_void_p]

def focus_ime():
    """当前焦点窗口的默认输入法窗口和键盘语言"""
    gti = GUITHREADINFO(cbSize=ctypes.sizeof(GUITHREADINFO))
    user32.GetGUIThreadInfo(0, ctypes.byref(gti))
    f = gti.hwndFocus or user32.GetForegroundWindow()
    lang = user32.GetKeyboardLayout(user32.GetWindowThreadProcessId(f, None)) & 0xFFFF
    return imm32.ImmGetDefaultIMEWnd(f), lang

def ime_is_native(ime):
    o, c = ctypes.c_size_t(0), ctypes.c_size_t(0)
    user32.SendMessageTimeoutW(ime, 0x283, 0x005, 0, 2, 100, ctypes.byref(o))   # IMC_GETOPENSTATUS
    user32.SendMessageTimeoutW(ime, 0x283, 0x001, 0, 2, 100, ctypes.byref(c))   # IMC_GETCONVERSIONMODE
    return bool(o.value) and bool(c.value & 1)

def ime_restore_native(prev_hwnd):
    """回到原来的输入框后：原来是中文/日文、现在是英文，就切回去（只在前台确实是原窗口时按键）"""
    time.sleep(0.1)
    ime, lang = focus_ime()
    if not ime or ime_is_native(ime) or user32.GetForegroundWindow() != prev_hwnd:
        return
    send_keys([(0x16, False), (0x16, True)])                 # VK_IME_ON：Windows 自带的「打开输入法」键
    time.sleep(0.12)
    if not ime_is_native(ime) and lang == 0x0804 and user32.GetForegroundWindow() == prev_hwnd:
        send_keys([(0xA0, False), (0xA0, True)])             # 微软拼音：Shift 单击切回中文
        time.sleep(0.12)
    print("回原输入框切回母语输入：", "成功" if ime_is_native(ime) else "失败", hex(lang))

def ime_english():
    """进框自动切英文（中文、日文都切）。只在前台确实是公式框时执行，只动框里编辑器的输入法，不碰别的软件。
    注意：编辑器跑在 WebView2 的浏览器进程里，焦点窗口不属于本进程，所以按「前台窗口是不是公式框」判断"""
    box_hwnd = user32.FindWindowW(None, "LatexAnywhere")
    focus = 0
    for _ in range(30):
        gti = GUITHREADINFO(cbSize=ctypes.sizeof(GUITHREADINFO))
        user32.GetGUIThreadInfo(0, ctypes.byref(gti))
        if box_hwnd and user32.GetForegroundWindow() == box_hwnd and gti.hwndFocus:
            focus = gti.hwndFocus
            break
        time.sleep(0.02)
    if not focus:
        print("前台不是公式框，不动输入法")
        return
    ime = imm32.ImmGetDefaultIMEWnd(focus)
    if not ime:
        return
    tid = user32.GetWindowThreadProcessId(focus, None)
    lang = user32.GetKeyboardLayout(tid) & 0xFFFF
    def native():                                # 读状态有效（旧式 IMM 查询），但 Chromium 走 TSF，旧式「设置」消息不理会
        o, c = ctypes.c_size_t(0), ctypes.c_size_t(0)
        user32.SendMessageTimeoutW(ime, 0x283, 0x005, 0, 2, 100, ctypes.byref(o))       # IMC_GETOPENSTATUS
        user32.SendMessageTimeoutW(ime, 0x283, 0x001, 0, 2, 100, ctypes.byref(c))       # IMC_GETCONVERSIONMODE
        return bool(o.value) and bool(c.value & 1)
    # 刚弹框时 WebView2（Chromium 走 TSF）那边的状态可能还没同步过来，读到「已是英文」并不可靠（实测会误判）：
    # 不管读数如何都先按一次 VK_IME_OFF，已经是英文时这个键不起作用
    was = native()
    # 1) Windows 自带的「关闭输入法」虚拟键 VK_IME_OFF（微软日文、中文输入法都认）
    if user32.GetForegroundWindow() == box_hwnd:
        send_keys([(0x1A, False), (0x1A, True)])
        time.sleep(0.12)
    # 2) 还是中文：补一次 Shift 单击（微软拼音的中英切换键）
    if native() and lang == 0x0804 and user32.GetForegroundWindow() == box_hwnd:
        send_keys([(0xA0, False), (0xA0, True)])
        time.sleep(0.12)
    print("框内输入法切英文：", "成功" if not native() else "失败", hex(lang), "（切之前读数：", "中文" if was else "英文", "）")

class MONITORINFO(ctypes.Structure):
    _fields_ = [("cbSize", wt.DWORD), ("rcMonitor", wt.RECT), ("rcWork", wt.RECT), ("dwFlags", wt.DWORD)]

user32.MonitorFromPoint.argtypes = [wt.POINT, wt.DWORD]
user32.MonitorFromPoint.restype = wt.HANDLE
user32.GetMonitorInfoW.argtypes = [wt.HANDLE, ctypes.POINTER(MONITORINFO)]

def work_area(x, y):
    """(x, y) 所在显示器的工作区（去掉任务栏）"""
    mi = MONITORINFO(cbSize=ctypes.sizeof(MONITORINFO))
    user32.GetMonitorInfoW(user32.MonitorFromPoint(wt.POINT(x, y), 2), ctypes.byref(mi))
    w = mi.rcWork
    return w.left, w.top, w.right, w.bottom

# ---------- 公式框 ----------
class Box:
    def __init__(self):
        self.win = None
        self.prev = 0
        self.ready = False
        self.visible = False
        self.anchor = (0, 0)
        self.size = (60, 0, 0)
        self.lock = threading.Lock()
        self.width = 120                         # 框宽随内容变（页面上报自然宽度）
        self.placed = None                       # 上次摆放的位置；和实际位置不同 = 用户拖动过

    # 页面发来的消息（commit / cancel / size / ready / error）
    def msg(self, m):
        t = m.get("type")
        if t == "commit":
            threading.Thread(target=self.commit, args=(m.get("text", ""), bool(m.get("uni")), bool(m.get("keep")), bool(m.get("raw"))), daemon=True).start()
        elif t == "cancel":
            threading.Thread(target=self.cancel, daemon=True).start()
        elif t == "size":
            self.size = (m.get("h", 60), m.get("tipAbove", 0), m.get("tipBelow", 0))
            if m.get("w"):
                self.width = m["w"]
            if self.visible and (self.size, self.width) != getattr(self, "_last_sz", None):
                self._last_sz = (self.size, self.width)
                self.place()
        elif t == "drag":
            self.drag()
        elif t == "error":
            print("页面错误：", m.get("msg"))

    def place(self):
        x, y = self.anchor                       # 光标底部（屏幕坐标）
        h, above, below = self.size
        hwnd = user32.FindWindowW(None, "LatexAnywhere")
        k = (user32.GetDpiForWindow(hwnd) / 96) if hwnd else 1.0   # 页面尺寸是 CSS 像素，按缩放比例换成物理像素
        total = int(min(h, 460) * k)                 # 窗口 = 卡片的实际尺寸
        l, t, r, b = work_area(x, y)
        maxw = min(int((r - l) * 0.4), int(560 * k))   # 最宽 560（且不超过屏幕 40%）：超出就换行往下长
        w = max(int(60 * k), min(int(self.width * k), maxw))
        # 用户拖动过：留在原位，只改大小
        if hwnd and self.placed:
            rc = wt.RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(rc))
            if (rc.left, rc.top) != self.placed:
                user32.SetWindowPos(hwnd, -1, rc.left, rc.top, w, total, 0x0040)
                self.placed = (rc.left, rc.top)
                self.moved = True
                return
            if getattr(self, "moved", False):
                user32.SetWindowPos(hwnd, -1, rc.left, rc.top, w, total, 0x0040)
                return
        nx = max(l, min(x - 12, r - w))
        # 照 Latex Suite 预览气泡：默认在光标上方，上面放不下就翻到下方
        ny = y - int(28 * k) - total
        if ny < t:
            ny = y + int(4 * k)
        ny = max(t, min(ny, b - total))
        if hwnd:                                 # pywebview 的 resize 对无边框窗口不生效：直接用 Win32 摆
            user32.SetWindowPos(hwnd, -1, nx, ny, w, total, 0x0040)
            self.placed = (nx, ny)
        else:
            self.win.resize(W, total)
            self.win.move(nx, ny)

    def open(self):
        if not self.lock.acquire(blocking=False):      # 上一次还没弹完，忽略连按
            return
        try:
            self._open()
        except Exception:
            import traceback
            print("弹框出错：", traceback.format_exc())
        finally:
            self.lock.release()

    def _open(self):
        if self.visible:
            self.cancel()
            return
        if not self.ready:
            print("页面还没就绪")
            return
        t0 = time.perf_counter()
        ms = lambda: f"{(time.perf_counter() - t0) * 1000:.0f}ms"
        self.prev = user32.GetForegroundWindow()
        pime, _ = focus_ime()
        self.prev_native = bool(pime) and ime_is_native(pime)      # 记下原输入框是不是中文/日文，关框时还原
        self.placed, self.moved, self.width, self._last_sz = None, False, 120, None
        self.anchor = caret_pos()
        t_caret = ms()
        sel = grab_selection()
        t_sel = ms()
        # 一律放进行内公式里编辑（上屏时去掉外层 $）：Latex Suite 只在公式里生效
        doc, a, h = "$" + sel + "$", 1, 1 + len(sel)
        _, _, r, _ = work_area(*self.anchor)
        l = work_area(*self.anchor)[0]
        maxw = min(int((r - l) * 0.4), 560)
        self.win.evaluate_js(f"window.__maxW = {maxw}; window.__reset({json.dumps(doc)}, {a}, {h}); window.__live = false;"
                             "document.querySelector('.cm-content').focus();")   # 和页面只通信一次
        self.visible = True
        self.win.show()
        self.place()                             # 隐藏时改大小不生效：显示后再摆
        self.focus_box()
        self.win.evaluate_js("document.querySelector('.cm-content').focus();")
        print(f"弹框：光标 {t_caret}，选区 {t_sel}，显示 {ms()}（选区长度 {len(sel)}）")
        ime_job(ime_english)                     # 切输入法排队到同一个后台线程，不叠加

    def focus_box(self):
        """把键盘焦点交给框：先把窗口切到前台（借前台线程的输入队列，绕开抢焦点限制），再让 WebView 控件本身拿焦点"""
        hwnd = user32.FindWindowW(None, "LatexAnywhere")
        if hwnd:
            send_keys([(0, False), (0, True)])   # 空按键：让系统认为本程序刚收到输入，才允许切前台
            user32.AllowSetForegroundWindow(0xFFFFFFFF)
            fg = user32.GetForegroundWindow()
            t1 = user32.GetWindowThreadProcessId(fg, None)
            t2 = kernel32.GetCurrentThreadId()
            user32.AttachThreadInput(t1, t2, True)
            user32.SetForegroundWindow(hwnd)
            user32.AttachThreadInput(t1, t2, False)
        try:
            from System import Action
            f = self.win.native
            def go():
                f.Activate()
                f.browser.webview.Focus()
            f.Invoke(Action(go))
        except Exception as e:
            print("WebView 聚焦失败：", e)

    def drag(self):
        """整个框都能拖：在界面线程里释放鼠标捕获，然后当作按住标题栏（Win32 标准拖窗口做法）"""
        hwnd = user32.FindWindowW(None, "LatexAnywhere")
        try:
            from System import Action
            def go():
                user32.ReleaseCapture()
                user32.SendMessageW(hwnd, 0x00A1, 2, 0)   # WM_NCLBUTTONDOWN, HTCAPTION
                self.moved = True
            self.win.native.BeginInvoke(Action(go))
        except Exception as e:
            print("拖动失败：", e)

    def hide(self):
        self.visible = False
        self.win.hide()

    def back(self):
        if self.prev:
            user32.SetForegroundWindow(self.prev)
            time.sleep(0.08)

    def commit(self, text, uni=False, keep=False, raw=False):   # raw：Ctrl+Shift+Enter，页面已转成 MathML，原样粘贴（Word 自动变公式）   # keep：Alt+Enter 上屏，保留外层 $（Typora / Obsidian 里直接是行内公式）          # uni：Ctrl+Enter 上屏，转成 Unicode（x² α ∑），不认 LaTeX 的软件也能直接看
        print("上屏：", "MathML" if raw else "Unicode" if uni else "保留$" if keep else "LaTeX", repr(text[:60]))
        if not keep and not raw and len(text) >= 2 and text.startswith("$") and text.endswith("$") and not text.startswith("$$"):
            text = text[1:-1]                    # 去掉外层 $，只插公式本身
        if uni:
            text = to_unicode(text)
        self.hide()
        self.back()
        if text:
            exe = fg_exe()
            office = exe in ("winword.exe", "powerpnt.exe")
            def leave_formula():
                # 插入公式后光标还在公式里，下一次粘贴会并进同一个公式：先把光标移出公式，再打空格隔开
                # Word：按 → 即可移出（实测）；PowerPoint：光标会落在公式中间，要按 End
                key = 0x23 if exe == "powerpnt.exe" else 0x27
                send_keys([(key, False), (key, True)])
                time.sleep(0.05)
                send_keys([(0x20, False), (0x20, True)])
            paste_text(text, settle=1.2 if (office and raw) else 0.3, after=leave_formula if (office and raw) else None)
        self.restore_ime()

    def cancel(self):
        self.hide()
        self.back()
        self.restore_ime()

    def restore_ime(self):
        if getattr(self, "prev_native", False) and self.prev:
            prev = self.prev
            ime_job(lambda: ime_restore_native(prev))

def fg_exe():
    """前台窗口的程序名（小写）"""
    try:
        import psutil
        pid = wt.DWORD()
        user32.GetWindowThreadProcessId(user32.GetForegroundWindow(), ctypes.byref(pid))
        return psutil.Process(pid.value).name().lower()
    except Exception:
        return ""

def to_unicode(tex):
    """LaTeX → Unicode（用开源库 unicodeit；它不管分式，先按括号配对把 \frac{a}{b} 换成 a/b，嵌套也行）"""
    import unicodeit

    def group(t, i):                             # t[i] == '{'：返回配对括号里的内容和结束位置
        depth = 0
        for j in range(i, len(t)):
            if t[j] == "{":
                depth += 1
            elif t[j] == "}":
                depth -= 1
                if depth == 0:
                    return t[i + 1:j], j + 1
        return t[i + 1:], len(t)

    def fracs(t):
        out, i = "", 0
        while i < len(t):
            for cmd in ("\\dfrac", "\\tfrac", "\\frac"):
                if t.startswith(cmd, i) and t[i + len(cmd):i + len(cmd) + 1] == "{":
                    num, k = group(t, i + len(cmd))
                    den, k = group(t, k) if t[k:k + 1] == "{" else ("", k)
                    num, den = fracs(num), fracs(den)
                    conv = lambda x: unicodeit.replace(x).replace("{", "").replace("}", "")
                    wrap = lambda x: x if len(conv(x)) <= 1 else "(" + x + ")"
                    out += wrap(num) + "/" + wrap(den)
                    i = k
                    break
            else:
                out += t[i]
                i += 1
        return out

    out = unicodeit.replace(fracs(tex))
    return out.replace("{", "").replace("}", "")

box = Box()

class Api:
    """暴露给网页的接口只有这一个方法（不能直接暴露 Box：它带着窗口对象，pywebview 遍历属性会死循环）"""
    def msg(self, m):
        box.msg(m)

def hotkey_loop():
    if not user32.RegisterHotKey(None, 1, MOD_ALT | MOD_NOREPEAT, VK_L):
        print("Alt+L 注册失败：已被别的程序占用", ctypes.get_last_error(), flush=True)
        return
    print("Alt+L 已注册", flush=True)
    if not user32.RegisterHotKey(None, 9, 0x0008 | 0x0001 | MOD_NOREPEAT, VK_L):   # Win+Alt+L：紧急强杀（Ctrl+Alt+Shift 系和 Moonlight 冲突）
        print("Win+Alt+L 注册失败", ctypes.get_last_error())
    msg = wt.MSG()
    last = 0.0
    while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) != 0:
        if msg.message == WM_HOTKEY and msg.wParam == 9:
            hard_exit("紧急热键 Win+Alt+L")
        elif msg.message == WM_HOTKEY:
            now = time.time()
            if now - last < 0.3 or box.lock.locked():   # 防连按：0.3 秒内或上一次还没走完，直接忽略
                continue
            last = now
            threading.Thread(target=box.open, daemon=True).start()

# ---------- 输入法操作排队（只执行最新一个） ----------
_ime_cv = threading.Condition()
_ime_pending = [None]
def ime_job(fn):
    with _ime_cv:
        _ime_pending[0] = fn
        _ime_cv.notify()
def _ime_worker():
    while True:
        with _ime_cv:
            while _ime_pending[0] is None:
                _ime_cv.wait()
            fn, _ime_pending[0] = _ime_pending[0], None
        try:
            fn()
        except Exception as e:
            print("输入法操作出错：", e)
threading.Thread(target=_ime_worker, daemon=True).start()

# ---------- 退出、托盘、自我监控 ----------
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
TASK = "LatexAnywhere"   # 开机自启：登录后 15 秒由计划任务运行 boot.pyw（失败自动重试），比注册表 Run 项可靠

def autostart_on():
    import subprocess
    r = subprocess.run(["schtasks", "/query", "/tn", TASK, "/fo", "csv", "/nh"], capture_output=True, text=True,
                       creationflags=0x08000000)
    return r.returncode == 0 and "Disabled" not in r.stdout and "禁用" not in r.stdout

def autostart_toggle(*_):
    import subprocess
    flag = "/disable" if autostart_on() else "/enable"
    subprocess.run(["schtasks", "/change", "/tn", TASK, flag], capture_output=True, creationflags=0x08000000)
    print("开机自启：", "开" if autostart_on() else "关")

def hard_exit(reason="托盘退出"):
    """强制结束整个进程树（含 WebView2 子进程），任何情况下都最优先"""
    print("退出：", reason)
    try:
        import subprocess
        for p in own_procs()[1:]:                # 只杀自己的 WebView2，不连带别的程序（如打开说明页时起的浏览器）
            subprocess.Popen(["taskkill", "/PID", str(p.pid), "/F"], creationflags=0x08000000)
    finally:
        time.sleep(1)
        os._exit(0)

def own_procs():
    """本程序自己 + 它的 WebView2 子进程（不含从这里打开的浏览器等别的程序）"""
    import psutil
    me = psutil.Process(os.getpid())
    out = [me]
    for c in me.children(recursive=True):
        try:
            if c.name().lower() == "msedgewebview2.exe":
                out.append(c)
        except Exception:
            pass
    return out

def open_help():
    """经 explorer 打开说明页：浏览器挂在 explorer 名下，不算本程序的子进程"""
    import subprocess
    subprocess.Popen(["explorer.exe", os.path.join(HERE, "help.html")], creationflags=0x08000000)

def make_icon():
    """托盘图标：256px 大画布上用 Cambria Math 画居中的 ∑，再缩小（小尺寸直接画会糊、会歪）"""
    from PIL import Image, ImageDraw, ImageFont
    S = 256
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((8, 8, S - 8, S - 8), 52, fill=(127, 109, 242, 255))
    font = None
    for f in ("cambria.ttc", "seguisym.ttf", "segoeui.ttf"):
        try:
            font = ImageFont.truetype(f, 190)
            break
        except OSError:
            pass
    x0, y0, x1, y1 = d.textbbox((0, 0), "∑", font=font)
    d.text(((S - (x1 - x0)) / 2 - x0, (S - (y1 - y0)) / 2 - y0), "∑", font=font, fill="white")
    return img.resize((64, 64), Image.LANCZOS)

def start_tray():
    import pystray
    from PIL import Image, ImageDraw
    img = make_icon()
    menu = pystray.Menu(
        pystray.MenuItem("LatexAnywhere（Alt+L）", None, enabled=False),
        pystray.MenuItem("使用说明", lambda *_: open_help()),
        pystray.MenuItem("开机自启", autostart_toggle, checked=lambda i: autostart_on()),
        pystray.MenuItem("退出（紧急：Win+Alt+L）", lambda *_: hard_exit("托盘退出")),
    )
    icon = pystray.Icon("LatexAnywhere", img, "LatexAnywhere（Alt+L 公式框）", menu)
    icon.run_detached()

def watchdog():
    """自我监控：私有内存超 800MB，或 CPU 连续 15 秒占满一个核，自动退出（只算自己和 WebView2）"""
    hot = 0
    seen = {}                                    # 复用进程对象，cpu_percent 才有上一轮的基准
    while True:
        time.sleep(5)
        try:
            procs = [seen.setdefault(p.pid, p) for p in own_procs()]
            mem = sum(p.memory_info().private for p in procs if p.is_running()) / 2**20
            cpu = sum(p.cpu_percent(None) for p in procs if p.is_running())
        except Exception:
            continue
        hot = hot + 1 if cpu > 90 else 0
        if mem > 800:
            hard_exit(f"内存超限 {mem:.0f}MB")
        if hot >= 3:
            hard_exit(f"CPU 持续占满 {cpu:.0f}%")

def on_started():
    for _ in range(100):                         # 等页面里的 Latex Suite 加载完
        try:
            if box.win.evaluate_js("typeof window.__reset === 'function' && !!document.querySelector('.cm-content')"):
                box.ready = True
                break
        except Exception:
            pass
        time.sleep(0.1)
    print("就绪" if box.ready else "页面加载失败")
    start_tray()
    threading.Thread(target=watchdog, daemon=True).start()
    acc_caret()                                  # 预热：第一次按 Alt+L 不用再等组件加载
    threading.Thread(target=hotkey_loop, daemon=True).start()

if __name__ == "__main__":
    # 只允许一个在跑：已经在运行就直接退出（双击快捷方式、开机自启叠加都不会出现两个）
    kernel32.CreateMutexW(None, False, "Local\\LatexAnywhereFormulaBox")
    if ctypes.get_last_error() == 183:           # ERROR_ALREADY_EXISTS
        print("已有实例在运行，本次退出")
        raise SystemExit(0)
    try:
        box.win = webview.create_window("LatexAnywhere", os.path.join(HERE, "www", "ime.html"), js_api=Api(),
                                        width=W, height=H0, frameless=True, on_top=True, hidden=True,
                                        background_color="#1E1E1E", easy_drag=False, min_size=(60, 20))   # 默认最小高度 100 会把框撑高压住输入框
        webview.start(on_started, http_server=True, private_mode=False)
    except Exception:
        import traceback
        print("启动失败（WebView）：", traceback.format_exc())
        raise SystemExit(1)
