#!/usr/bin/env bash
# AmpForge macOS installer.
#
# Copies the prebuilt ampforge_main.vst3 / ampforge_main.clap /
# ampforge_main.lv2 / ampforge_main into the standard per-user plugin
# directories macOS hosts scan by default, mirroring
# installer/linux/install.sh's role for Linux. Works from two layouts:
# inside an extracted release zip (a bin/ dir sits right next to this
# script), or run directly from the repo root after building from source
# (falls back to build-macos/bin/ - or BUILD_DIR/bin/ if BUILD_DIR is set,
# the same override installer/macos/build.sh takes).
#
# No sudo needed - ~/Library/Audio/Plug-Ins/... is scanned by every major
# macOS host (Reaper, Bitwig, Logic, Ableton, ...) without any system-wide
# install step.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -d "$SCRIPT_DIR/bin" ]]; then
    BIN_DIR="$SCRIPT_DIR/bin"                # extracted release zip
else
    BIN_DIR="$SCRIPT_DIR/../../${BUILD_DIR:-build-macos}/bin"  # repo root, built from source
fi

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
LV2_DIR="$HOME/Library/Audio/Plug-Ins/LV2"

if [[ "${1:-}" == "--uninstall" ]]; then
    rm -rf "$VST3_DIR/ampforge_main.vst3" "$CLAP_DIR/ampforge_main.clap" \
           "$LV2_DIR/ampforge_main.lv2" "$HOME/.local/bin/ampforge_main"
    echo "AmpForge removed."
    exit 0
fi

if [[ ! -d "$BIN_DIR" ]]; then
    echo "error: $BIN_DIR not found - run this from inside the extracted release zip, or build from source first (see README)." >&2
    exit 1
fi

install_bundle() {
    local name=$1 src="$BIN_DIR/$2" dest_dir=$3
    if [[ -e "$src" ]]; then
        mkdir -p "$dest_dir"
        rm -rf "${dest_dir:?}/$2"
        cp -r "$src" "$dest_dir/"
        echo "Installed $name -> $dest_dir/$2"
    fi
}

install_bundle "VST3" "ampforge_main.vst3" "$VST3_DIR"
install_bundle "CLAP" "ampforge_main.clap" "$CLAP_DIR"
install_bundle "LV2"  "ampforge_main.lv2"  "$LV2_DIR"

# Same plain-executable shape as the Linux standalone (DPF doesn't wrap the
# "jack" target in a .app bundle on any platform) - no host needed, run
# straight from a terminal.
if [[ -f "$BIN_DIR/ampforge_main" ]]; then
    mkdir -p "$HOME/.local/bin"
    install -m 755 "$BIN_DIR/ampforge_main" "$HOME/.local/bin/ampforge_main"
    echo "Installed standalone -> $HOME/.local/bin/ampforge_main"
    case ":$PATH:" in
        *":$HOME/.local/bin:"*) ;;
        *) echo "note: $HOME/.local/bin isn't on your PATH - add it, or run the standalone with its full path." ;;
    esac
fi

# macOS Gatekeeper flags anything downloaded but not notarized (this build
# isn't - there's no paid Apple Developer certificate behind it) with an
# "is damaged and can't be opened" dialog on first launch, even though
# nothing is actually wrong with it. Clearing the quarantine attribute
# ourselves is the same fix a user would otherwise have to look up by hand.
xattr -dr com.apple.quarantine "$VST3_DIR/ampforge_main.vst3" \
    "$CLAP_DIR/ampforge_main.clap" "$LV2_DIR/ampforge_main.lv2" \
    "$HOME/.local/bin/ampforge_main" 2>/dev/null || true

echo "Done - rescan plugins in your host."
