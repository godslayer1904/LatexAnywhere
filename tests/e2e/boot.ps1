# 沙盒内：放入用户的 Latex Suite 设置；有安装包就静默安装（否则直接拷 exe），跑端到端测试，最后测卸载。结果写回 C:\Results
$ErrorActionPreference = "Continue"
$R = "C:\Results"
function L($m) { "$(Get-Date -Format HH:mm:ss.fff) $m" | Out-File "$R\boot.log" -Append -Encoding utf8 }
L "开始"
New-Item -ItemType Directory "$env:APPDATA\LatexAnywhere" -Force | Out-Null
Copy-Item C:\LSPlugin\data.json "$env:APPDATA\LatexAnywhere\settings.json" -Force
$env:LA_TRACE = "1"
$runKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$setup = Get-ChildItem C:\Installer\LatexAnywhere-Setup-*.exe -ErrorAction SilentlyContinue | Select-Object -First 1
$exe = "$env:LOCALAPPDATA\Programs\LatexAnywhere\LatexAnywhere.exe"
if ($setup) {
    L "安装包模式：$($setup.Name)"
    $p = Start-Process $setup.FullName -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/TASKS=autostart" -PassThru
    $p.WaitForExit(120000) | Out-Null
    L ("安装退出码 $($p.ExitCode)；已安装：" + (Test-Path $exe) + "；开机自启：" + [bool](Get-ItemProperty $runKey -Name LatexAnywhere -ErrorAction SilentlyContinue))
} else {
    New-Item -ItemType Directory "C:\LA" -Force | Out-Null
    Copy-Item C:\LABuild\LatexAnywhere.exe C:\LA\ -Force
    Start-Process C:\LA\LatexAnywhere.exe
}
$log = "$env:LOCALAPPDATA\LatexAnywhere\log.txt"
$t0 = Get-Date
while (((Get-Date) - $t0).TotalSeconds -lt 30) { if ((Test-Path $log) -and (Select-String -Path $log -Pattern "Alt\+L" -Quiet)) { break }; Start-Sleep -Milliseconds 200 }
L ("程序就绪用时 {0:N1} 秒" -f ((Get-Date) - $t0).TotalSeconds)
$p = Start-Process "C:\AHK\AutoHotkey64.exe" -ArgumentList '/ErrorStdOut', '"C:\E2E\e2e.ahk"' -WorkingDirectory C:\E2E -PassThru -RedirectStandardError "$R\e2e.stderr.txt"
# 每次只等 1 秒、数满 900 次才算超时：沙盒刚启动时系统时钟会和主机对时而跳变，一次等 15 分钟会被提前唤醒误判超时（实测）
$n = 0
while (-not $p.WaitForExit(1000) -and ++$n -lt 900) { }
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; L "测试超时" }
Copy-Item $log "$R\app.log" -Force -ErrorAction SilentlyContinue
Get-ChildItem "$env:LOCALAPPDATA\LatexAnywhere\crash" -ErrorAction SilentlyContinue | Copy-Item -Destination $R -Force
if ($setup) {
    Start-Process $exe   # 测试最后强杀过：先启动，验证卸载会结束正在运行的程序
    Start-Sleep 3
    $p = Start-Process "$env:LOCALAPPDATA\Programs\LatexAnywhere\unins000.exe" -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -PassThru
    $p.WaitForExit(120000) | Out-Null
    Start-Sleep 3
    $left = @(Get-Process LatexAnywhere -ErrorAction SilentlyContinue).Count
    L ("卸载退出码 $($p.ExitCode)；残留进程 $left；程序还在：" + (Test-Path $exe) + "；开机自启项还在：" + [bool](Get-ItemProperty $runKey -Name LatexAnywhere -ErrorAction SilentlyContinue) + "；数据目录还在：" + (Test-Path "$env:LOCALAPPDATA\LatexAnywhere") + "；设置注册表还在：" + (Test-Path "HKCU:\Software\LatexAnywhere"))
}
L "结束"
New-Item "$R\DONE" -ItemType File -Force | Out-Null
