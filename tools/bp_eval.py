"""Phase 0 of docs/PLAN_GUITAR_MIDI.md: how good is Basic Pitch (Spotify, Apache 2.0) when it may only look a few frames into the future?

The model is not causal: every output frame depends on about 10 frames (116 ms) of the audio that comes after it, and Basic Pitch itself throws away the
15 frames at each edge of its 2 s window because the edges are unreliable. A live converter has to answer at the edge. This script emulates that: to
answer for frame f with a look-ahead of L frames it gives the model the 2 s of audio that END at frame f + L (nothing later exists), and keeps the output
frame that sits L frames from the end of the window. Then it turns the frames into note-on events (onset peaks) and compares them with the annotations of
GuitarSet (the notes the players really played): precision, recall and F of the note onsets (right pitch, onset within 50 ms), and the latency of each
detected note-on, counted from the real onset to the moment the frame was available (without the time the model takes to run).

    python bp_eval.py --guitarset <folder with audio_mono-mic.zip and annotation.zip> [--files 16] [--seconds 20] [--lookahead 0 3 6 10 15]

Needs onnxruntime and numpy and the nmp.onnx that the basic-pitch package ships (pip install --no-deps basic-pitch). Nothing here is part of the plugin.
"""
import argparse
import io
import json
import os
import sys
import time
import wave
import zipfile

import numpy as np
import onnxruntime as ort

SR = 22050
HOP = 256
WIN = 43844
FRAMES = 172
FRAME_MS = 1000.0 * HOP / SR
LOWEST_MIDI = 21  # the model's 88 bins start at A0


def find_model():
    import basic_pitch
    return os.path.join(os.path.dirname(basic_pitch.__file__), "saved_models", "icassp_2022", "nmp.onnx")


def read_wav(zf, name):
    with zf.open(name) as f:
        w = wave.open(io.BytesIO(f.read()))
    n, ch, width, rate = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(n)
    dtype = {2: "<i2", 4: "<i4"}[width]
    x = np.frombuffer(raw, dtype=dtype).astype(np.float32) / float(2 ** (8 * width - 1))
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return x, rate


def to_22050(x, rate):
    """Resample to 22050 Hz: exact halving for 44.1 kHz (a windowed sinc low-pass, then every second sample)."""
    if rate == SR:
        return x
    if rate != 2 * SR:
        raise SystemExit("unexpected sample rate %d" % rate)
    taps = 63
    k = np.arange(taps) - taps // 2
    h = np.sinc(k / 2.0) / 2.0 * np.hamming(taps)
    h /= h.sum()
    return np.convolve(x, h, mode="same")[::2].astype(np.float32)


def read_notes(zf, name):
    """The notes of a JAMS file: (onset s, offset s, midi pitch rounded), from every note_midi annotation (one per string)."""
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


class Model:
    def __init__(self, threads=1):
        opts = ort.SessionOptions()
        opts.intra_op_num_threads = threads
        self.s = ort.InferenceSession(find_model(), sess_options=opts, providers=["CPUExecutionProvider"])
        self.inp = self.s.get_inputs()[0].name
        self.ms_per_window = None

    def run(self, windows):
        """windows: (n, WIN) -> dict of arrays (n, 172, bins): the three outputs, by their shapes and sparsity."""
        out = self.s.run(None, {self.inp: windows[:, :, None].astype(np.float32)})
        contour = [o for o in out if o.shape[-1] == 264][0]
        flat = [o for o in out if o.shape[-1] == 88]
        # the onset output is the sparser of the two 88-bin outputs
        onset, note = sorted(flat, key=lambda o: float(o.mean()))
        return onset, note, contour


def streaming_frames(model, x, lookahead, batch=48):
    """The onset and note probabilities of every frame as a live converter would get them with `lookahead` frames of future."""
    n_frames = len(x) // HOP
    k = FRAMES - 1 - lookahead  # the index in the window of the frame being answered
    onset = np.zeros((n_frames, 88), np.float32)
    note = np.zeros((n_frames, 88), np.float32)
    padded = np.concatenate([np.zeros(WIN, np.float32), x, np.zeros(WIN, np.float32)])
    t0 = time.perf_counter()
    count = 0
    for start in range(0, n_frames, batch):
        frames = list(range(start, min(n_frames, start + batch)))
        wins = np.stack([padded[WIN + (f - k) * HOP + (0) : WIN + (f - k) * HOP + WIN] for f in frames])
        o, nt, _ = model.run(wins)
        onset[frames] = o[:, k, :]
        note[frames] = nt[:, k, :]
        count += len(frames)
    model.ms_per_window = 1000.0 * (time.perf_counter() - t0) / max(1, count)
    return onset, note


