; Mirra — Inno Setup Installer Script
; Per-user install, no elevation required.
; Usage: iscc /DMyAppVersion=1.0.0 mirra_setup.iss

#define MyAppName      "Mirra"
#define MyAppPublisher "Mirra"
#define MyAppURL       "https://github.com/your-org/mirra"
#define MyAppExeName   "Mirra.exe"
#ifdef MyAppVersion
  ; passed via command line: /DMyAppVersion=1.0.0
#else
  #define MyAppVersion "1.0.0"
#endif

[Setup]
AppId={{F7A2B1C3-D4E5-4F60-8A1B-C2D3E4F56789}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases

; Per-user install — no elevation
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline

DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=mirra_setup_{#MyAppVersion}

; Code signing (enable when certificate is available)
SignTool=signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /a $f

Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Minimum Windows 10 20H1
MinVersion=10.0.19041

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Main application files (from publish/ directory at build time)
Source: "..\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Clean up user data on uninstall (only app data, not user recordings/screenshots)
Type: filesandordirs; Name: "{userappdata}\Mirra\logs"
Type: filesandordirs; Name: "{userappdata}\Mirra\preferences"

[Code]
// Verify .NET 10 is installed (or guide user to install it)
function IsNetInstalled: Boolean;
var
  FileName: string;
begin
  FileName := ExpandConstant('{sys}\dotnet.exe');
  Result := FileExists(FileName);
end;

procedure InitializeWizard;
begin
  // If using self-contained publish, .NET check is not needed.
  // Uncomment below if using framework-dependent publish:
  // if not IsNetInstalled then
  //   MsgBox('This application requires .NET 10. Please install it from https://dot.net', mbError, MB_OK);
end;
