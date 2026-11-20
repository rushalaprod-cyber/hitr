#!/bin/bash
# Construit l'installeur macOS de h.i.t.r (VST3 + AU + application), binaire universel.
# Usage : packaging/macos/package.sh <dossier artefacts Release> <dossier de sortie> <version>
set -euo pipefail

ART="$1"
OUT="$2"
VER="${3:-1.1.0}"
STAGE="$(mktemp -d)"
mkdir -p "$OUT" "$STAGE/vst3" "$STAGE/au" "$STAGE/app"
OUT="$(cd "$OUT" && pwd)"   # chemin absolu (le zip est créé depuis le dossier temporaire)

cp -R "$ART/VST3/h.i.t.r.vst3"        "$STAGE/vst3/"
cp -R "$ART/AU/h.i.t.r.component"     "$STAGE/au/"
cp -R "$ART/Standalone/h.i.t.r.app"   "$STAGE/app/"

# Signature ad-hoc (obligatoire pour charger un plugin sur Apple Silicon)
for b in "$STAGE/vst3/h.i.t.r.vst3" "$STAGE/au/h.i.t.r.component" "$STAGE/app/h.i.t.r.app"; do
    codesign --force --deep --sign - "$b"
done

make_pkg () {
    local name="$1" location="$2" id="$3"
    pkgbuild --analyze --root "$STAGE/$name" "$STAGE/$name.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$STAGE/$name.plist"
    pkgbuild --root "$STAGE/$name" --component-plist "$STAGE/$name.plist" \
             --install-location "$location" --identifier "$id" --version "$VER" "$STAGE/$name.pkg"
}

make_pkg vst3 "/Library/Audio/Plug-Ins/VST3"       com.hitr.vst3
make_pkg au   "/Library/Audio/Plug-Ins/Components" com.hitr.au
make_pkg app  "/Applications"                      com.hitr.app

productbuild --package "$STAGE/vst3.pkg" --package "$STAGE/au.pkg" --package "$STAGE/app.pkg" \
             "$OUT/hitr-$VER-macos.pkg"

# Archive zip en secours (installation manuelle)
( cd "$STAGE" && mkdir -p zip && cp -R vst3/h.i.t.r.vst3 au/h.i.t.r.component app/h.i.t.r.app zip/ \
  && ditto -c -k --sequesterRsrc zip "$OUT/hitr-$VER-macos.zip" )

echo "OK : $OUT/hitr-$VER-macos.pkg"
