; LatexAnywhere 安装程序：一个安装包同时含 x64 与 ARM64，按本机架构安装；装到用户目录，不需要管理员
#define AppVersion GetEnv("LA_VERSION")
#if AppVersion == ""
  #define AppVersion "0.9.0"
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

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "ar"; MessagesFile: "compiler:Languages\Arabic.isl"

[Tasks]
Name: "autostart"; Description: "Start with Windows"; Flags: checkedonce

[Files]
Source: "..\dist\arm64\LatexAnywhere.exe"; DestDir: "{app}"; Check: IsArm64; Flags: ignoreversion
Source: "..\dist\x64\LatexAnywhere.exe"; DestDir: "{app}"; Check: not IsArm64; Flags: ignoreversion

[Icons]
Name: "{userprograms}\LatexAnywhere"; Filename: "{app}\LatexAnywhere.exe"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "LatexAnywhere"; ValueData: """{app}\LatexAnywhere.exe"""; Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\LatexAnywhere.exe"; Flags: nowait postinstall skipifsilent; Description: "Start LatexAnywhere"
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
