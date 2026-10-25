#Requires AutoHotkey v2.0
#Warn All, StdOut
#NoTrayIcon
; 沙盒里的端到端测试：真实键盘事件 → LatexAnywhere 公式框 → 上屏到 Win32 编辑框，读回内容逐条比对。
; 出错只写结果文件，绝不弹窗。
SendMode "Event"
SetKeyDelay 20, 10
global RES := EnvGet("LA_E2E_RESULTS") || "C:\Results"   ; 结果目录：沙盒里是 C:\Results，真机上用环境变量指定
DirCreate RES
global OUT := RES "\e2e.out.txt", rep := "", pass := 0, fail := 0, shotN := 0
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
	f := RES "\shot" Format("{:02}", shotN) "_" name ".png"
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
	Sleep 700   ; 等粘贴落地
	got := StrReplace(GEd.Value, "`r", "")
	R(h >= 0 && (expected == "__ANY__" || got == expected), name, "期望 [" expected "] 得到 [" got "]" (h < 0 ? "（框没收起）" : ""))
}

rep .= "开始 " A_Now "`n"

; ---------------- 首次启动欢迎窗口：不勾选关掉→下次还弹；勾选关掉→下次不弹 ----------------
AppExe() {
	for p in ComObjGet("winmgmts:").ExecQuery("select ExecutablePath, CommandLine from Win32_Process where Name='LatexAnywhere.exe'")
		if InStr(p.CommandLine, "--worker")
			return p.ExecutablePath
	return ""
}
RestartApp() {
	exe := AppExe()
	RunWait '"' exe '" --quit', , "Hide"
	Sleep 1500
	Run '"' exe '"'
	t := A_TickCount
	while (A_TickCount - t < 8000 && AppExe() == "")
		Sleep 100
	Sleep 1500
}
w1 := WinWait("ahk_class LatexAnywhereHelp", , 10)
R(!!w1, "首次启动弹出欢迎窗口")
if w1 {
	Sleep 500
	Send "{Enter}"   ; 不勾选直接关
	WinWaitClose "ahk_class LatexAnywhereHelp", , 5
}
RestartApp()
w2 := WinWait("ahk_class LatexAnywhereHelp", , 6)
R(!!w2, "没勾选「不再显示」：再次启动仍弹出")
if w2 {
	Sleep 300   ; 等窗口画完再点
	; 勾选「不再显示」：真实鼠标点击（沙盒里不碰用户的鼠标）
	WinActivate "ahk_class LatexAnywhereHelp"
	ControlGetPos &cx, &cy, &cw, &ch, "Button1", "ahk_class LatexAnywhereHelp"
	CoordMode "Mouse", "Client"
	Click cx + 10, cy + ch // 2
	Sleep 300
	R(ControlGetChecked("Button1", "ahk_class LatexAnywhereHelp"), "点复选框勾上「不再显示」")
	R(RegRead("HKCU\Software\LatexAnywhere", "HideWelcome", 0) == 1, "一勾选就已保存（不等关窗）")
	Sleep 300
	; 「开始使用」：用真实鼠标点按钮中心（沙盒里不碰用户的鼠标），和真人操作一致
	WinActivate "ahk_class LatexAnywhereHelp"
	ControlGetPos &bx, &by, &bw, &bh, "Button2", "ahk_class LatexAnywhereHelp"
	CoordMode "Mouse", "Client"
	Click bx + bw // 2, by + bh // 2
	closed := WinWaitClose("ahk_class LatexAnywhereHelp", , 5)
	R(closed, "点「开始使用」关闭欢迎窗口")
	if !closed
		WinClose "ahk_class LatexAnywhereHelp"
}
RestartApp()
w3 := WinWait("ahk_class LatexAnywhereHelp", , 5)
R(!w3, "勾选后再次启动不再弹出")
if w3
	WinClose "ahk_class LatexAnywhereHelp"

