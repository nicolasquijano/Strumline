#!/usr/bin/env bash
# Packages the macOS build into dist/ (run it on a Mac, after building with ./build.sh):
#   Strumline-<version>-macos-<arch>.zip   the VST3 bundle and the standalone app (each with libonnxruntime.dylib inside), LICENSE, notices, README, INSTALL.txt
#   SHA256SUMS-macos.txt
# The bundles get an ad-hoc signature (Apple Silicon will not run unsigned code). That is NOT the Developer ID signature plus notarization that
# Gatekeeper wants for a download without warnings.
set -euo pipefail
cd "$(dirname "$0")"

version=$(sed -n 's/^set(STRUMLINE_VERSION \([0-9][0-9.]*\))/\1/p' CMakeLists.txt)
suffix=$(sed -n 's/^set(STRUMLINE_VERSION_SUFFIX "\(.*\)")/\1/p' CMakeLists.txt)
[ -n "$version" ] || { echo "could not read the version from CMakeLists.txt" >&2; exit 1; }
full="${version}${suffix}"
arch=$(uname -m)

artefacts="build/plugin/Strumline_artefacts/Release"
vst3="$artefacts/VST3/Strumline.vst3"
app="$artefacts/Standalone/Strumline.app"
for b in "$vst3" "$app"; do [ -d "$b" ] || { echo "missing bundle: $b" >&2; exit 1; }; done

echo "Packaging Strumline $full for macOS ($arch)"
for b in "$vst3" "$app"; do
  # libonnxruntime.dylib sits in Contents/MacOS next to the binary; --deep signs it together with the bundle (Microsoft's own signature on it is replaced).
  codesign --force --deep --sign - "$b"
  echo "  $(basename "$b"): $(lipo -archs "$b/Contents/MacOS/"$(basename "$b" | sed 's/\..*//') 2>/dev/null | head -1)"
done

name="Strumline $full"
stage="$(mktemp -d)/$name"
mkdir -p "$stage"
cp -R "$vst3" "$app" "$stage/"
cp LICENSE THIRD_PARTY_NOTICES.md README.md README.es.md CHANGELOG.md "$stage/"
cat > "$stage/INSTALL.txt" <<EOF
Strumline $full for macOS ($arch, macOS 11 or later)

1. Copy  Strumline.vst3  to  /Library/Audio/Plug-Ins/VST3  (or ~/Library/Audio/Plug-Ins/VST3 for your user only).
   Strumline.app is the standalone application: copy it to Applications.
2. This beta is not notarized by Apple, so macOS may refuse to load it ("damaged" or "unidentified developer"). Remove the quarantine mark:
     xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Strumline.vst3"
     xattr -dr com.apple.quarantine "/Applications/Strumline.app"
   (use the same paths where you copied them)
3. Rescan the plugins in your DAW. No Audio Unit yet.

The chord converter needs libonnxruntime.dylib, which is inside each bundle. ONNX Runtime ships no Intel library, so on an Intel Mac Strumline plays one note at a time.
Strumline is free software under the AGPLv3 (LICENSE); third-party notices are in THIRD_PARTY_NOTICES.md.
EOF

mkdir -p dist
zip="dist/Strumline-$full-macos-$arch.zip"
rm -f "$zip"
ditto -c -k --keepParent "$stage" "$zip"
echo "  $zip"
shasum -a 256 "$zip" | sed 's#  dist/#  #' > dist/SHA256SUMS-macos.txt
echo "Checksums: dist/SHA256SUMS-macos.txt"
