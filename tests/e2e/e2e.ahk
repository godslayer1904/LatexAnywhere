#Requires AutoHotkey v2.0
#Warn All, StdOut
#NoTrayIcon
; 沙盒里的端到端测试：真实键盘事件 → LatexAnywhere 公式框 → 上屏到 Win32 编辑框，读回内容逐条比对。
; 出错只写结果文件，绝不弹窗。
SendMode "Event"
SetKeyDelay 20, 10
global OUT := "C:\Results\e2e.out.txt", rep := "", pass := 0, fail := 0, shotN := 0
OnError((e, m) => (FileOpen(OUT, "w", "UTF-8").Write(rep "`n运行错误：" e.Message " " e.Extra " 行 " e.Line), ExitApp(1)))

R(ok, name, detail := "") {
	global rep, pass, fail
	rep .= (ok ? "✓ " : "✗ ") name (detail != "" ? "  " detail : "") "`n"
	ok ? pass++ : fail++
	FileOpen(OUT, "w", "UTF-8").Write(rep)
}

BoxHwnd() {
	for h in WinGetList("ahk_class LatexAnywhereBox")
		if DllCall("IsWindowVisible", "ptr", h)
			return h
	return 0
}

WaitBox(visible, ms := 3000) {
	t := A_TickCount
	while (A_TickCount - t < ms) {
		if (!!BoxHwnd() == visible)
			return A_TickCount - t
		Sleep 5
	}
	return -1
}

Shot(name) {
	global shotN
	h := BoxHwnd()
	if !h
		return
	WinGetPos &x, &y, &w, &hh, h
	shotN++
	f := "C:\Results\shot" Format("{:02}", shotN) "_" name ".png"
	ps := "Add-Type -AssemblyName System.Drawing; $b = New-Object Drawing.Bitmap " w "," hh "; $g = [Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(" x "," y ",0,0,$b.Size); $b.Save('" f "')"
	RunWait 'powershell -NoProfile -Command "' ps '"', , "Hide"
	WinActivate "ahk_id " h   ; 截图用的 PowerShell 可能抢走焦点：交还给公式框
	WinWaitActive "ahk_id " h, , 2
	Sleep 100
}

; 键序列：{Tab} {Enter} {S-Enter} {Left} 等写成花括号，其余逐字原样输入
TypeKeys(s) {
	i := 1
	while (i <= StrLen(s)) {
		if (SubStr(s, i, 1) == "{" && RegExMatch(s, "\G\{([A-Za-z\-]+)\}", &m, i)) {
			k := m[1]
			k := StrReplace(k, "S-", "+")
			k := StrReplace(k, "C-", "^")
			if InStr(k, "+") == 1 || InStr(k, "^") == 1
				Send SubStr(k, 1, 1) "{" SubStr(k, 2) "}"
			else
				Send "{" k "}"
			i += StrLen(m[0])
		} else {
			SendText SubStr(s, i, 1)
			i++
		}
		Sleep 15
	}
}

global G := Gui("+Resize", "LatexAnywhere 测试目标"), GEd := G.Add("Edit", "w700 h200 Multi WantTab")
G.Show("x60 y300")
WinWaitActive "ahk_id " G.Hwnd, , 5
GEd.Focus()
Sleep 800

lat := []
TestCase(name, keys, commit, expected, shotName := "") {
	global GEd, lat
	GEd.Value := ""
	WinActivate "ahk_id " G.Hwnd
	GEd.Focus()
	Sleep 150
	Send "!l"
	t := WaitBox(true)
	if (t < 0) {
		R(false, name, "公式框没有弹出")
		return
	}
	lat.Push(t)
	Sleep 150
	TypeKeys(keys)
	Sleep 250
	if (shotName != "")
		Shot(shotName)
	Send commit
	h := WaitBox(false)
	Sleep 700   ; 粘贴等待 + 剪贴板恢复
	got := StrReplace(GEd.Value, "`r", "")
	R(h >= 0 && (expected == "__ANY__" || got == expected), name, "期望 [" expected "] 得到 [" got "]" (h < 0 ? "（框没收起）" : ""))
}

