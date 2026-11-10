#!/bin/bash
# Arma Lineage.app y "Lineage Settings.app" (universales: arm64 y x86_64) y un .dmg con los dos.
# Uso: herramientas/empaquetar_macos.sh [VERSIÓN]   (por defecto, 0.0.0)
# Deja todo en dist/. Lo usa .github/workflows/release.yml; también se puede correr a mano.
#
# - Compila con el preset release (sin el control en vivo) en build/empaquetado.
# - datos/ va en Contents/Resources: dentro de un .app, SDL_GetBasePath() devuelve esa carpeta.
# - Firma ad hoc: sin una cuenta de Apple Developer, macOS avisa que no puede verificar al
#   desarrollador (clic derecho > Abrir la primera vez). Firmar y notarizar queda pendiente.
# - No incluye recursos del juego: el jugador elige su carpeta en Settings.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-0.0.0}"
BUILD=build/empaquetado
DIST=dist

cmake --preset release -B "$BUILD" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build "$BUILD" --target lineage settings
python3 tools/generar_iconos.py --icns "$BUILD/iconos" >/dev/null

rm -rf "$DIST"
mkdir -p "$DIST/dmg"

# app NOMBRE EJECUTABLE ID ICONO
app() {
    local nombre="$1" ejecutable="$2" id="$3" icono="$4"
    local raiz="$DIST/dmg/$nombre.app/Contents"
    mkdir -p "$raiz/MacOS" "$raiz/Resources"
    cp "$BUILD/$ejecutable" "$raiz/MacOS/"
    cp -R datos "$raiz/Resources/"
    cp "$BUILD/iconos/$icono.icns" "$raiz/Resources/"
    cat > "$raiz/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>$nombre</string>
    <key>CFBundleDisplayName</key><string>$nombre</string>
    <key>CFBundleExecutable</key><string>$ejecutable</string>
    <key>CFBundleIdentifier</key><string>$id</string>
    <key>CFBundleIconFile</key><string>$icono</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key><string>$VERSION</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST
    codesign --force --deep --sign - "$DIST/dmg/$nombre.app"
}

app "Lineage" lineage cl.lineage.cliente lineage
app "Lineage Settings" settings cl.lineage.settings settings
ln -s /Applications "$DIST/dmg/Applications"

hdiutil create -volname "Lineage $VERSION" -srcfolder "$DIST/dmg" -ov -format UDZO "$DIST/Lineage-$VERSION-macos.dmg" >/dev/null
rm -rf "$DIST/dmg"
echo "$DIST/Lineage-$VERSION-macos.dmg"
