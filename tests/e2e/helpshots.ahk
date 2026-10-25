#Requires AutoHotkey v2.0
#Warn All, StdOut
#NoTrayIcon
; 依次用 10 种语言打开原生使用说明窗口并截图；最后截两张欢迎模式（中文、阿拉伯语）
LOGF := "C:\Results\help.log"
OnError((e, m) => (FileAppend("运行错误：" e.Message " 行 " e.Line "`n", LOGF, "UTF-8"), ExitApp(1)))
Shot(name) {
	h := WinExist("ahk_class LatexAnywhereHelp")
	WinGetPos &x, &y, &w, &hh, h
	f := "C:\Results\help_" name ".png"
	ps := "Add-Type -AssemblyName System.Drawing; $b = New-Object Drawing.Bitmap " w "," hh "; $g = [Drawing.Graphics]::FromImage($b); $g.CopyFromScreen(" x "," y ",0,0,$b.Size); $b.Save('" f "')"
	RunWait 'powershell -NoProfile -Command "' ps '"', , "Hide"
}
Open(lang, extra, name) {
	EnvSet "LA_LANG", lang
	Run '"C:\LABuild\LatexAnywhere.exe" --help ' extra, , , &pid
	if !WinWait("ahk_class LatexAnywhereHelp", , 10) {
		FileAppend(name " 窗口没出现`n", LOGF, "UTF-8")
		return
	}
	Sleep 1200
	Shot(name)
	FileAppend(name " 截图完成`n", LOGF, "UTF-8")
	WinClose "ahk_class LatexAnywhereHelp"
	WinWaitClose "ahk_class LatexAnywhereHelp", , 5
	ProcessClose pid
	Sleep 400
}
for lang in ["zh-CN", "zh-TW", "ja", "en", "ko", "vi", "ru", "ar", "es", "pt-BR"]
	Open(lang, "--welcome", lang)
ExitApp 0
