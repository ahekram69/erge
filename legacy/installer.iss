#define AppVersion "0.9.5"
#define AppExe "USB-Camera-Control-Legacy-x86-v" + AppVersion + ".exe"
[Setup]
AppId={{F0DF96A8-48E1-4C91-98CE-37D9D3A60377}
AppName=USB Camera Control Legacy (32-bit Test)
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\USB Camera Control Legacy
PrivilegesRequired=lowest
MinVersion=6.1sp1
OutputDir=..\legacy-output
OutputBaseFilename=USB-Camera-Control-Legacy-Setup-v{#AppVersion}-Win7-x86-TEST
SetupIconFile=..\assets\app-icon.ico
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
[Files]
Source: "..\legacy-package\*"; DestDir: "{app}"; Excludes: "VC_redist.x86.exe,legacy-encoder-test.mp4,decoded.rgb"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\legacy-package\VC_redist.x86.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
[Icons]
Name: "{group}\USB Camera Control Legacy"; Filename: "{app}\{#AppExe}"
[Run]
Filename: "{tmp}\VC_redist.x86.exe"; Parameters: "/install /passive /norestart"; Flags: waituntilterminated
