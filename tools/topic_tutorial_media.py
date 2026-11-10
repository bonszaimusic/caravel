#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
"""The screens and the sound examples of the topic tutorials, from the actual firmware code on the host.

  tools/tutorial_media_docker.sh topic_tutorial_media.py render      compile and run tools/topic_tutorial_media.c: PNG screens in docs/tutorials/img,
                                                                  WAV renders in build/release-prep/audio (needs a C compiler, Pillow, build/gen)
  python3 tools/topic_tutorial_media.py encode                      on the host (needs ffmpeg): WAV -> MP3 in docs/audio/tutorials and the manifest
                                                                  docs/audio/tutorials.json (SHA-256, gain, decoded peak and RMS of every clip)

The sounds are native DSP renders: the firmware's own code, MIDI notes in, audio out, computed on a computer. They are not recordings of an FM-1;
the presets load with their factory sends, global effects at their defaults. Every clip is peak-normalised to -3 dBFS with a 150 ms end fade."""
import array
import hashlib
import json
import math
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/release-prep"
IMG = ROOT / "docs/tutorials/img"
AUDIO = ROOT / "docs/audio/tutorials"
CLIPS = {  # key: (title, engine, what is played)
    "sem-warm-pad": ("SEM WARM PAD", "SEM", "a held C3 - G3 - E4 chord"), "sem-brass": ("SEM SEM BRASS", "SEM", "a held C3 - G3 - E4 chord"),
    "sem-notch-pad": ("SEM NOTCH PAD", "SEM", "a held C3 - G3 - E4 chord"), "sem-round-bas": ("SEM ROUND BAS", "SEM", "a mono bass line (MONO voice mode)"),
    "hades-deep-sub": ("HADES DEEP SUB", "HADES", "a mono bass line (MONO voice mode)"), "hades-dark-saw": ("HADES DARK SAW", "HADES", "a mono bass line (MONO voice mode)"),
    "hades-dark-plk": ("HADES DARK PLK", "HADES", "a mono bass line (MONO voice mode)"), "hades-raw-lead": ("HADES RAW LEAD", "HADES", "a mono melody (MONO voice mode)"),
    "drone-still-air": ("DRONE STILL AIR", "DRONE", "two held notes, C3 and G3"), "drone-slow-chain": ("DRONE SLOW CHAIN", "DRONE", "two held notes, C3 and G3"),
    "drone-dark-ring": ("DRONE DARK RING", "DRONE", "two held notes, C3 and G3"), "drone-tide": ("DRONE TIDE", "DRONE", "two held notes, C3 and G3"),
    "ensemble-vintage-strings": ("ENSEMBLE VINTAGE STRINGS", "ENSEMBLE", "a held C3 - G3 - E4 chord"), "ensemble-glass-choir": ("ENSEMBLE GLASS CHOIR", "ENSEMBLE", "a held C3 - G3 - E4 chord"),
    "ensemble-abyss-pad": ("ENSEMBLE ABYSS PAD", "ENSEMBLE", "a held C3 - G3 - E4 chord"), "ensemble-glass-shimmer": ("ENSEMBLE GLASS SHIMMER", "ENSEMBLE", "a short melody"),
}


def render():
    from PIL import Image
    (OUT / "audio").mkdir(parents=True, exist_ok=True)
    exe = ROOT / "build/topic-tutorial-media"
    subprocess.run(["cc", "-O2", "-w", "-ffp-contract=off", "-Ibuild/gen", "-Ifirmware/src", "-DCARAVEL_ACTION_BANKS=1", "-DFELUCCA_RANDOM_MORPH=1", "-DCARAVEL_REVERB_MINIMAL=1",
                    "-DCARAVEL_STASIS_EXPAND=1", "-DFELUCCA_SLICE=0", "-DFELUCCA_PERF_HW=1", "tools/topic_tutorial_media.c", "-o", str(exe), "-lm"], cwd=ROOT, check=True)
    for old in OUT.glob("*.ppm"):
        old.unlink()
    subprocess.run([str(exe), "frames"], cwd=ROOT, check=True)
    names = sorted(p.stem for p in OUT.glob("*.ppm"))
    for name in names:
        if name.startswith(("start-", "card-")):
            continue                                        # the START HERE frames belong to tools/start_tutorial_frames.py
        Image.open(OUT / (name + ".ppm")).save(IMG / (name + ".png"), optimize=True)
    for key in CLIPS:
        subprocess.run([str(exe), "audio", key], cwd=ROOT, check=True)
    print("wrote", len([n for n in names if not n.startswith(("start-", "card-"))]), "screens and", len(CLIPS), "renders")


def read_wav(path):
    with wave.open(str(path)) as f:
        assert f.getnchannels() == 2 and f.getframerate() == 44100
        data = array.array("h", f.readframes(f.getnframes()))
    return data


def stats_f32(raw):
    a = array.array("f"); a.frombytes(raw)
    peak = max(abs(x) for x in a)
    rms = math.sqrt(sum(x * x for x in a) / len(a))
    return peak, rms, len(a) // 2


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def encode():
    AUDIO.mkdir(parents=True, exist_ok=True)
    clips = []
    for key, (title, engine, played) in CLIPS.items():
        wav = OUT / "audio" / (key + ".wav")
        samples = read_wav(wav)
        peak = max(abs(x) for x in samples) / 32768
        assert .001 < peak < 1.0, key
        gain = 10 ** (-3 / 20) / peak
        seconds = len(samples) / 2 / 44100
        mp3 = AUDIO / (key + ".mp3")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(wav), "-af", f"volume={gain:.12f},afade=t=out:st={seconds - .15:.3f}:d=0.15",
                        "-codec:a", "libmp3lame", "-q:a", "4", "-map_metadata", "-1", str(mp3)], check=True)
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(mp3), "-f", "f32le", "-acodec", "pcm_f32le", "-"], capture_output=True, check=True).stdout
        dpeak, drms, n = stats_f32(raw)
        assert math.isfinite(dpeak) and dpeak < 1.0 and abs(n / 44100 - seconds) < .1 and drms > .005, key
        clips.append({"id": key, "file": key + ".mp3", "title": title, "engine": engine, "played": played, "seconds": round(seconds, 1), "sha256": sha(mp3),
                      "raw_wav_sha256": sha(wav), "gain_db": round(20 * math.log10(gain), 3), "decoded_peak": round(dpeak, 4), "rms": round(drms, 4)})
        print(key, clips[-1]["seconds"], "s", clips[-1]["decoded_peak"], clips[-1]["rms"])
    manifest = {"origin": "Native render of the actual Caravel firmware DSP (host harness, tools/topic_tutorial_media.c); not a recording of an FM-1.",
                "processing": "Constant peak normalization to -3 dBFS and a 150-ms end fade; MP3 (LAME VBR q4); no external sound effects. The preset loads with its factory sends; global effects at their defaults.",
                "sample_rate": 44100, "channels": 2, "clips": clips}
    (ROOT / "docs/audio/tutorials.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "render":
        render()
    elif mode == "encode":
        encode()
    else:
        sys.exit(__doc__)
