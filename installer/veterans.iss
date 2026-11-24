; Inno Setup 6: установщик «Ветеранов» (M11-02).
;
; Собирать после portable-сборки (tools\package-portable.ps1):
;   iscc installer\veterans.iss
;
; Данные пользователя (data, backups, settings.ini) живут в
; %LOCALAPPDATA%\Veterans, потому что каталог Program Files недоступен
; для записи. Поэтому обновление и деинсталляция не трогают базу.

#define AppName "Ветераны"
#define AppExe "Veterans.exe"
#define AppVersion "0.1.0"

[Setup]
AppId={{7E3C1B5A-2F4D-4E8A-9C61-5B2A0D7F3E11}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Veterans
DefaultDirName={autopf}\Veterans
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExe}
OutputDir=..\dist
OutputBaseFilename=Veterans-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
DisableDirPage=no
DisableProgramGroupPage=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительно:"

[Files]
Source: "..\dist\Veterans\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Удалить {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "Запустить {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Удаляем только то, что создаёт сама программа в своём каталоге.
; Пользовательская база остаётся в %LOCALAPPDATA%\Veterans.
Type: filesandordirs; Name: "{app}\logs"
Type: filesandordirs; Name: "{app}\data"
Type: filesandordirs; Name: "{app}\backups"

[Code]
// При установке в Program Files приложение работает без прав администратора,
// а данные уходят в профиль пользователя. Обновление поверх установки
// сохраняет базу: каталог %LOCALAPPDATA%\Veterans не входит в [Files].
