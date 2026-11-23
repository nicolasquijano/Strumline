# Changelog

All notable changes to Strumline. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the version numbers follow
[Semantic Versioning](https://semver.org/) (see [docs/RELEASING.md](docs/RELEASING.md)).

## [Unreleased]

## [0.1.0-beta] - 2026-11-23

First release, for Windows (VST3 and standalone, tried by hand), Linux (x86_64) and macOS (Apple Silicon). The Linux and macOS packages are built and checked (`strumline_check`) by GitHub Actions but have not been tried in a DAW yet.

### Added
- **Guitar to MIDI in real time** from an ordinary electric guitar:
  - a one-note converter (about 30 to 40 ms from the pluck to the note-on, with pitch bend and a tuner-style readout in cents);
  - a chord converter (Spotify's Basic Pitch model through ONNX Runtime, on its own thread: about 80 to 100 ms), with a look-ahead control;
  - three modes: **Mono**, **Poly** and **Hybrid** (the first note fast, the rest of the chord after it).
- Output: MIDI channel, transpose, velocity curve, pitch bend off / channel; a built-in monitor synth (sine, saw, square); THRU for the guitar; a TEST script; PANIC.
- Interface: relief style with the logo, input level with the sensitivity mark, the six strings, a piano roll of the last 12 seconds with the pitch bend lane, latency of the note and of the chord,
  load of the chord model, presets, tooltips, and **ten languages** (English, Spanish, Portuguese, Italian, Chinese, French, German, Japanese, Russian, Korean; Auto follows the system).
- Packages for the three systems: a Windows installer (VST3 for all users or only the current user, standalone, Start menu entry) and zip; a Linux tar.gz; a macOS zip (ad-hoc signed, not notarized: see INSTALL.txt).

### Known limitations
- Chord latency is 80 to 100 ms, above the 30 to 60 ms goal; getting there needs a causal model of our own.
- The chord converter takes about 0.5 to 1 core while a chord rings (nothing when there is silence); "Saver" lowers it by a third and costs 8 ms.
- MPE (a pitch bend per note) is not written: "MPE" sends like "Channel".
- On GuitarSet recordings (microphone, not an interface) the F measure is about 0.72 for chords and 0.77 for solos in Poly mode.
- The translations have not been reviewed by native speakers.
- Linux and macOS: built and checked by CI but not tried in a DAW. The macOS build is for Apple Silicon only (the ONNX Runtime package has no Intel-Mac library, so an Intel Mac would play one note at a time) and is not notarized.
- The installer is not code-signed, so Windows SmartScreen may warn the first time.

[Unreleased]: https://github.com/nicolasquijano/Strumline/compare/v0.1.0-beta...HEAD
[0.1.0-beta]: https://github.com/nicolasquijano/Strumline/releases/tag/v0.1.0-beta
