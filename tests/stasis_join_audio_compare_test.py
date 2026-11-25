#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Smoke-regress the raw WAV clipping gate; no engines, tables, or FM-1 needed.

Execute independently in a private checkout:
  python3 tests/stasis_join_audio_compare_test.py
The synthetic WAVs are NOT an STASIS audible-quality or DSP acceptance test.
"""
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "stasis2_join_audio_compare.py"
FS = 44100
CTL = 32
NFRAMES = FS * 9
RESUME = (FS * 8 + FS // 7) // CTL * CTL
NAMES = ("hq", "long", "drone")
VERSIONS = ("default", "joined", "lut")


def write_wav(path, raw):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(raw)


def inputs(folder):
    base = bytes(struct.pack("<hh", 1200, -900)) * NFRAMES
    for name in NAMES:
        for mode in VERSIONS:
            raw = bytearray(base)
            if name == "hq" and mode != "default":
                # One change AFTER RESUME for both candidates; LUT may be
                # identical to the cubic join at PCM16 resolution.
                struct.pack_into("<h", raw, (RESUME + 32) * 4, 1260)
            write_wav(folder / f"{name}-{mode}.wav", raw)


def run(folder, expect_success):
    p = subprocess.run([sys.executable, str(SCRIPT), str(folder)],
                       capture_output=True, text=True, check=False)
    if expect_success:
        assert p.returncode == 0, (p.stdout, p.stderr)
    else:
        assert p.returncode == 1, (p.stdout, p.stderr)
    return p


def test_headroom_gate():
    with tempfile.TemporaryDirectory(prefix="stasis-join-wav-gate-") as t:
        folder = pathlib.Path(t)
        inputs(folder)
        run(folder, True)
        summary = json.loads((folder / "analysis.json").read_text())
        assert summary["audition_audio_gate"] == "PASS"
        assert summary["clip_warning"] is False
        assert summary["headroom_warning"] is False
        assert all((folder / f"{q}-{v}-listen.wav").is_file()
                   for q in NAMES for v in VERSIONS)

        # Re-running on a bad raw render must REMOVE all old "listen" files.
        # PCM16 exact endpoint saturation after RESUME; A/B timing stays valid.
        bad = bytearray(bytes(struct.pack("<hh", 1200, -900)) * NFRAMES)
        struct.pack_into("<h", bad, (RESUME + 2048) * 4, 32767)
        write_wav(folder / "hq-default.wav", bad)
        run(folder, False)
        summary = json.loads((folder / "analysis.json").read_text())
        assert summary["audition_audio_gate"] == "FAIL_CLIPPING"
        assert summary["clip_warning"] is True
        assert not any(folder.glob("*-listen.wav"))

        # 30,100 does not clip PCM16 but lacks the required <30k headroom.
        struct.pack_into("<h", bad, (RESUME + 2048) * 4, 30100)
        write_wav(folder / "hq-default.wav", bad)
        run(folder, False)
        summary = json.loads((folder / "analysis.json").read_text())
        assert summary["audition_audio_gate"] == "FAIL_HEADROOM"
        assert summary["clip_warning"] is False
        assert summary["headroom_warning"] is True
        assert not any(folder.glob("*-listen.wav"))


if __name__ == "__main__":
    test_headroom_gate()
    print("STASIS join WAV gate: clean PASS, clipping FAIL, "
          "insufficient-headroom FAIL, stale listening WAV purge PASS")