def note_ons(onset, note, onset_thr=0.5, frame_thr=0.3, refractory=4):
    """Note-on events (frame, midi): an onset peak above the threshold, with the note still active a frame later; a pitch cannot start again for `refractory` frames."""
    events = []
    last = {}
    for p in range(onset.shape[1]):
        col = onset[:, p]
        for f in range(1, len(col) - 1):
            if col[f] >= onset_thr and col[f] >= col[f - 1] and col[f] > col[f + 1]:
                if f - last.get(p, -999) < refractory:
                    continue
                if note[min(f + 1, len(col) - 1), p] < frame_thr and col[f] < 0.8:
                    continue
                last[p] = f
                events.append((f, p + LOWEST_MIDI))
    return sorted(events)


def score(events, ref, lookahead, tol=0.05):
    """Greedy one-to-one matching of note-ons to the annotated onsets: the right pitch, within 50 ms."""
    ref_on = [(t, m) for t, _, m in ref]
    used = [False] * len(ref_on)
    tp = 0
    lat = []
    for f, m in events:
        t_ev = f * HOP / SR
        best, bd = -1, tol + 1e-9
        for i, (t, mm) in enumerate(ref_on):
            if used[i] or mm != m:
                continue
            d = abs(t_ev - t)
            if d < bd:
                best, bd = i, d
        if best >= 0:
            used[best] = True
            tp += 1
            # the frame f can be announced once the frame f + lookahead + 1 exists (one more frame to see that it is a peak)
            lat.append(((f + lookahead + 1) * HOP / SR - ref_on[best][0]) * 1000.0)
    return tp, len(events), len(ref_on), lat


def summarize(a, tot, speed, quiet=False):
    lines = ["look-ahead  frames  ms     precision  recall  F      latency median / p90 (ms, without the model's run time)   model ms per window (%d threads)" % a.threads]
    for L in a.lookahead:
        tp, n_ev, n_ref, lat = tot[L]
        if not n_ref:
            continue
        p, r = tp / max(1, n_ev), tp / max(1, n_ref)
        f = 2 * p * r / max(1e-9, p + r)
        lat = np.array(lat) if lat else np.array([float("nan")])
        lines.append("%10d  %6.0f  %7.0f %9.2f  %6.2f  %5.2f  %8.0f / %-8.0f %27.2f" % (L, L, L * FRAME_MS, p, r, f, np.nanmedian(lat), np.nanpercentile(lat, 90), speed.get(L, float("nan"))))
    text = "\n".join(lines)
    if a.out:
        with open(a.out, "w", encoding="utf-8") as fh:
            fh.write(text + "\n")
    if not quiet:
        print("\n" + text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--guitarset", required=True)
    ap.add_argument("--files", type=int, default=16)
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--lookahead", type=int, nargs="+", default=[0, 3, 6, 10, 15])
    ap.add_argument("--kind", choices=["comp", "solo", "both"], default="both")
    ap.add_argument("--onset-thr", type=float, default=0.5)
    ap.add_argument("--out", default="", help="a text file that gets the summary table, rewritten after every file")
    ap.add_argument("--threads", type=int, default=1, help="threads of the model (the speed of the evaluation; the ms per window printed are for this many)")
    a = ap.parse_args()

    za = zipfile.ZipFile(os.path.join(a.guitarset, "audio_mono-mic.zip"))
    zn = zipfile.ZipFile(os.path.join(a.guitarset, "annotation.zip"))
    audio = {os.path.basename(n)[:-8]: n for n in za.namelist() if n.endswith("_mic.wav")}
    ann = {os.path.basename(n)[:-5]: n for n in zn.namelist() if n.endswith(".jams")}
    ids = sorted(set(audio) & set(ann))
    if a.kind != "both":
        ids = [i for i in ids if i.endswith("_" + a.kind)]
    # an even spread over the whole set (players, genres, tempos)
    pick = [ids[int(i * len(ids) / a.files)] for i in range(a.files)]
    model = Model(a.threads)
    print("%d files of %d, %.0f s each; model %s" % (len(pick), len(ids), a.seconds, os.path.basename(find_model())))

    tot = {L: [0, 0, 0, []] for L in a.lookahead}
    speed = {}
    for name in pick:
        x, rate = read_wav(za, audio[name])
        x = to_22050(x, rate)[: int(a.seconds * SR)]
        ref = [n for n in read_notes(zn, ann[name]) if n[0] < a.seconds]
        row = []
        for L in a.lookahead:
            onset, note = streaming_frames(model, x, L)
            speed[L] = model.ms_per_window
            ev = note_ons(onset, note, a.onset_thr)
            tp, n_ev, n_ref, lat = score(ev, ref, L)
            t = tot[L]
            t[0] += tp; t[1] += n_ev; t[2] += n_ref; t[3] += lat
            row.append("L%d F=%.2f" % (L, 2 * tp / max(1, n_ev + n_ref)))
        print("%-28s notes %3d  %s" % (name, len(ref), "  ".join(row)))
        sys.stdout.flush()
        if a.out:
            summarize(a, tot, speed, quiet=True)  # an interrupted run keeps what it measured

    summarize(a, tot, speed)


if __name__ == "__main__":
    main()
