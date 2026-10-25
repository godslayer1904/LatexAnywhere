# 自测：开一个自己的输入窗口 → Alt+L → 打 x^2 → 截图 → Enter → 读回窗口内容
# 只操作自己开的窗口（标题唯一），绝不找系统里已经开着的记事本等程序——那可能是用户正在编辑的文件。
import ctypes, os, time, subprocess, sys, tempfile, uuid
HERE = os.path.dirname(os.path.abspath(__file__))
u = ctypes.windll.user32
def k(vk, up=False): u.keybd_event(vk, 0, 2 if up else 0, 0)
def tap(vk, mods=()):
    for m in mods: k(m); time.sleep(0.02)
    k(vk); time.sleep(0.02); k(vk, True)
    for m in reversed(mods): time.sleep(0.02); k(m, True)
    time.sleep(0.06)
def typestr(s):
    for c in s:
        r = u.VkKeyScanW(ord(c)); vk = r & 0xFF; sh = (r >> 8) & 1
        tap(vk, (0x10,) if sh else ())
def shot(name):
    subprocess.run(["powershell", "-NoProfile", "-Command",
        "Add-Type -AssemblyName System.Windows.Forms,System.Drawing; $b=[System.Windows.Forms.Screen]::AllScreens | ? Primary; $b=$b.Bounds; "
        "$bmp=New-Object Drawing.Bitmap $b.Width,$b.Height; $g=[Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location,[Drawing.Point]::Empty,$b.Size); "
        f"$bmp.Save('{name}')"])

# 自己的输入窗口（tkinter，标准库自带）：内容一变就写进临时文件
title = "LatexAnywhere selftest " + uuid.uuid4().hex[:8]
out = os.path.join(tempfile.gettempdir(), title.replace(" ", "_") + ".txt")
app = (
    "import tkinter as tk, sys\n"
    "r = tk.Tk(); r.title(sys.argv[1]); r.geometry('600x200')\n"
    "t = tk.Text(r); t.pack(fill='both', expand=True); t.focus_set()\n"
    "def save(e=None): open(sys.argv[2], 'w', encoding='utf-8').write(t.get('1.0', 'end-1c'))\n"
    "t.bind('<<Modified>>', lambda e: (save(), t.edit_modified(False)))\n"
    "r.after(300, lambda: (r.lift(), r.focus_force(), t.focus_set())); r.mainloop()\n"
)
p = subprocess.Popen([sys.executable, "-c", app, title, out]); time.sleep(2)
h = u.FindWindowW(None, title)
if not h: print("测试窗口没出来"); p.kill(); sys.exit(1)
u.SetForegroundWindow(h); time.sleep(0.8)
if u.GetForegroundWindow() != h: print("测试窗口没拿到前台"); p.kill(); sys.exit(1)
want = sys.argv[1] if len(sys.argv) > 1 else "x^2"
tap(0x4C, (0x12,)); time.sleep(1.2)
typestr(want); time.sleep(0.8)
shot(os.path.join(HERE, "shot_box.png"))
tap(0x0D); time.sleep(1.2)
shot(os.path.join(HERE, "shot_after.png"))
got = open(out, encoding="utf-8").read() if os.path.exists(out) else ""
p.kill()
print("上屏结果：", repr(got))
print("完成")
