# LatexAnywhere 公式框：开机自启 + 快捷方式（都直接调用 pythonw.exe，不经过批处理，没有任何黑窗口）
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$pyw  = (Get-Command pythonw.exe -ErrorAction SilentlyContinue).Source   # PATH 里的 Python
if (-not $pyw) { Write-Host "找不到 pythonw.exe：请先安装 Python 3.10+（勾选 Add to PATH），再 pip install -r requirements.txt"; exit 1 }
$app  = Join-Path $here "formula_box.py"
$cmd  = "`"$pyw`" `"$app`""
# 开机自启：计划任务（登录后 15 秒运行 boot.pyw，启动失败自动重试），不用注册表 Run 项（开机时会无声失败）
Remove-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "LatexAnywhere" -ErrorAction SilentlyContinue
$boot = Join-Path $here "boot.pyw"
$act = New-ScheduledTaskAction -Execute $pyw -Argument "`"$boot`"" -WorkingDirectory $here
$trg = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME; $trg.Delay = "PT15S"
$set = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1) -StartWhenAvailable
$pri = New-ScheduledTaskPrincipal -UserId $env:USERNAME -LogonType Interactive -RunLevel Limited
Register-ScheduledTask -TaskName "LatexAnywhere" -Action $act -Trigger $trg -Settings $set -Principal $pri -Force | Out-Null
$ws = New-Object -ComObject WScript.Shell
foreach ($dir in @([Environment]::GetFolderPath('Desktop'), (Join-Path ([Environment]::GetFolderPath('StartMenu')) 'Programs'))) {
    $lnk = $ws.CreateShortcut((Join-Path $dir "LatexAnywhere.lnk"))
    $lnk.TargetPath = $pyw
    $lnk.Arguments = "`"$app`""
    $lnk.WorkingDirectory = $here
    $lnk.Description = "Alt+L 公式框（Latex Suite）"
    $lnk.Save()
}
"已设置开机自启和快捷方式：$cmd"
