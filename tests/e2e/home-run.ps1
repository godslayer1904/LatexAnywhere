# 真机验收（家里 x64）：停掉 Python 原型 → 以首次安装状态静默安装 C++ 版 → 在用户桌面上跑端到端测试 → 恢复日常使用。
# 用法（ssh 进去后）：powershell -ExecutionPolicy Bypass -File C:\Users\Administrator\la-test\home-run.ps1
# 程序和测试都要在用户的桌面会话里跑：经「交互式」计划任务启动，ssh 会话本身看不到桌面。
$ErrorActionPreference = "Continue"
$D = "C:\Users\Administrator\la-test"
$R = "$D\results"
Remove-Item $R -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $R -Force | Out-Null
function L($m) { "$(Get-Date -Format HH:mm:ss) $m" | Tee-Object -FilePath "$R\home.log" -Append }

function RunInDesktop($name, $exe, $arg) {
	# 在当前登录用户的桌面会话里启动（Interactive 登录类型），立即返回
	Unregister-ScheduledTask -TaskName $name -Confirm:$false -ErrorAction SilentlyContinue
	$a = if ($arg) { New-ScheduledTaskAction -Execute $exe -Argument $arg } else { New-ScheduledTaskAction -Execute $exe }   # 参数不能是空串
	$p = New-ScheduledTaskPrincipal -UserId "Administrator" -LogonType Interactive -RunLevel Limited
	Register-ScheduledTask -TaskName $name -Action $a -Principal $p -Force | Out-Null
	Start-ScheduledTask -TaskName $name
}

# 1. 停掉 Python 原型（只结束 LatexAnywhere 的 pythonw，别的 Python 程序不动），停用它的开机任务
foreach ($p in Get-CimInstance Win32_Process | Where-Object { $_.Name -eq "pythonw.exe" }) {
	if ($p.CommandLine -match "LatexAnywhere|formula_box|boot\.pyw") { L "结束 Python 原型 $($p.ProcessId)：$($p.CommandLine)"; Stop-Process -Id $p.ProcessId -Force }
	else { L "保留其他 Python 程序 $($p.ProcessId)：$($p.CommandLine)" }
}
Disable-ScheduledTask -TaskName "LatexAnywhere" -ErrorAction SilentlyContinue | Out-Null
# 上一轮测试留下的 Edge（只认测试专用的配置目录，用户自己的 Edge 不动）
Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -match "LAEdgeProfile" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
L ("Python 原型开机任务：" + (Get-ScheduledTask -TaskName "LatexAnywhere" -ErrorAction SilentlyContinue).State)

# 2. 首次安装状态：清掉 C++ 版的设置（欢迎窗口、语言）
$exe = "$env:LOCALAPPDATA\Programs\LatexAnywhere\LatexAnywhere.exe"
if (Test-Path $exe) { & $exe --quit; Start-Sleep 2 }
Remove-Item HKCU:\Software\LatexAnywhere -Recurse -Force -ErrorAction SilentlyContinue
$obs = "E:\obsidian\obsidian\.obsidian\plugins\obsidian-latex-suite\data.json"
L ("Obsidian 的 Latex Suite 设置：" + (Test-Path $obs))

# 3. 静默安装（带安装日志）
$setup = Get-ChildItem "$D\LatexAnywhere-Setup-*.exe" | Select-Object -First 1
L "安装 $($setup.Name)"
RunInDesktop "LA_Install" $setup.FullName "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG=`"$R\install.log`""
$t = 0
while ($t -lt 120 -and -not (Get-Process LatexAnywhere -ErrorAction SilentlyContinue)) { Start-Sleep 1; $t++ }
Start-Sleep 3
L ("安装后：程序在运行 " + [bool](Get-Process LatexAnywhere -ErrorAction SilentlyContinue) + "；进程会话 " + ((Get-Process LatexAnywhere -ErrorAction SilentlyContinue).SessionId -join ","))
L ("开机自启项：" + (Get-ItemProperty HKCU:\Software\Microsoft\Windows\CurrentVersion\Run -ErrorAction SilentlyContinue).LatexAnywhere)
Select-String -Path "$R\install.log" -Pattern "Run|Registry|LatexAnywhere\.exe|Exit code|Installation process succeeded|error" -ErrorAction SilentlyContinue |
	Select-Object -Last 12 | ForEach-Object { L ("  安装日志：" + $_.Line.Trim()) }

# 4. 端到端测试（真实桌面）
$cmd = "$D\run-e2e.cmd"
@"
@echo off
set LA_E2E_RESULTS=$R
"C:\Users\Administrator\AutoHotkey64.exe" /ErrorStdOut "$D\e2e\e2e.ahk" 2> "$R\e2e.stderr.txt"
echo done> "$R\DONE"
"@ | Out-File $cmd -Encoding oem
L "开始端到端测试"
RunInDesktop "LA_E2E" "cmd.exe" "/c `"$cmd`""
$t = 0
while ($t -lt 1500 -and -not (Test-Path "$R\DONE")) { Start-Sleep 2; $t += 2 }
if (-not (Test-Path "$R\DONE")) { L "测试 25 分钟没结束，强制结束"; Stop-ScheduledTask -TaskName LA_E2E; Get-Process AutoHotkey* -ErrorAction SilentlyContinue | Stop-Process -Force }
L "测试结束"

# 5. 恢复日常使用：测试最后会紧急强杀程序，重新启动它；清掉临时任务
if (-not (Get-Process LatexAnywhere -ErrorAction SilentlyContinue)) { RunInDesktop "LA_Start" $exe ""; Start-Sleep 4 }
L ("恢复后：程序在运行 " + [bool](Get-Process LatexAnywhere -ErrorAction SilentlyContinue))
foreach ($n in "LA_Install", "LA_E2E", "LA_Start") { Unregister-ScheduledTask -TaskName $n -Confirm:$false -ErrorAction SilentlyContinue }
Copy-Item "$env:LOCALAPPDATA\LatexAnywhere\log.txt" "$R\app.log" -ErrorAction SilentlyContinue
