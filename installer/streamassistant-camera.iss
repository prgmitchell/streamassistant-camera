#ifndef SourceDir
  #error SourceDir must identify the installed plugin staging directory.
#endif
#ifndef PackageVersion
  #define PackageVersion "1.2.0"
#endif
#ifndef OutputDir
  #define OutputDir "..\release"
#endif

[Setup]
AppId={{A8D83F2F-4A0D-4A95-87E8-302A4A0D3866}
AppName=StreamAssistant Camera
AppVersion={#PackageVersion}
AppPublisher=StreamAssistant
AppPublisherURL=https://camera.streamassistant.app
AppSupportURL=https://github.com/prgmitchell/streamassistant-camera
DefaultDirName={commonappdata}\obs-studio\plugins\streamassistant-camera
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=streamassistant-camera-v{#PackageVersion}-windows-x64
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=StreamAssistant Camera
VersionInfoVersion=1.2.0.0
VersionInfoCompany=StreamAssistant
VersionInfoDescription=StreamAssistant Camera
VersionInfoProductName=StreamAssistant Camera

[Files]
Source: "{#SourceDir}\bin\64bit\streamassistant-camera.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
WelcomeLabel2=This installs StreamAssistant Camera for the current Windows streaming setup.

[Run]
Filename: "https://camera.streamassistant.app"; Description: "Open the StreamAssistant Camera website"; Flags: postinstall shellexec skipifsilent unchecked