; ---------------- 托盘「语言」菜单：真实弹出菜单、用键盘选 ----------------
TrayMenu(keys) {   ; 菜单顺序：使用说明、开机自启、语言 ▸、退出
	DetectHiddenWindows True
	PostMessage 0x800A, 0, 0x205, , "ahk_class LatexAnywhereMsg"   ; WM_TRAY + WM_RBUTTONUP
	DetectHiddenWindows False
	if !WinWait("ahk_class #32768", , 3)
		return false
	Sleep 300
	Send keys
	Sleep 800
	return true
}
LangReg() => RegRead("HKCU\Software\LatexAnywhere", "Language", "(无)")
HelpTitle() {   ; 从托盘打开说明窗口，读标题后关掉
	TrayMenu("{Down}{Enter}")
	if !WinWait("ahk_class LatexAnywhereHelp", , 5)
		return "(没打开)"
	t := WinGetTitle()
	WinClose
	WinWaitClose "ahk_class LatexAnywhereHelp", , 3
	return t
}
sysTitle := HelpTitle()
ok := TrayMenu("{Down 3}{Right}{Down 3}{Enter}")   ; 跟随系统、（分隔线）、简体、繁體、日本語
jaTitle := HelpTitle()
R(ok && LangReg() == "ja" && jaTitle == "LatexAnywhere の使い方", "托盘选「日本語」立即生效并保存", "注册表 " LangReg() "，标题 " jaTitle)
RestartApp()
R(HelpTitle() == "LatexAnywhere の使い方", "重启后仍是日文")
TrayMenu("{Down 3}{Right}{Enter}")   ; 跟随系统
t := HelpTitle()
R(LangReg() == "" && t == sysTitle, "选「跟随系统」恢复系统语言", "注册表 [" LangReg() "]，标题 " t)

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
;只认测试页面自己的窗口（标题带 VAL:）：真机上用户自己的 Edge 也开着，绝不能碰（10-25 家里真机踩过）
SetTitleMatchMode 2
global EW := "VAL: ahk_exe msedge.exe"
EdgeTitles() {
	s := ""
	for h in WinGetList("ahk_exe msedge.exe")
		s .= "[" WinGetTitle(h) "] "
	return s
}
EdgeVal() {
	if !WinExist(EW)
		return "<没有测试窗口；Edge 窗口：" EdgeTitles() ">"
	t := WinGetTitle(EW)
	p := InStr(t, "VAL:")
	if !p
		return "<无标题>"
	v := SubStr(t, p + 4)
	v := RegExReplace(v, " [-—] .*$", "")
	return StrReplace(v, "\n", "`n")
}
Run '"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe" --no-first-run --no-default-browser-check --user-data-dir=' A_Temp '\LAEdgeProfile --app=file:///' StrReplace(A_ScriptDir, '\', '/') '/target.html'
if !WinWait(EW, , 20) {
	R(false, "Edge 启动", "20 秒内没出现测试窗口；Edge 窗口：" EdgeTitles())
} else {
	Sleep 4000
	WinActivate EW
	Sleep 800
	EdgeCase(name, keys, commit, expected) {
		WinActivate EW
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
	WinActivate EW
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
	WinClose EW
}
; 只结束测试用的 Edge（按它独有的配置目录识别）
for p in ComObjGet("winmgmts:").ExecQuery("select ProcessId, CommandLine from Win32_Process where Name='msedge.exe'")
	if InStr(p.CommandLine, "LAEdgeProfile") && !InStr(p.CommandLine, "--type=")
		ProcessClose p.ProcessId

; 上屏的内容留在剪贴板里（四种上屏方式都要）：万一目标没接住，用户还能自己再粘贴
ClipCase(name, keys, commit, expected) {
	A_Clipboard := "上屏前的剪贴板"
	Sleep 200
	TestCase(name, keys, commit, expected == "__MATHML__" ? "__ANY__" : expected)
	cb := A_Clipboard
	ok := expected == "__MATHML__" ? (InStr(cb, "<math") == 1) : (cb == expected)
	R(ok, name "：剪贴板里留着上屏内容", "剪贴板 [" SubStr(cb, 1, 60) "]")
}
ClipCase("剪贴板：Enter", "//1{Tab}2{Tab}", "{Enter}", "\frac{1}{2}")
ClipCase("剪贴板：Alt+Enter", "x^2{Tab}", "!{Enter}", "$x^{2}$")
ClipCase("剪贴板：Ctrl+Enter", "x^2{Tab}+@a", "^{Enter}", "x²+α")
ClipCase("剪贴板：Ctrl+Shift+Enter", "x^2{Tab}", "^+{Enter}", "__MATHML__")

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
		if (soakFail <= 10)
			rep .= "  浸泡第 " A_Index " 次：框没弹出`n"
		continue
	}
	SendText "a_" Mod(A_Index, 10)
	Send "{Tab}"
	SendText "+@b"
	Send "{Enter}"
	hid := WaitBox(false, 2000)
	Sleep 450
	want := "a_{" Mod(A_Index, 10) "}+\beta"
	got := StrReplace(GEd.Value, "`r", "")
	if (got != want) {
		soakFail++
		if (soakFail <= 10)
			rep .= "  浸泡第 " A_Index " 次：期望 [" want "] 得到 [" got "]" (hid < 0 ? "（框没收起）" : "") "`n"
	}
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
