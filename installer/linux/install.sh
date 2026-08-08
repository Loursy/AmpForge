#!/usr/bin/env bash
# AmpForge Linux installer.
#
# Copies the prebuilt ampforge_main.vst3 / ampforge_main.clap /
# ampforge_main.lv2 / ampforge_main into the same per-user directories
# the README's manual "3. Install" steps already use (~/.vst3, ~/.clap,
# ~/.lv2, ~/.local/bin), so a Linux user doesn't have to hunt down the
# paths themselves. Works from two layouts: inside an extracted release
# tarball (a bin/ dir sits right next to this script), or run directly
# from the repo root after building from source (falls back to
# build/bin/).
#
# No sudo/root needed - matches how Linux plugin hosts (Reaper, Carla,
# Ardour, ...) already scan a user's home directory by default.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -d "$SCRIPT_DIR/bin" ]]; then
    BIN_DIR="$SCRIPT_DIR/bin"          # extracted release tarball
else
    BIN_DIR="$SCRIPT_DIR/../../build/bin"  # repo root, built from source
fi

if [[ "${1:-}" == "--uninstall" ]]; then
    rm -rf "$HOME/.vst3/ampforge_main.vst3" "$HOME/.clap/ampforge_main.clap" \
           "$HOME/.lv2/ampforge_main.lv2" "$HOME/.local/bin/ampforge_main"
    echo "AmpForge removed."
    exit 0
fi

if [[ ! -d "$BIN_DIR" ]]; then
    echo "error: $BIN_DIR not found - run this from inside the extracted release tarball, or build from source first (see README)." >&2
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

install_bundle "VST3" "ampforge_main.vst3" "$HOME/.vst3"
install_bundle "CLAP" "ampforge_main.clap" "$HOME/.clap"
install_bundle "LV2"  "ampforge_main.lv2"  "$HOME/.lv2"

if [[ -f "$BIN_DIR/ampforge_main" ]]; then
    mkdir -p "$HOME/.local/bin"
    install -m 755 "$BIN_DIR/ampforge_main" "$HOME/.local/bin/ampforge_main"
    echo "Installed standalone -> $HOME/.local/bin/ampforge_main"
    case ":$PATH:" in
        *":$HOME/.local/bin:"*) ;;
        *) echo "note: $HOME/.local/bin isn't on your PATH - add it, or run the standalone with its full path." ;;
    esac
fi

echo "Done - rescan plugins in your host."
