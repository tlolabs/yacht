[Setup]
AppId=com.local.yacht.csvhtmltranslator
AppName=YACHT
AppVersion={#Version}
AppPublisher=YACHT contributors
AppPublisherURL=https://github.com/tlolabs/yacht
DefaultDirName={localappdata}\Programs\YACHT
DefaultGroupName=YACHT
PrivilegesRequired=lowest
ArchitecturesAllowed={#Architecture}
ArchitecturesInstallIn64BitMode={#Architecture}
MinVersion=10.0.17763
OutputDir={#OutputDirectory}
OutputBaseFilename=YACHT-{#Version}-windows-{#Label}-setup
UninstallDisplayIcon={app}\YachtApp.exe
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
WizardStyle=modern
ChangesAssociations=yes
[Files]
Source: "{#PublishDirectory}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\YACHT"; Filename: "{app}\YachtApp.exe"; AppUserModelID: "com.local.yacht.csvhtmltranslator"
[Registry]
Root: HKCU; Subkey: "Software\Classes\Applications\YachtApp.exe\shell\open\command"; ValueType: string; ValueData: """{app}\YachtApp.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\YachtApp.exe\SupportedTypes"; ValueType: string; ValueName: ".csv"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\YachtApp.exe\SupportedTypes"; ValueType: string; ValueName: ".tsv"; ValueData: ""
[Run]
Filename: "{app}\YachtApp.exe"; Description: "Launch YACHT"; Flags: nowait postinstall skipifsilent
