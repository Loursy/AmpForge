; AmpForge Windows installer.
;
; Packages the VST3/CLAP/LV2 binaries produced by the mingw-w64 cross-compile
; (see README's "Building for Windows" section) into a real, double-clickable
; Setup.exe - installing each format into the same standard per-machine
; locations a Windows DAW/host already scans, instead of a Windows user
; having to manually copy plugin folders into Program Files themselves.
;
; Build (from the repo root, after the cross-compile build in
; build-windows/ already exists):
;   makensis installer/windows/ampforge.nsi
; Output: build-windows/AmpForge-<version>-Setup.exe

!include "MUI2.nsh"

!define PRODUCT_NAME "AmpForge"
; No CMake project(... VERSION ...) exists yet - bump this by hand
; alongside installer/linux/package.sh's own VERSION and the git tag,
; every time a release goes out. This isn't rebuilt/republished on
; every Linux release (no Windows cross-build CI yet - see the
; README's Project status section), so it can trail a release or two
; behind if only Linux changed.
!define PRODUCT_VERSION "0.2.0"
!define PRODUCT_WEB_SITE "https://github.com/Loursy/AmpForge"
!define BUILD_DIR "..\..\build-windows\bin"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\AmpForge"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\..\build-windows\AmpForge-${PRODUCT_VERSION}-Setup.exe"
; Not a user-chosen install location - each component below has its own
; fixed, standard-scan-path destination (Program Files\Common Files\...).
; This is only where the uninstaller itself and its registry entry live.
InstallDir "$COMMONFILES64\AmpForge"
RequestExecutionLevel admin

!define MUI_ABORTWARNING

Function .onInit
  ; Write to the real (64-bit) registry view, not WOW6432Node - matches
  ; the 64-bit-only mingw-w64 toolchain this project cross-compiles with.
  SetRegView 64
FunctionEnd

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "VST3 Plugin" SecVST3
  SectionIn RO
  SetOutPath "$COMMONFILES64\VST3\ampforge_main.vst3"
  File /r "${BUILD_DIR}\ampforge_main.vst3\*"
SectionEnd

Section "CLAP Plugin" SecCLAP
  SetOutPath "$COMMONFILES64\CLAP"
  File "${BUILD_DIR}\ampforge_main.clap"
SectionEnd

Section "LV2 Plugin" SecLV2
  SetOutPath "$COMMONFILES64\LV2\ampforge_main.lv2"
  File /r "${BUILD_DIR}\ampforge_main.lv2\*"
SectionEnd

Section -Uninstaller
  SetOutPath "$INSTDIR"
  ; DPF and what it bundles (pugl, NanoVG, a fallback font, CLAP/LV2
  ; headers) are compiled directly into the plugin binaries above -
  ; their permissive licenses require this notice to travel with them.
  File "..\..\THIRD-PARTY-NOTICES.md"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "${UNINSTALL_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "Installs the VST3 plugin to Program Files\Common Files\VST3 (Reaper, and most other VST3 hosts)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCLAP} "Installs the CLAP plugin to Program Files\Common Files\CLAP (Reaper, Bitwig, and other CLAP hosts)."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecLV2} "Installs the LV2 plugin to Program Files\Common Files\LV2 (Carla and other LV2 hosts)."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function un.onInit
  SetRegView 64
FunctionEnd

Section "Uninstall"
  RMDir /r "$COMMONFILES64\VST3\ampforge_main.vst3"
  Delete "$COMMONFILES64\CLAP\ampforge_main.clap"
  RMDir /r "$COMMONFILES64\LV2\ampforge_main.lv2"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\THIRD-PARTY-NOTICES.md"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
SectionEnd
