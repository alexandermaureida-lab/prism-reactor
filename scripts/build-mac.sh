#!/usr/bin/env bash
# Compila Prism Reactor en macOS (universal arm64 + x86_64), lo firma ad-hoc
# e instala VST3 y AU para el usuario actual. Ejecutar desde la raíz del repo:
#   chmod +x scripts/build-mac.sh && ./scripts/build-mac.sh
set -euo pipefail

if [[ "$(uname)" != "Darwin" ]]; then
  echo "Este script es solo para macOS." >&2
  exit 1
fi

command -v cmake >/dev/null || { echo "Falta cmake. Instálalo con: brew install cmake"; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Faltan las herramientas de Xcode. Ejecuta: xcode-select --install"; exit 1; }

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-mac"
OUT="$BUILD/PrismReactor_artefacts/Release"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build "$BUILD" --config Release --parallel \
      --target PrismReactor_VST3 PrismReactor_AU PrismReactor_Standalone

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

install_bundle () {
  local src="$1" dest_dir="$2"
  local name; name="$(basename "$src")"
  rm -rf "$dest_dir/$name"
  cp -R "$src" "$dest_dir/"
  xattr -cr "$dest_dir/$name"                                   # quita la cuarentena
  codesign --force --deep --sign - "$dest_dir/$name"            # firma ad-hoc (obligatoria en Apple Silicon)
  echo "Instalado: $dest_dir/$name"
}

install_bundle "$OUT/VST3/Prism Reactor.vst3" "$VST3_DIR"
install_bundle "$OUT/AU/Prism Reactor.component" "$AU_DIR"

echo
echo "Listo. En Ableton Live:"
echo "  1. Settings > Plug-Ins: activa 'Use VST3 Plug-In System Folders' (y AU si quieres el AU)."
echo "  2. Pulsa 'Rescan' (mantén Option para un rescan completo)."
echo "  3. Plug-Ins > Prism Reactor, arrástralo a una pista (o al Master)."
echo
echo "Para probarlo sin Live: open \"$OUT/Standalone/Prism Reactor.app\""
