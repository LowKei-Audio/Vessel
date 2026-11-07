; Vessel Windows installer (Inno Setup 6).
; Built by CI:  iscc /DAppVersion=1.0.0 /DBuildDir=..\build installer\Vessel.iss
; Installs the VST3 and CLAP plug-ins to the standard shared folders (every DAW
; scans them) and, optionally, the standalone app with a Start menu shortcut.
; Matches the rest of the LowKei Audio family.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif
; JUCE puts the plug-in targets under build\src because the plugin is added
; with add_subdirectory(src).
#define Artefacts BuildDir + "\src\Vessel_artefacts\Release"

[Setup]
; AppId identifies this product to Windows for upgrades and uninstall.
; Never change it once shipped, and never reuse it for another plugin.
AppId={{BC13CAA9-CAD9-4FCB-BC67-43C2A958DB14}
AppName=Vessel
AppVersion={#AppVersion}
AppVerName=Vessel {#AppVersion}
AppPublisher=LowKei Audio
DefaultDirName={autopf}\LowKei\Vessel
DefaultGroupName=Vessel
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Admin rights are needed to write to C:\Program Files\Common Files\VST3.
PrivilegesRequired=admin
OutputDir=Output
OutputBaseFilename=Vessel-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Vessel {#AppVersion}
UninstallDisplayIcon={app}\Vessel.exe

[Types]
Name: "full"; Description: "VST3 and CLAP plug-ins and standalone app"
Name: "plugins"; Description: "Plug-ins only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Serato Studio, FL Studio, Ableton Live)"; Types: full plugins custom
Name: "clap"; Description: "CLAP plug-in (FL Studio, Bitwig Studio, Reaper)"; Types: full plugins custom
Name: "standalone"; Description: "Standalone app (no DAW needed)"; Types: full

[Files]
; The VST3 is a bundle folder; copy the whole folder, not just the inner DLL.
Source: "{#Artefacts}\VST3\Vessel.vst3\*"; DestDir: "{commoncf64}\VST3\Vessel.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; CLAP is a single file in the shared CLAP folder.
Source: "{#Artefacts}\CLAP\Vessel.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#Artefacts}\Standalone\Vessel.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\docs\README.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.txt"; DestDir: "{app}"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\Vessel"; Filename: "{app}\Vessel.exe"; Components: standalone
Name: "{group}\Vessel README"; Filename: "{app}\README.pdf"
Name: "{group}\Uninstall Vessel"; Filename: "{uninstallexe}"

[UninstallDelete]
; Remove the plug-in bundle folder itself, which Inno only empties by default.
Type: dirifempty; Name: "{commoncf64}\VST3\Vessel.vst3"
