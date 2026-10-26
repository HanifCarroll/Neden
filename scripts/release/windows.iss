; Build with /DStaging=... /DOutput=... using the runner's pinned Inno Setup.
#ifndef Staging
  #error Staging is required
#endif
#ifndef Output
  #error Output is required
#endif
[Setup]
AppId={{6D1E6B8B-8427-46C6-B4E5-8D4AF9558E22}
AppName=Neden
AppVersion=0.1.0-preview
AppPublisher=Neden contributors
AppPublisherURL=https://neden.hanifcarroll.com
DefaultDirName={localappdata}\Programs\Neden
DefaultGroupName=Neden
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Output}
OutputBaseFilename=Neden-windows-x86_64-preview-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\bin\Neden.exe
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#Staging}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\Neden"; Filename: "{app}\bin\Neden.exe"
[Run]
Filename: "{app}\bin\Neden.exe"; Description: "Open Neden"; Flags: nowait postinstall skipifsilent unchecked