rep .= "开始 " A_Now "`n"
TestCase("x^2 Tab 后上屏（LaTeX）", "x^2{Tab}", "{Enter}", "x^{2}", "x2")
TestCase("分式 Tab 跳位", "//a{Tab}b{Tab}", "{Enter}", "\frac{a}{b}", "frac")
TestCase("希腊字母", "@a+@b", "{Enter}", "\alpha+\beta")
TestCase("矩阵 Tab、Enter、Shift+Enter 退出", "pmat1{Tab}2{Enter}3{Tab}4{S-Enter}", "{Enter}", "\begin{pmatrix}1 & 2 \\  3 & 4\end{pmatrix}", "pmatrix")
TestCase("求和占位", "sum{Tab}k{Tab}0{Tab}n{Tab}", "{Enter}", "\sum_{k=0}^{n}", "sum")
TestCase("Ctrl+Enter 转 Unicode", "x^2{Tab}+@a", "^{Enter}", "x²+α")
TestCase("Alt+Enter 保留 $", "x^2{Tab}", "!{Enter}", "$x^{2}$")
TestCase("Esc 取消不插入", "x^2", "{Esc}", "")

; Ctrl+Shift+Enter：MathML 文本
GEd.Value := ""
TestCase("Ctrl+Shift+Enter 出 MathML", "x^2{Tab}", "^+{Enter}", "__ANY__")
got := StrReplace(GEd.Value, "`r", "")
R(InStr(got, "<math") == 1 && InStr(got, "<msup>"), "MathML 内容正确", SubStr(got, 1, 60))

; 选中文字再唤起：带入编辑、替换选区
GEd.Value := "a+b"
WinActivate "ahk_id " G.Hwnd
GEd.Focus()
Send "^a"
Sleep 150
Send "!l"
WaitBox(true)
Sleep 200
Shot("selection")
TypeKeys("{End}{Left}+c")
Send "{Enter}"
WaitBox(false)
Sleep 700
R(StrReplace(GEd.Value, "`r", "") == "a+b+c", "选中带入并替换", "得到 [" GEd.Value "]")

; ---------------- 浏览器目标（Edge，Chromium：MSAA 光标、UI Automation 取选区、粘贴） ----------------
EdgeVal() {
	t := WinGetTitle("ahk_exe msedge.exe")
	p := InStr(t, "VAL:")
	if !p
		return "<无标题>"
	v := SubStr(t, p + 4)
	v := RegExReplace(v, " [-—] .*$", "")
	return StrReplace(v, "\n", "`n")
}
Run '"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" --no-first-run --no-default-browser-check --user-data-dir=C:\EdgeProfile --app=file:///C:/E2E/target.html'
if !WinWait("ahk_exe msedge.exe", , 20) {
	R(false, "Edge 启动", "20 秒内没出现窗口")
} else {
	Sleep 4000
	WinActivate "ahk_exe msedge.exe"
	Sleep 800
	EdgeCase(name, keys, commit, expected) {
		WinActivate "ahk_exe msedge.exe"
		Sleep 200
		Send "{F9}"
		Sleep 300
		Send "!l"
		t := WaitBox(true)
		if (t < 0) {
			R(false, name, "公式框没有弹出")
			return
		}
		Sleep 150
		TypeKeys(keys)
		Sleep 250
		Send commit
		WaitBox(false)
		Sleep 900
		got := EdgeVal()
		R(got == expected, name, "期望 [" expected "] 得到 [" got "]（弹框 " t " ms）")
	}
	EdgeCase("Edge：x^2 上屏", "x^2{Tab}", "{Enter}", "x^{2}")
	EdgeCase("Edge：分式 + Unicode", "//1{Tab}2{Tab}", "^{Enter}", "1/2")
	; 选中再唤起（UI Automation 读选区）
	WinActivate "ahk_exe msedge.exe"
	Send "{F8}"
	Sleep 400
	Send "!l"
	WaitBox(true)
	Sleep 200
	Shot("edge_selection")
	TypeKeys("{End}{Left}+c")
	Send "{Enter}"
	WaitBox(false)
	Sleep 900
	R(EdgeVal() == "a+b+c", "Edge：选中带入并替换", "得到 [" EdgeVal() "]")
	WinClose "ahk_exe msedge.exe"
}

; 剪贴板保护
A_Clipboard := "原来的剪贴板内容"
Sleep 200
TestCase("剪贴板保护用例", "//1{Tab}2{Tab}", "{Enter}", "\frac{1}{2}")
cb := A_Clipboard
want := "原来的剪贴板内容"
hx(t) {
	o := ""
	loop parse t
		o .= Format("{:04X} ", Ord(A_LoopField))
	return o
}
rep .= "  剪贴板 " hx(cb) "`n  期望   " hx(want) "`n"
R(cb == want, "上屏后剪贴板原样", "现在是 [" cb "] 长度 " StrLen(cb) " 末字符 " (StrLen(cb) ? Ord(SubStr(cb, -1)) : 0))

