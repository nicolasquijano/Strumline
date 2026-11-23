# Third-party notices

Strumline is licensed under the GNU Affero General Public License v3 (see `LICENSE`). It includes or uses the following.

| Component | Used for | Licence | Where |
|---|---|---|---|
| [JUCE](https://juce.com) 8.0.16 | the plug-in framework and the interface | AGPLv3 (or a commercial licence from the JUCE owners) | compiled into the plug-in |
| [ONNX Runtime](https://github.com/microsoft/onnxruntime) 1.32.0 (Microsoft) | runs the chord model | MIT | `onnxruntime.dll` and `onnxruntime_providers_shared.dll`, next to the plug-in; its own notices are in the `ThirdPartyNotices.txt` of the package it comes in |
| [Basic Pitch](https://github.com/spotify/basic-pitch) model (`nmp.onnx`, Spotify) | the chord converter | Apache License 2.0 (text in `engine/models/LICENSE-basic-pitch.txt`) | embedded in the plug-in |

The one-note converter, the monitor synth, the note merger and the interface are original to this project.

Not distributed with Strumline, used only to measure it during development: [GuitarSet](https://zenodo.org/records/3371780) (CC BY 4.0, Xi et al., ISMIR 2018).

The fonts of the interface are the ones the system provides.

## Source code

The source code of Strumline (the corresponding source for the AGPL) is in the repository the release comes from. A copy of an installed version can be obtained from the same place;
the version number is in the name of the installer and in the plug-in's properties.
