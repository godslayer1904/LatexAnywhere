; 真机输入法测试：在自己开的窗口里用真实按键（会经过输入法，不是 Unicode 注入），检查进框自动切英文、离开后切回中文。
; 只操作本脚本自己的窗口。结果写到 %LA_E2E_RESULTS%\ime.txt
#Requires AutoHotkey v2.0
#SingleInstance Force
SetKeyDelay 30, 20
RES := EnvGet("LA_E2E_RESULTS") || A_ScriptDir
OUT := RES "\ime.txt"
try FileDelete OUT
Log(s) => FileAppend(s "`n", OUT, "UTF-8")

title := "LA IME test " Random(100000, 999999)
G := Gui("+AlwaysOnTop", title)
G.SetFont("s14")
E := G.AddEdit("w500 h80")
G.Show()
WinWaitActive(title, , 5)
E.Focus()
Val() => StrReplace(E.Value, "`r", "")
Focused() => WinActive(title)

; 1. 让输入法进入中文状态：打 ni 空格应得到「你」
Send "{vk16}"   ; VK_IME_ON
Sleep 300
Send "ni{Space}"
Sleep 600
if (Val() != "你") {
	E.Value := ""
	Send "{LShift}"   ; 微软拼音：Shift 切中英
	Sleep 300
	Send "ni{Space}"
	Sleep 600
}
if (Val() != "你") {
	Log("✗ 测试环境：没能进入中文输入状态，得到 [" Val() "]")
	ExitApp
}
Log("✓ 测试环境：中文输入状态（ni 空格 → 你）")

; 2. Alt+L，真实按键打 x^2，Tab，Enter
Send "!l"
Sleep 1500
Send "x+62"      ; x ^ 2（Shift+6 = ^）
Sleep 300
Send "{Tab}"
Sleep 300
Send "{Enter}"
Sleep 1500
if !Focused() {
	WinActivate title
	Sleep 300
}
got := Val()
Log((got == "你x^{2}" ? "✓" : "✗") " 中文输入法开着时进框自动切英文、上屏  期望 [你x^{2}] 得到 [" got "]")

; 3. 回到原窗口后仍是中文
Send "ni{Space}"
Sleep 600
got2 := Val()
Log((got2 == "你x^{2}你" ? "✓" : "✗") " 回原输入框后仍是中文  期望 [你x^{2}你] 得到 [" got2 "]")
G.Destroy()
ExitApp
