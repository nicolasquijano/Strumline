"""Takes some recordings of GuitarSet out of its zip files, for the C++ evaluation (plugin/eval.cpp): the audio as it is (<id>.wav, mono, 44.1 kHz) and the
notes the players played as a text file (<id>.txt, one note per line: onset in seconds, offset in seconds, MIDI note), from every string of the JAMS annotation.

    python extract_guitarset.py <folder with audio_mono-mic.zip and annotation.zip> <output folder> [--kind solo|comp] [--files 8]
"""
import argparse
import json
import os
import zipfile


def notes_of(zf, name):
    with zf.open(name) as f:
        jam = json.load(f)
    notes = []
    for ann in jam["annotations"]:
        if ann["namespace"] != "note_midi":
            continue
        data = ann["data"]
        rows = data if isinstance(data, list) else [dict(zip(data.keys(), v)) for v in zip(*data.values())]
        for r in rows:
            t, d, v = float(r["time"]), float(r["duration"]), float(r["value"])
            notes.append((t, t + d, int(round(v))))
    return sorted(notes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("out")
    ap.add_argument("--kind", choices=["solo", "comp"], default="solo")
    ap.add_argument("--files", type=int, default=8)
    a = ap.parse_args()
    za = zipfile.ZipFile(os.path.join(a.source, "audio_mono-mic.zip"))
    zn = zipfile.ZipFile(os.path.join(a.source, "annotation.zip"))
    audio = {os.path.basename(n)[:-8]: n for n in za.namelist() if n.endswith("_mic.wav")}
    ann = {os.path.basename(n)[:-5]: n for n in zn.namelist() if n.endswith(".jams")}
    ids = sorted(i for i in set(audio) & set(ann) if i.endswith("_" + a.kind))
    pick = [ids[int(i * len(ids) / a.files)] for i in range(a.files)]
    os.makedirs(a.out, exist_ok=True)
    for name in pick:
        with open(os.path.join(a.out, name + ".wav"), "wb") as f:
            f.write(za.read(audio[name]))
        with open(os.path.join(a.out, name + ".txt"), "w") as f:
            for t0, t1, m in notes_of(zn, ann[name]):
                f.write("%.4f %.4f %d\n" % (t0, t1, m))
        print(name)


if __name__ == "__main__":
    main()
