# Releasing

Versions follow [Semantic Versioning](https://semver.org/), with a `-beta` suffix while the saved-project format (the parameter ids and their meaning) may still change. The version is written in
**one place**: `STRUMLINE_VERSION` and `STRUMLINE_VERSION_SUFFIX` at the top of `CMakeLists.txt`; the plug-in, the package scripts and the installer read it.

## Steps of a release

1. The tree is clean and `strumline_check` passes. CI (`.github/workflows/ci.yml`) builds and checks Windows, macOS and Linux on every push to master; look at it before tagging.
2. Edit the version in `CMakeLists.txt`.
3. Add a section `## [x.y.z] - YYYY-MM-DD` to `CHANGELOG.md` (it becomes the notes of the release) and the link at the bottom.
4. Commit `Release x.y.z`, then tag it: `git tag -a vx.y.z -m "Strumline x.y.z"` and `git push origin master --tags`.
5. Pushing the tag starts `.github/workflows/release.yml`: it builds, checks and packages **Windows** (installer + zip), **macOS** (zip) and **Linux** (tar.gz), and creates a **draft** release with all of them,
   the checksums and the changelog section as notes (a tag with a `-` in it, like `-beta`, is marked as a pre-release). If macOS or Linux fail, the Windows packages are still attached.
6. Read the draft on GitHub (Releases), try the packages, and press "Publish release".

`package.cmd` (Windows), `package-linux.sh` and `package-macos.sh` make the same packages by hand into `dist\` (`package.cmd -SkipTests` skips the check). `dist/`, `build/` and `third_party/` are ignored by git.

## Notes per system
- **Windows:** `installer/strumline.iss` (Inno Setup 6 or 7). The `AppId` GUID in it identifies the product so that a newer installer replaces the older install: never change it. Not code-signed, so SmartScreen may warn.
  The VST3 bundle carries `onnxruntime.dll` next to `Strumline.vst3`: the chord converter opens it from there.
- **macOS:** the build is for the architecture of the runner (arm64). ONNX Runtime's package has no Intel library, so an Intel Mac would play one note at a time. The bundles get an ad-hoc signature; a download without
  warnings needs a Developer ID signature plus notarization (not done). No Audio Unit yet.
- **Linux:** a tar.gz with the VST3 bundle (with `libonnxruntime.so` inside) and the standalone. Built on Ubuntu 22.04; not tried in a Linux DAW yet.
