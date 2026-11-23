#!/usr/bin/env bash
# Packages the Linux build into dist/ (run it on Linux, after building with ./build.sh):
#   Strumline-<version>-linux-<arch>.tar.gz   the VST3 bundle (with libonnxruntime.so inside) and the standalone application, LICENSE, notices, README, INSTALL.txt
#   SHA256SUMS-linux.txt
set -euo pipefail
cd "$(dirname "$0")"

version=$(sed -n 's/^set(STRUMLINE_VERSION \([0-9][0-9.]*\))/\1/p' CMakeLists.txt)
suffix=$(sed -n 's/^set(STRUMLINE_VERSION_SUFFIX "\(.*\)")/\1/p' CMakeLists.txt)
[ -n "$version" ] || { echo "could not read the version from CMakeLists.txt" >&2; exit 1; }
full="${version}${suffix}"
arch=$(uname -m)

artefacts="build/plugin/Strumline_artefacts/Release"
vst3="$artefacts/VST3/Strumline.vst3"
exe="$artefacts/Standalone/Strumline"
[ -d "$vst3" ] || { echo "missing bundle: $vst3" >&2; exit 1; }
[ -f "$exe" ] || { echo "missing standalone: $exe" >&2; exit 1; }

echo "Packaging Strumline $full for Linux ($arch)"
name="Strumline-$full-linux-$arch"
stage="$(mktemp -d)/$name"
mkdir -p "$stage/Standalone"
cp -R "$vst3" "$stage/"
cp "$exe" "$stage/Standalone/"
# The standalone opens libonnxruntime.so from its own folder.
cp "$artefacts"/Standalone/libonnxruntime*.so "$stage/Standalone/" 2>/dev/null || echo "  (no libonnxruntime next to the standalone: it will play one note at a time)"
chmod +x "$stage/Standalone/Strumline"
cp LICENSE THIRD_PARTY_NOTICES.md README.md README.es.md CHANGELOG.md "$stage/"
cat > "$stage/INSTALL.txt" <<EOF
Strumline $full for Linux ($arch)

VST3 plug-in:
  Copy the folder  Strumline.vst3  to  ~/.vst3  (for your user) or /usr/lib/vst3 (for everybody), then rescan the plugins in your DAW.
  libonnxruntime.so has to stay inside the bundle, next to Strumline.so (the chord converter opens it from there).

Standalone application:
  Run  Standalone/Strumline  (it needs a desktop session and ALSA or JACK; keep the .so files next to it).

Both need these libraries (Debian / Ubuntu package names; others are similar):
  libasound2 libx11-6 libxext6 libxrandr2 libxinerama1 libxcursor1 libfreetype6 libfontconfig1 libgl1

This beta was built and checked by GitHub Actions on Ubuntu; it has not been tried in a Linux DAW yet.
Strumline is free software under the AGPLv3 (LICENSE); third-party notices are in THIRD_PARTY_NOTICES.md.
EOF

mkdir -p dist
tarball="dist/$name.tar.gz"
rm -f "$tarball"
tar -C "$(dirname "$stage")" -czf "$tarball" "$name"
echo "  $tarball"
(cd dist && sha256sum "$name.tar.gz" > SHA256SUMS-linux.txt)
echo "Checksums: dist/SHA256SUMS-linux.txt"
