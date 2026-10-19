#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Analyze synthetic hades_audio.c WAVs; make explicitly RMS-matched copies.

Usage: python3 tools/hades_audio_report.py build/hades_audio
Requires numpy. Does not download or redistribute hardware-demo audio.
"""
import json
import math
from pathlib import Path
import sys
import wave

import numpy as np


def read(path):
    with wave.open(str(path), "rb") as f:
        assert f.getsampwidth() == 2 and f.getnchannels() == 2
        fs = f.getframerate()
        x = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").reshape(-1, 2).astype(float) / 32768
    return fs, x


def db(x):
    return 20 * math.log10(max(float(x), 1e-12))


def analyze(path):
    fs, x = read(path)
    mono = x.mean(axis=1)
    active = mono[: fs * 4]
    spec = np.abs(np.fft.rfft(active * np.hanning(len(active)))) ** 2
    hz = np.fft.rfftfreq(len(active), 1 / fs)
    total = max(spec.sum(), 1e-20)
    return dict(file=path.name, seconds=len(x) / fs, rms_dbfs=db(np.sqrt(np.mean(active ** 2))),
                peak_dbfs=db(np.max(np.abs(x))), dc=float(active.mean()),
                last_half_second_peak_dbfs=db(np.max(np.abs(mono[-fs // 2:]))),
                below_150hz_percent=round(float(spec[hz < 150].sum() / total * 100), 2),
                centroid_hz=round(float((spec * hz).sum() / total), 1),
                full_scale_samples=int(np.sum(np.abs(x) >= 32767 / 32768)))


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1 else "build/hades_audio")
    files = sorted(root.glob("*.wav"))
    files = [f for f in files if not f.stem.endswith("_rms_matched")]
    rows = [analyze(f) for f in files]
    # A shared gain target, capped jointly for all three files: maintain matched
    # active-region RMS and never clip. This is RMS matching, not LUFS matching.
    compare = [f for f in files if f.name.startswith("compare_")]
    target = 10 ** (-20 / 20)
    gains = []
    for f in compare:
        fs, x = read(f)
        rms = np.sqrt(np.mean(x[:fs * 4].mean(axis=1) ** 2))
        gains.append(target / max(rms, 1e-12))
    peak = max((np.max(np.abs(read(f)[1])) * g for f, g in zip(compare, gains)), default=0)
    common = min(1.0, 0.89 / max(peak, 1e-12))
    for f, gain in zip(compare, gains):
        fs, x = read(f)
        with wave.open(str(f.with_name(f.stem + "_rms_matched.wav")), "wb") as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(fs)
            w.writeframes(np.round(x * gain * common * 32768).astype("<i2").tobytes())
    metadata = dict(note="Synthetic Felucca renders. No hardware reference was numerically matched.",
                    comparison="Same notes, envelope, cutoff index and resonance index; parameter indices do not imply identical physical resonance.",
                    rms_matching="First four seconds, mono RMS; common clipping-safe target. Not LUFS.", rows=rows)
    (root / "metrics.json").write_text(json.dumps(metadata, indent=2) + "\n")
    lines = ["# HADES listening and measurement set", "", metadata["note"], "", metadata["comparison"], "",
             "Listen to `compare_*_rms_matched.wav` for a level-matched comparison. Original `_dry.wav` files preserve native levels.",
             "All eight factory presets and the cutoff/drive sweep are dry, with no insert, delay, chorus or reverb.", "",
             "| File | RMS dBFS | Peak dBFS | Below 150 Hz | DC |",
             "| --- | ---: | ---: | ---: | ---: |"]
    for r in rows:
        lines.append(f"| {r['file']} | {r['rms_dbfs']:.1f} | {r['peak_dbfs']:.1f} | {r['below_150hz_percent']:.1f}% | {r['dc']:.6f} |")
    lines += ["", "These measurements establish reproducibility and signal health, not Hades hardware authenticity.",
              "Manufacturer reference: https://www.youtube.com/watch?v=aUy92MXWezg . Metadata was accessible, but media retrieval did not yield decodable audio in this environment.",
              "No numerical hardware A/B or claimed perceptual match has therefore been performed.", ""]
    (root / "README.md").write_text("\n".join(lines))
    print("\n".join(lines))


if __name__ == "__main__":
    main()
