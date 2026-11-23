# Releasing

Versions follow [Semantic Versioning](https://semver.org/), with a `-beta` suffix while the saved-project format (the parameter ids and their meaning) may still change. The version is written in
**one place**: `STRUMLINE_VERSION` and `STRUMLINE_VERSION_SUFFIX` at the top of `CMakeLists.txt`; the plug-in, `package.ps1` and the installer read it.

Steps: tests pass (`strumline_check`) and the tree is clean; edit the version; add a section to `CHANGELOG.md`; commit `Release x.y.z`; run `package.cmd` (builds, runs the check, writes `dist\`: the zip,
the installer if Inno Setup 6 or 7 is installed, and `SHA256SUMS.txt`); try the installer on a machine or a DAW that does not have the plug-in; tag `vx.y.z`; push with the tags; on GitHub draft a
release from the tag with the changelog section as the notes and the files of `dist\` attached (pre-release while the version has the `-beta` suffix).

The installer is `installer/strumline.iss`; the `AppId` GUID in it identifies the product so that a newer installer replaces the older install: never change it. It is not code-signed, so SmartScreen may warn.
The VST3 bundle must carry `onnxruntime.dll` next to `Strumline.vst3`: the chord converter opens it from there. `dist/`, `build/` and `third_party/` are ignored by git.
