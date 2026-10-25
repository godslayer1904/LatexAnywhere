# 沙盒内：安装程序到本地盘，放入用户的 Latex Suite 设置，启动程序，跑端到端测试，结果写回 C:\Results
$ErrorActionPreference = "Continue"
$R = "C:\Results"
function L($m) { "$(Get-Date -Format HH:mm:ss.fff) $m" | Out-File "$R\boot.log" -Append -Encoding utf8 }
L "开始"
New-Item -ItemType Directory "C:\LA" -Force | Out-Null
Copy-Item C:\LABuild\LatexAnywhere.exe C:\LA\ -Force
New-Item -ItemType Directory "$env:APPDATA\LatexAnywhere" -Force | Out-Null
Copy-Item C:\LSPlugin\data.json "$env:APPDATA\LatexAnywhere\settings.json" -Force
$env:LA_TRACE = "1"
Start-Process C:\LA\LatexAnywhere.exe
$log = "$env:LOCALAPPDATA\LatexAnywhere\log.txt"
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt 30) { if ((Test-Path $log) -and (Select-String -Path $log -Pattern "Alt\+L" -Quiet)) { break }; Start-Sleep -Milliseconds 200 }
L ("程序就绪用时 {0:N1} 秒" -f ((Get-Date) - $t0).TotalSeconds)
$p = Start-Process "C:\AHK\AutoHotkey64.exe" -ArgumentList '/ErrorStdOut', '"C:\E2E\e2e.ahk"' -WorkingDirectory C:\E2E -PassThru -RedirectStandardError "$R\e2e.stderr.txt"
if (-not $p.WaitForExit(900000)) { Stop-Process -Id $p.Id -Force; L "测试超时" }
Copy-Item $log "$R\app.log" -Force -ErrorAction SilentlyContinue
Get-ChildItem "$env:LOCALAPPDATA\LatexAnywhere\crash" -ErrorAction SilentlyContinue | Copy-Item -Destination $R -Force
L "结束"
New-Item "$R\DONE" -ItemType File -Force | Out-Null
