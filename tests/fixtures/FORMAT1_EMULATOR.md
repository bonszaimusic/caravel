# Frozen FORMAT 1 regression fixture

GPL-3.0-only, same source and license notices as Caravel. Used only by the 36 old/new storage-guard checks;
not a release image or an FM-1 firmware package. The new basis never rebuilds this old profile.
Produced locally on 2026-11-15 during F2-e, before the owner retired the legacy build (E85).
Effective legacy code path corresponds to `aac1bc2ddd589f5d4526be3900ec1d8111ef17ac`;
FORMAT-2-only work was still in progress in the working tree, so this is not an exact-source release receipt.
Source identity is the commit above; it is not a published release or an exact-source receipt for this fixture.
Build: Emscripten from caravel-validate:9e8a33d4f5c9, web/emu/build.sh,
CARAVEL_SLIM=1, FORMAT2=0, UX_LAYERS=1, UX_CAPTURE=1, STASIS_EXPAND=1,
REVERB_MINIMAL=1, RANDOM_MORPH=1, UX_EVOLVE=1, ELASTIC_P1=1, ELASTIC_PLAYABLE=1.
The guard test itself verifies absence of the FORMAT 2 marker and actual legacy save/load behavior.

Uncompressed SHA-256: `6bf7289b9b45b4b4b7bac949dc147807058bcdb22316d09fe8e26fc147c974d4`
Compressed SHA-256: `742b99efaca02182048c8ca49e68a3a27e9e1bbffa82f696769f489b52f8ab01`
