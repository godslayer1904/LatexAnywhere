# 本机启动器：用 wsb 命令行正规地启动沙盒 → 连上（窗口最小化）→ 等测试完成 → wsb stop 关闭，打印结果
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$res = Join-Path $here "results"
$wsb = "$env:LOCALAPPDATA\Microsoft\WindowsApps\wsb.exe"
foreach ($id in (& $wsb list --raw | ConvertFrom-Json).WindowsSandboxEnvironments) { & $wsb stop --id $id | Out-Null }   # 清掉上一轮没关的
Get-ChildItem $res -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force
New-Item -ItemType Directory $res -Force | Out-Null
$cfg = Get-Content "$here\e2e.wsb" -Raw
$started = & $wsb start --config $cfg --raw | ConvertFrom-Json
$id = $started.Id
if (-not $id) { "沙盒启动失败：$started"; exit 1 }
Add-Type -Name W -Namespace U -MemberDefinition '[DllImport("user32.dll")] public static extern bool ShowWindow(System.IntPtr h, int c);'
Start-Process $wsb -ArgumentList "connect", "--id", $id -WindowStyle Minimized
$t0 = Get-Date
$minimized = $false
while (-not (Test-Path "$res\DONE") -and ((Get-Date) - $t0).TotalMinutes -lt 8) {
    if (-not $minimized) {
        $c = Get-Process WindowsSandboxRemoteSession, WindowsSandboxClient -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 }
        foreach ($p in $c) { [U.W]::ShowWindow($p.MainWindowHandle, 6) | Out-Null; $minimized = $true }   # 6 = 最小化
    }
    Start-Sleep 2
}
& $wsb stop --id $id | Out-Null
"耗时 {0:N0} 秒" -f ((Get-Date) - $t0).TotalSeconds
foreach ($f in "boot.log", "e2e.stderr.txt", "e2e.out.txt", "app.log") { if (Test-Path "$res\$f") { "== $f"; Get-Content "$res\$f" -Encoding UTF8 } }
