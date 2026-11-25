#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate six or nine deterministic real-FX WAVs at one matched gain.

This is an *audition aid*, NOT a blind listening verdict, FM-1 DSP timing
measurement, or replacement for the 48-case A4 pre-limiter continuity gate.
Generated files remain under the private build directory.
Usage: python3 tools/stasis2_join_audio_compare.py build/stasis-v5/join-audition
"""
import array
import json
import math
import pathlib
import sys
import wave

FS = 44100
CTL = 32
RESUME_FRAME = (FS * 8 + FS // 7) // CTL * CTL
NAMES = ("hq", "long", "drone")


def load_wav(path):
    with wave.open(str(path), "rb") as w:
        props = (w.getnchannels(), w.getsampwidth(), w.getframerate(),
                 w.getcomptype())
        if props != (2, 2, FS, "NONE"):
            raise ValueError(f"{path}: expected stereo PCM16 44100Hz, got {props}")
        n = w.getnframes()
        raw = w.readframes(n)
    if len(raw) != n * 4:
        raise ValueError(f"{path}: truncated PCM data")
    samples = array.array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()
    return n, raw, samples


def save_wav(path, samples, rate):
    out = array.array("h", (max(-32768, min(32767, int(round(s * rate))))
                             for s in samples))
    if sys.byteorder != "little":
        out.byteswap()
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(FS)
        w.writeframes(out.tobytes())


def stats(samples):
    peak = max(abs(v) for v in samples)
    rms = math.sqrt(sum(int(v) * int(v) for v in samples) / len(samples))
    clipped = sum(1 for v in samples if abs(v) >= 32767)
    return {"peak_pcm16": peak, "rms_pcm16": round(rms, 3),
            "clipped_samples": clipped,
            "rms_dbfs": round(20.0 * math.log10(max(rms, 1e-12) / 32768), 2)}


def main(folder):
    folder = pathlib.Path(folder)
    # Clear only generated outputs, even for direct analyzer invocations:
    # a failed new run must never leave OLD valid-looking listening WAVs.
    (folder / "analysis.json").unlink(missing_ok=True)
    for name in NAMES:
        for version in ("default", "joined", "lut"):
            (folder / f"{name}-{version}-listen.wav").unlink(missing_ok=True)
    pairs = {}
    peaks = []
    lut_present = any((folder / f"{name}-lut.wav").exists()
                      for name in NAMES)
    if lut_present and not all((folder / f"{name}-lut.wav").exists()
                               for name in NAMES):
        raise ValueError("incomplete LUT audition triplet")
    report = {
        "purpose": "A/B audition of actual host STASIS FX (not FM-1)",
        "sample_rate": FS, "resume_frame": RESUME_FRAME,
        "requires_human_listening": True,
        "automated_audio_quality_verdict": "NOT_ASSESSED",
        "quality": {},
    }
    for name in NAMES:
        p0 = folder / f"{name}-default.wav"
        p1 = folder / f"{name}-joined.wav"
        n0, raw0, a = load_wav(p0)
        n1, raw1, b = load_wav(p1)
        if n0 != n1 or n0 < FS * 9:
            raise ValueError(f"{name}: mismatched or too-short recordings")
        if raw0[:RESUME_FRAME * 4] != raw1[:RESUME_FRAME * 4]:
            raise ValueError(f"{name}: join unexpectedly alters audio before RESUME")
        first = next((i // 2 for i, (x, y) in enumerate(zip(a, b)) if x != y), None)
        if name == "hq" and (first is None or first < RESUME_FRAME):
            raise ValueError("HQ: source join did not change audio after RESUME")
        if name != "hq" and first is not None:
            raise ValueError(f"{name}: HQ-only join changed another quality!")
        s0, s1 = stats(a), stats(b)
        diff = max(abs(int(x) - int(y)) for x, y in zip(a, b))
        report["quality"][name] = {
            "default": s0, "joined": s1,
            "before_resume_byteidentical": True,
            "first_changed_frame": first,
            "first_changed_seconds": round(first / FS, 6) if first is not None else None,
            "maximum_output_sample_delta": diff,
            "note": "These PCM16 metrics are not the pre-limiter A4 threshold",
        }
        changed_text = f"{first / FS:.3f}s" if first is not None else "none (HQ-only expected)"
        print(f"{name}: before RESUME identical, first FX output change "
              f"at {changed_text}, max PCM16 delta {diff}, "
              f"RMS {s0['rms_dbfs']}/{s1['rms_dbfs']} dBFS, "
              f"clips {s0['clipped_samples']}/{s1['clipped_samples']}")
        pairs[name] = [("default", a), ("joined", b)]
        peaks.extend([s0["peak_pcm16"], s1["peak_pcm16"]])
        if lut_present:
            n2, raw2, c = load_wav(folder / f"{name}-lut.wav")
            if n2 != n0 or raw0[:RESUME_FRAME * 4] != raw2[:RESUME_FRAME * 4]:
                raise ValueError(f"{name}: LUT changes audio before RESUME")
            if name != "hq" and raw1 != raw2:
                raise ValueError(f"{name}: LUT changes non-HQ output")
            diff2 = max(abs(int(x) - int(y)) for x, y in zip(b, c))
            rms2 = math.sqrt(sum((int(x) - int(y)) ** 2
                                 for x, y in zip(b, c)) / len(b))
            first2 = next((i // 2 for i, (x, y) in enumerate(zip(b, c))
                           if x != y), None)
            report["quality"][name]["lut"] = stats(c)
            report["quality"][name]["lut_vs_cubic_pcm16"] = {
                "max_absolute_delta": diff2,
                "rms_delta": round(rms2, 4),
                "first_changed_frame": first2,
            }
            print(f"{name}: cubic vs LUT max PCM16 delta {diff2}, "
                  f"RMS delta {rms2:.4f}, first frame {first2}")
            pairs[name].append(("lut", c))
            peaks.append(stats(c)["peak_pcm16"])
    peak = max(peaks)
    if peak == 0:
        raise ValueError("all recordings are silent")
    # A warning is insufficient: clipped raw renders can hide the original
    # source seam and make the matched-gain WAVs misleading. Fail CLOSED.
    versions = ("default", "joined", "lut") if lut_present else ("default", "joined")
    report["clip_warning"] = any(
        report["quality"][name][key]["clipped_samples"] > 0
        for name in NAMES for key in versions
    )
    report["headroom_warning"] = peak > 30000
    report["max_raw_pcm16_peak"] = peak
    report["audition_audio_gate"] = (
        "FAIL_CLIPPING" if report["clip_warning"]
        else "FAIL_HEADROOM" if report["headroom_warning"] else "PASS"
    )
    # Report actual failure metrics before exiting; no "listen" files may
    # be created from saturated raw data, regardless of normalization.
    if report["audition_audio_gate"] != "PASS":
        (folder / "analysis.json").write_text(
            json.dumps(report, indent=2) + "\n")
        raise ValueError(
            f"audition gate {report['audition_audio_gate']}: peak={peak}, "
            "lower STASIS_AB_INPUT_DIV stimulus gain by increasing divisor "
            "(e.g. from 16 to 32); rerun ALL variants")
    # One common listening gain for ALL 6/9 WAVs: never normalize A/B
    # separately, which could hide a level change caused by a DSP candidate.
    rate = min(20.0, 30000.0 / peak)
    report["common_listening_gain"] = round(rate, 8)
    for name, captures in pairs.items():
        for label, samples in captures:
            save_wav(folder / f"{name}-{label}-listen.wav", samples, rate)
    (folder / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"Pairwise-matched, raw-headroom-checked WAVs and analysis.json: {folder}")
    print("PASS: WAV format, before-RESUME identity, HQ-only post-RESUME difference"
          + (", LUT-vs-cubic reported" if lut_present else "") + ". "
          "No audible quality or FM-1 acceptance is implied.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: stasis2_join_audio_compare.py FOLDER")
    try:
        raise SystemExit(main(sys.argv[1]))
    except (ValueError, FileNotFoundError, wave.Error) as e:
        print(f"STASIS A/B audition invalid: {e}", file=sys.stderr)
        raise SystemExit(1)