; 弹框延迟
s := 0, mx := 0
for v in lat
	s += v, mx := Max(mx, v)
R(true, "弹框延迟（含取选区）", "平均 " Round(s / lat.Length) " ms，最大 " mx " ms，共 " lat.Length " 次")

; 连按 Alt+L 不出问题
WinActivate "ahk_id " G.Hwnd
GEd.Focus()
loop 6 {
	Send "!l"
	Sleep 60
}
Sleep 800
if BoxHwnd()
	Send "{Esc}"
Sleep 500
cnt := 0
for p in ComObjGet("winmgmts:").ExecQuery("select ProcessId from Win32_Process where Name='LatexAnywhere.exe'")
	cnt++
R(cnt == 2, "连按 6 次后进程正常（守护 + 工作）", "进程数 " cnt)
TestCase("连按之后照常可用", "x^2{Tab}", "{Enter}", "x^{2}")

; 内存
mem := ""
for p in ComObjGet("winmgmts:").ExecQuery("select ProcessId, CommandLine, PrivatePageCount, WorkingSetSize from Win32_Process where Name='LatexAnywhere.exe'")
	mem .= (InStr(p.CommandLine, "--worker") ? "工作进程" : "守护进程") " 私有 " Round(p.PrivatePageCount / 1048576, 1) " MB / 工作集 " Round(p.WorkingSetSize / 1048576, 1) " MB；"
R(true, "内存", mem)

; ---------------- 崩溃自动恢复：强行结束工作进程，守护进程应在 1 秒左右拉起新的 ----------------
WorkerPid() {
	for p in ComObjGet("winmgmts:").ExecQuery("select ProcessId, CommandLine from Win32_Process where Name='LatexAnywhere.exe'")
		if InStr(p.CommandLine, "--worker")
			return p.ProcessId
	return 0
}
old := WorkerPid()
RunWait 'taskkill /PID ' old ' /F', , "Hide"
t0 := A_TickCount, nw := 0
while (A_TickCount - t0 < 5000) {
	nw := WorkerPid()
	if (nw && nw != old)
		break
	Sleep 50
}
R(nw && nw != old, "工作进程被强杀后自动重启", "用时 " (A_TickCount - t0) " ms（旧 " old " → 新 " nw "）")
Sleep 1500
TestCase("重启后照常可用", "x^2{Tab}", "{Enter}", "x^{2}")

; ---------------- 浸泡（缩短版）：连续弹框—打字—上屏 300 次，看内存是否增长 ----------------
MemOf(pid) {
	for p in ComObjGet("winmgmts:").ExecQuery("select PrivatePageCount from Win32_Process where ProcessId=" pid)
		return Round(p.PrivatePageCount / 1048576, 1)
	return -1
}
wp := WorkerPid()
m0 := MemOf(wp), samples := "", soakFail := 0
loop 300 {
	GEd.Value := ""
	WinActivate "ahk_id " G.Hwnd
	GEd.Focus()
	Send "!l"
	if (WaitBox(true, 2000) < 0) {
		soakFail++
		continue
	}
	SendText "a_" Mod(A_Index, 10)
	Send "{Tab}"
	SendText "+@b"
	Send "{Enter}"
	WaitBox(false, 2000)
	Sleep 450
	if (StrReplace(GEd.Value, "`r", "") != "a_{" Mod(A_Index, 10) "}+\beta")
		soakFail++
	if (Mod(A_Index, 50) == 0)
		samples .= A_Index ":" MemOf(wp) "MB "
}
m1 := MemOf(wp)
R(soakFail == 0, "浸泡 300 次全部正确", "失败 " soakFail " 次")
R(m1 - m0 < 10, "浸泡 300 次内存不持续增长", "开始 " m0 " MB → 结束 " m1 " MB；采样 " samples)

; 紧急强杀
Send "#!l"
Sleep 2500
cnt := 0
for p in ComObjGet("winmgmts:").ExecQuery("select ProcessId from Win32_Process where Name='LatexAnywhere.exe'")
	cnt++
R(cnt == 0, "Win+Alt+L 紧急强杀（守护进程一并退出）", "剩余进程 " cnt)

rep .= "`n通过 " pass "，失败 " fail "`n"
FileOpen(OUT, "w", "UTF-8").Write(rep)
ExitApp 0
