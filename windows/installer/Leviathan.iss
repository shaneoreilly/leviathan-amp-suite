; Leviathan for Windows installer (Inno Setup 6).
;   iscc /DAppVersion=0.3.0 /DBuildDir=..\build\Leviathan_artefacts\Release /DOutDir=..\dist Leviathan.iss
;
; Installs per user (no admin prompt), so the in-app updater can run it silently:
;   Leviathan-Setup-x.y.z.exe /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS
; closes the running app, installs over it, and reopens it (the [Run] entry also runs in silent mode).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\Leviathan_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\dist"
#endif

[Setup]
AppId={{6B1E7C1A-3F3B-4C47-9E2D-1E5C2A7D9B40}
AppName=Leviathan
AppVersion={#AppVersion}
AppVerName=Leviathan {#AppVersion}
AppPublisher=Leviathan
AppPublisherURL=https://github.com/shaneoreilly/leviathan-amp-suite
AppSupportURL=https://github.com/shaneoreilly/leviathan-amp-suite/issues
AppUpdatesURL=https://github.com/shaneoreilly/leviathan-amp-suite/releases
DefaultDirName={autopf}\Leviathan
DefaultGroupName=Leviathan
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutDir}
OutputBaseFilename=Leviathan-Setup-{#AppVersion}
SetupIconFile=Leviathan.ico
UninstallDisplayIcon={app}\Leviathan.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes
#if FileExists(AddBackslash(SourcePath) + "..\..\LICENSE")
LicenseFile=..\..\LICENSE
#endif

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#BuildDir}\Leviathan.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD-PARTY.md"; DestDir: "{app}"; Flags: ignoreversion
#if FileExists(AddBackslash(SourcePath) + "..\..\LICENSE")
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
#endif

[Icons]
Name: "{autoprograms}\Leviathan"; Filename: "{app}\Leviathan.exe"
Name: "{autodesktop}\Leviathan"; Filename: "{app}\Leviathan.exe"; Tasks: desktopicon

[Registry]
; .leviathan preset files open in Leviathan (imported into your presets)
Root: HKA; Subkey: "Software\Classes\.leviathan"; ValueType: string; ValueName: ""; ValueData: "Leviathan.Preset"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Leviathan.Preset"; ValueType: string; ValueName: ""; ValueData: "Leviathan Preset"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Leviathan.Preset\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\Leviathan.exe,0"
Root: HKA; Subkey: "Software\Classes\Leviathan.Preset\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Leviathan.exe"" ""%1"""

[Run]
Filename: "{app}\Leviathan.exe"; Description: "{cm:LaunchProgram,Leviathan}"; Flags: nowait postinstall
