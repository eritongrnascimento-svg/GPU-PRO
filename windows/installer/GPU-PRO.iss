; Instalador do GPU-PRO para Windows (Inno Setup 6).
; Gerado por windows/build-windows.ps1 a partir da pasta portátil em dist/.
; ISCC /DAppVersion=0.8.0 /DSourceDir=<pasta portátil> /DOutputDir=<dist> GPU-PRO.iss

#ifndef AppVersion
  #define AppVersion "0.8.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\GPU-PRO-" + AppVersion + "-windows-x64"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif

[Setup]
; AppId fixo: atualizações futuras substituem esta instalação em vez de criar outra.
AppId={{F107AD2E-F85D-4BFC-ACFB-AD58E49E9FE5}
AppName=GPU-PRO
AppVersion={#AppVersion}
AppVerName=GPU-PRO {#AppVersion}
AppPublisher=Ériton do Nascimento
AppPublisherURL=https://github.com/eritongrnascimento-svg/GPU-PRO
AppSupportURL=https://github.com/eritongrnascimento-svg/GPU-PRO/issues
AppUpdatesURL=https://github.com/eritongrnascimento-svg/GPU-PRO/releases
DefaultDirName={autopf}\GPU-PRO
DefaultGroupName=GPU-PRO
DisableProgramGroupPage=yes
; Instala para o usuário atual sem administrador; a tela inicial permite instalar para todos.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
LicenseFile=..\..\LICENSE
SetupIconFile=..\GpuPro.WinUI\Assets\gpu-pro.ico
UninstallDisplayIcon={app}\GPU-PRO.exe
UninstallDisplayName=GPU-PRO {#AppVersion}
OutputDir={#OutputDir}
OutputBaseFilename=GPU-PRO-{#AppVersion}-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoProductName=GPU-PRO

[Languages]
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
; O Windows só libera a leitura dos quadros dos jogos para administradores: o contador do GPU-PRO
; roda como serviço, instalado uma vez em Arquivos de Programas (pede confirmação do Windows).
Name: "fpsservice"; Description: "Ativar FPS automático dos jogos (instala o serviço GPU-PRO · Contador de quadros; pede administrador)"; GroupDescription: "FPS dos jogos:"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\GPU-PRO"; Filename: "{app}\GPU-PRO.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\GPU-PRO · Leia-me"; Filename: "{app}\LEIA-ME-WINDOWS.md"
Name: "{autodesktop}\GPU-PRO"; Filename: "{app}\GPU-PRO.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\gpupro-frames.exe"; Parameters: "--install"; Flags: shellexec runhidden waituntilterminated; Verb: "runas"; StatusMsg: "Ativando o FPS automático..."; Tasks: fpsservice
Filename: "{app}\GPU-PRO.exe"; Description: "{cm:LaunchProgram,GPU-PRO}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\gpupro-frames.exe"; Parameters: "--uninstall"; Flags: shellexec runhidden waituntilterminated; Verb: "runas"; RunOnceId: "GPUPROFrames"; Check: FrameServiceInstalled

[Code]
function FrameServiceInstalled: Boolean;
begin
  Result := RegKeyExists(HKLM64, 'SYSTEM\CurrentControlSet\Services\GPUPROFrames');
end;

; Configurações, histórico e logs em %LOCALAPPDATA%\GPU-PRO são do usuário e permanecem após desinstalar.
