#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""V1+V3 factory sample policy, tested with deterministic synthetic PCM sources.

This checks actual gen_samples.Builder behavior without requiring WAV assets
or the JieLi SDK. Real audio, target bytes, and golden renders need separate
verification in the full source checkout.
"""
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import gen_samples as g  # noqa: E402


def main():
    original = g.cc0_entries
    roots = {"PIANO": (36, 48, 60, 72, 84), "FLUTE": (72, 84, 96)}

    def fake(name, kind):
        assert kind in ("oneshot", "sus")
        return [(f"{name}_{root}", [((i * 37 + root * 19) % 16000) - 8000 for i in range(64)],
                 (8, 63) if kind == "sus" else None, root)
                for root in roots[name]]

    g.cc0_entries = fake
    try:
        factory = g.Builder()
        for name, kind in g.CC0_SETS:
            if kind == "alias":
                factory.alias_set(name)
            else:
                factory.cc0_set(name, kind)
        assert [s[0] for s in factory.sets] == ["PIANO", "PIANO", "FLUTE", "PIANO", "PIANO"]
        assert len(factory.sets) == 5
        assert [s[2] for s in factory.sets] == [3, 3, 2, 3, 3]
        assert factory.alias == {1: 0, 3: 0, 4: 0}
        piano = factory.zones[:3]
        flute = factory.zones[3:]
        assert [z["root16"] // 16 for z in piano] == [36, 60, 84]
        assert [(z["lo"], z["hi"]) for z in piano] == [(0, 48), (49, 72), (73, 127)]
        assert [z["root16"] // 16 for z in flute] == [72, 96]
        assert [(z["lo"], z["hi"]) for z in flute] == [(0, 84), (85, 127)]
        assert all(not z["looped"] for z in piano)
        assert all(z["looped"] and (z["ls"], z["le"]) == (8, 63) for z in flute)
        factory.slice_piano()
        assert factory.pno is not None
        assert factory.pno["off"] == piano[1]["off"]
        assert factory.pno["n"] == piano[1]["n"]
        header = factory.header()
        assert '#define SMP_NSETS 5' in header
        assert '#define SMP_NPRESETS 4' in header
        assert 'static const uint8_t SMP_SET_ORIG[] = {0, 0, 2, 0, 0};' in header
        assert '#define SLC_PIANO_NOTE 60' in header

        # The legacy user export MUST still have all five original piano roots.
        user = g.Builder()
        user.cc0_set("PIANO", "oneshot", factory=False)
        assert [z["root16"] // 16 for z in user.zones] == list(roots["PIANO"])
        assert len(user.blob) == 5 * 32 and flute[0]["off"] == 3 * 32
        # Exercise the actual legacy user-file writer: it must not re-use
        # the factory zone restriction internally.
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "PIANO_HD"
            g.user_slot_file("PIANO", "PIANO HD", str(out))
            assert out.with_suffix(".hdr").is_file()
            assert out.with_suffix(".bin").is_file()
            assert out.with_suffix(".slot").is_file()

        # Fail closed if a source recording disappears.
        roots["PIANO"] = (36, 48, 72, 84)
        try:
            g.Builder().cc0_set("PIANO", "oneshot")
            raise AssertionError("missing middle-C source was silently accepted")
        except ValueError:
            pass
    finally:
        g.cc0_entries = original
    print("sample_zone_policy_test: PASS (factory V1+V3, splits, aliases, SLICE, HD .hdr/.bin/.slot, missing source)")


if __name__ == "__main__":
    main()
