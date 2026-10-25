; LatexAnywhere 安装程序：一个安装包同时含 x64 与 ARM64，按本机架构安装；装到用户目录，不需要管理员
#define AppVersion GetEnv("LA_VERSION")
#if AppVersion == ""
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6C1E2A57-6B5E-4E57-9D7B-3F1B8A6D2C41}
AppName=LatexAnywhere
AppVersion={#AppVersion}
AppPublisher=LatexAnywhere
AppPublisherURL=https://github.com/godslayer1904/LatexAnywhere
DefaultDirName={localappdata}\Programs\LatexAnywhere
DefaultGroupName=LatexAnywhere
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible arm64
ArchitecturesInstallIn64BitMode=x64compatible arm64
OutputDir=..\dist
OutputBaseFilename=LatexAnywhere-Setup-{#AppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\win\res\app.ico
UninstallDisplayIcon={app}\LatexAnywhere.exe
CloseApplications=no

; 与程序界面相同的 10 种语言，按系统界面语言自动选择。中文、越南语的翻译文件随项目自带（Languages\，取自 Inno Setup 源码库）
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "zhcn"; MessagesFile: "Languages\ChineseSimplified.isl"
Name: "zhtw"; MessagesFile: "Languages\ChineseTraditional.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "vi"; MessagesFile: "Languages\Vietnamese.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "ar"; MessagesFile: "compiler:Languages\Arabic.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[CustomMessages]
en.AutoStart=Start with Windows
zhcn.AutoStart=开机自启
zhtw.AutoStart=開機自動啟動
ja.AutoStart=Windows の起動時に開始
ko.AutoStart=Windows 시작 시 실행
vi.AutoStart=Khởi động cùng Windows
ru.AutoStart=Запускать вместе с Windows
ar.AutoStart=التشغيل مع بدء Windows
es.AutoStart=Iniciar con Windows
pt.AutoStart=Iniciar com o Windows
en.Launch=Start LatexAnywhere
zhcn.Launch=启动 LatexAnywhere
zhtw.Launch=啟動 LatexAnywhere
ja.Launch=LatexAnywhere を起動
ko.Launch=LatexAnywhere 실행
vi.Launch=Khởi động LatexAnywhere
ru.Launch=Запустить LatexAnywhere
ar.Launch=تشغيل LatexAnywhere
es.Launch=Iniciar LatexAnywhere
pt.Launch=Iniciar LatexAnywhere

[Tasks]
Name: "autostart"; Description: "{cm:AutoStart}"; Flags: checkedonce

[Files]
Source: "..\dist\arm64\LatexAnywhere.exe"; DestDir: "{app}"; Check: IsArm64; Flags: ignoreversion
Source: "..\dist\x64\LatexAnywhere.exe"; DestDir: "{app}"; Check: not IsArm64; Flags: ignoreversion

[Icons]
Name: "{userprograms}\LatexAnywhere"; Filename: "{app}\LatexAnywhere.exe"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "LatexAnywhere"; ValueData: """{app}\LatexAnywhere.exe"""; Tasks: autostart; Flags: uninsdeletevalue
; 程序自己的设置（欢迎窗口「不再显示」、界面语言）：卸载时一并删除
Root: HKCU; Subkey: "Software\LatexAnywhere"; Flags: uninsdeletekeyifempty uninsdeletekey

[Run]
Filename: "{app}\LatexAnywhere.exe"; Flags: nowait postinstall skipifsilent; Description: "{cm:Launch}"
Filename: "{app}\LatexAnywhere.exe"; Flags: nowait skipifnotsilent

[UninstallRun]
Filename: "{app}\LatexAnywhere.exe"; Parameters: "--quit"; Flags: waituntilterminated; RunOnceId: "QuitApp"

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\LatexAnywhere"

[Code]
// 安装或升级前先结束正在运行的实例，避免文件被占用
procedure QuitRunning();
var Code: Integer;
begin
  if FileExists(ExpandConstant('{app}\LatexAnywhere.exe')) then
    Exec(ExpandConstant('{app}\LatexAnywhere.exe'), '--quit', '', SW_HIDE, ewWaitUntilTerminated, Code);
  Sleep(800);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  QuitRunning();
  Result := '';
end;
