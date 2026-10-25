# 沙盒内：用 AutoHotkey 依次打开 10 种语言的使用说明窗口（及欢迎模式）并截图
"开始 $(Get-Date -Format HH:mm:ss)" | Out-File C:\Results\help.log -Encoding utf8
$p = Start-Process "C:\AHK\AutoHotkey64.exe" -ArgumentList '/ErrorStdOut', '"C:\E2E\helpshots.ahk"' -WorkingDirectory C:\E2E -PassThru -RedirectStandardError C:\Results\help.stderr.txt
if (-not $p.WaitForExit(240000)) { Stop-Process -Id $p.Id -Force; "截图脚本超时" | Out-File C:\Results\help.log -Append -Encoding utf8 }
"结束 $(Get-Date -Format HH:mm:ss)" | Out-File C:\Results\help.log -Append -Encoding utf8
New-Item C:\Results\DONE -ItemType File -Force | Out-Null
