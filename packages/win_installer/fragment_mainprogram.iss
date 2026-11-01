[Types]
Name: "basic"; Description: "Standard installation"
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "main"; Description: "Main Files"; Types: full compact custom basic; Flags: fixed
Name: "macros"; Description: "Automation Scripts"; Types: full basic
Name: "macros\bundled"; Description: "Bundled macros"; Types: full basic
Name: "macros\demos"; Description: "Example macros/Demos"; Types: full basic
#ifdef DEPCTRL
Name: "macros\modules"; Description: "Modules"; Types: full
Name: "macros\modules\depctrl"; Description: "DependencyControl"; Types: full
#endif
Name: "dictionaries"; Description: "Spellcheck Dictionaries"; Types: full basic
Name: "dictionaries\en_US"; Description: "English (US)"; Types: full basic
Name: "translations"; Description: "Aegisub Translations"; Types: full basic

[Tasks]
Name: "startmenuicon"; Description: "{cm:StartMenuIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "checkforupdates"; Description: "{cm:CheckForUpdates}"; GroupDescription: "{cm:UpdatesGroup}"

[Files]
; main
DestDir: {app}; Source: "{#BUILD_ROOT}\danketsub.exe"; Flags: ignoreversion; Components: main
DestDir: {app}; Source: "{#INSTALLER_DIR}\license.txt"; Flags: ignoreversion; Components: main
DestDir: {app}; Source: "{#SOURCE_ROOT}\LICENCE"; Flags: ignoreversion; Components: main
DestDir: {app}; Source: "{#SOURCE_ROOT}\COPYING"; Flags: ignoreversion; Components: main
DestDir: {app}; Source: "{#SOURCE_ROOT}\THIRD-PARTY-NOTICES.txt"; Flags: ignoreversion; Components: main
DestDir: {app}; Source: "{#SOURCE_ROOT}\SOURCE.txt"; Flags: ignoreversion; Components: main

[Icons]
Name: {commonprograms}\DanketSub; Filename: {app}\danketsub.exe; WorkingDir: {app}; IconIndex: 0; Tasks: startmenuicon; Comment: Create and edit subtitle files

[Registry]
; Register in App Paths so the user can conveniently enter 'danketsub' in their Run box
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\danketsub.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\danketsub.exe"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\danketsub.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\danketsub.exe"; Flags: uninsdeletekey

[Run]
Filename: {app}\danketsub.exe; Description: {cm:LaunchProgram,DanketSub}; Flags: nowait postinstall skipifsilent

[InstallDelete]
Type: files; Name: "{app}\ffms2_64.dll"
Type: files; Name: "{app}\ffms2_32.dll"
