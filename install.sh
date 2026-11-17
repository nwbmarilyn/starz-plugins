#!/usr/bin/env bash
set -e
REPO="${STARZ_REPO:-nwbmarilyn/starz-plugins}"
NAMES="StarzVocal StarzReverb StarzDelay"
if [ "$1" = "--system" ]; then
  DEST="/Library/Audio/Plug-Ins/VST3"
  SUDO="sudo"
else
  DEST="$HOME/Library/Audio/Plug-Ins/VST3"
  SUDO=""
fi
HERE="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" 2>/dev/null && pwd || echo "")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ "$1" = "--uninstall" ]; then
  for n in $NAMES; do rm -rf "$DEST/$n.vst3" && echo "удалён $n"; done
  exit 0
fi

$SUDO mkdir -p "$DEST"
for n in $NAMES; do
  if [ -n "$HERE" ] && [ -d "$HERE/dist/mac/$n.vst3" ]; then
    $SUDO rm -rf "$DEST/$n.vst3"
    $SUDO cp -R "$HERE/dist/mac/$n.vst3" "$DEST/$n.vst3"
  else
    curl -fsSL "https://github.com/$REPO/releases/latest/download/$n.vst3.zip" -o "$TMP/$n.zip"
    $SUDO rm -rf "$DEST/$n.vst3"
    $SUDO ditto -x -k "$TMP/$n.zip" "$DEST"
  fi
  $SUDO xattr -rd com.apple.quarantine "$DEST/$n.vst3" 2>/dev/null || true
  $SUDO codesign --force --deep -s - "$DEST/$n.vst3" 2>/dev/null || true
  echo "установлен $n"
done
echo "Готово: $DEST"
echo "Перезапусти FL Studio и пересканируй плагины. В проекте плагины вставляй заново."
