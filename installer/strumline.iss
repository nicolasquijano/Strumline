; Inno Setup script for the Windows installer of Strumline.
; Build it with package.cmd (which passes the version), or by hand:
;   ISCC /DAppVersion=0.1.0 /DAppVersionFull=0.1.0-beta installer\strumline.iss
; Two components, both selectable on the "Select Components" page (full / VST3 only / standalone only / custom):
;  - the VST3 plug-in: the bundle (with onnxruntime.dll next to the plug-in: the chord converter loads it from there) goes into the VST3 folder of all
;    users (C:\Program Files\Common Files\VST3) or of the current user only;
;  - the standalone application, which needs no DAW, in Program Files with a Start menu entry (it has its own copy of onnxruntime.dll).
; The uninstaller, the licence, the third-party notices and the README live in the application folder, never inside the VST3 folder (a DAW would scan them).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef AppVersionFull
  #define AppVersionFull AppVersion
#endif

[Setup]
; A fixed id: it is how a newer installer recognises (and replaces) an older install. Never change it.
AppId={{B4F1D7A3-6C2E-4E58-A0D9-3F7B81C29E64}
AppName=Strumline
AppVersion={#AppVersionFull}
AppVerName=Strumline {#AppVersionFull}
AppPublisher=Nico
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\Strumline
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
SetupIconFile=..\plugin\icon\icon.ico
OutputDir=..\dist
OutputBaseFilename=Strumline-{#AppVersionFull}-win64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
; The dialog offers "for all users" (C:\Program Files\Common Files\VST3) or "only for me" (%LOCALAPPDATA%\Programs\Common\VST3, no administrator needed).
PrivilegesRequiredOverridesAllowed=commandline dialog
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=Strumline {#AppVersionFull}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Types]
Name: "full"; Description: "Full installation: VST3 plug-in and standalone application"
Name: "vst3"; Description: "VST3 plug-in only (for your DAW)"
Name: "standalone"; Description: "Standalone application only (runs without a DAW)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (rescan the plugins in your DAW to see it)"; Types: full vst3 custom
Name: "standalone"; Description: "Standalone application (runs without a DAW) and its Start menu entry"; Types: full standalone custom

[Files]
; The whole bundle: Contents\x86_64-win\Strumline.vst3 (the DLL), onnxruntime.dll and its helper, and Contents\Resources\moduleinfo.json.
Source: "..\build\plugin\Strumline_artefacts\Release\VST3\Strumline.vst3\*"; DestDir: "{autocf64}\VST3\Strumline.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: vst3
Source: "..\build\plugin\Strumline_artefacts\Release\Standalone\Strumline.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "..\build\plugin\Strumline_artefacts\Release\Standalone\onnxruntime.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "..\build\plugin\Strumline_artefacts\Release\Standalone\onnxruntime_providers_shared.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.es.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Strumline"; Filename: "{app}\Strumline.exe"; Components: standalone

[Messages]
FinishedLabel=Strumline was installed. If you chose the VST3 plug-in, rescan the plugins in your DAW to see it; the standalone application, if you chose it, is in the Start menu.
