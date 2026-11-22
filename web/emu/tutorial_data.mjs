// SPDX-License-Identifier: GPL-3.0-only
// GENERATED public Academy data; never edit. Source is in the private build.
// Public release stage count: 2
export const TOUR_AMBIENT_STAGES = [
  {
    "title": "Strings before anything else",
    "steps": [
      {
        "layer": "BUILD",
        "target": "HOME",
        "title": "Start in BUILD",
        "instruction": "Power on the emulator. The first input may dismiss TWO LEVELS help. Confirm BUILD (hold HOME 1 second only if the screen currently shows PLAY).",
        "caption": "BUILD prepares your ambient sound",
        "actions": [
          {
            "do": "ensure-layer",
            "control": "HOME",
            "seconds": 1.1,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "BUILD",
          "track": 0,
          "firmwareObserved": false
        },
        "id": "step-1-1",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "ALGORITHM",
        "title": "Select T1",
        "instruction": "Turn ALGORITHM until T1 is selected. T1 will carry the stereo string layer.",
        "caption": "T1 is our string bed",
        "actions": [
          {
            "do": "turn",
            "control": "ALGORITHM",
            "steps": null,
            "seconds": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "T1",
          "track": 0,
          "firmwareObserved": false
        },
        "id": "step-1-2",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "EDIT",
        "title": "Open the EDIT selector",
        "instruction": "Hold EDIT for its SET layer. With mouse: Shift-click EDIT to latch it; Shift-click again to release. On touch, hold EDIT with one finger and turn the knob with another.",
        "caption": "Choose a rich engine in EDIT",
        "actions": [
          {
            "do": "hold",
            "control": "EDIT",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "EDIT",
          "track": 0,
          "firmwareObserved": false
        },
        "id": "step-1-3",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "KNOB 1",
        "title": "Select ENSEMBLE (ENSEMB)",
        "instruction": "While EDIT is held, turn K1 ENG until ENSEMB/ENSEMBLE is shown. This is the wide string engine; do not stay on the dry ANALOG ACID sound.",
        "caption": "ENSEMBLE makes the sound wide",
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 1",
            "steps": null,
            "seconds": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "ENSEMB",
          "track": 0,
          "firmwareObserved": false,
          "engine": 17
        },
        "id": "step-1-4",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "KNOB 2",
        "title": "Load preset 6 STRING MACHINE",
        "instruction": "While EDIT is held, turn K2 (sound number) to preset 6, STRING MACHINE, using 1-based numbering. Read the LCD; then release EDIT (Shift-click again if latched). If ENSEMBLE preset 1 is the starting point, K2 takes five increments to preset 6; otherwise use the LCD number.",
        "caption": "Preset 6 is STRING MACHINE",
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 2",
            "steps": 5,
            "seconds": null,
            "verified": true,
            "conditionalStart": "Only when preset 1 of ENSEMBLE is selected before turning K2",
            "evidence": "Local KI on real virtual firmware, 2026-11-21 PR #80 review #7061906632: K2 five increments from first preset to preset 6"
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "STRING MACHINE",
          "track": 0,
          "firmwareObserved": false,
          "engine": 17
        },
        "id": "step-1-5",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-6",
        "stage": 0,
        "layer": "BUILD",
        "target": "EDIT",
        "title": "Open BUILD EDIT 1",
        "instruction": "Release EDIT if held (Shift-click EDIT again if latched). Tap EDIT to open BUILD / EDIT 1 and check the page title exactly says EDIT 1. Confirm the cards REG / MORPH / TONE / DRIFT; K3 is TONE only on this page. If EDIT 2, VOICE or VOICE 2 appears, tap EDIT until EDIT 1 returns.",
        "caption": "Find ENSEMBLE's TONE control",
        "actions": [
          {
            "do": "press",
            "control": "EDIT",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD page title EDIT 1; K3 card TONE"
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "EDIT 1 · REG MORPH TONE DRIFT",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "EDIT 1"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-7",
        "stage": 0,
        "layer": "BUILD",
        "target": "KNOB 3",
        "title": "Warm the strings with TONE",
        "instruction": "On BUILD / EDIT 1 (REG / MORPH / TONE / DRIFT), the factory STRING MACHINE shows TONE 100 %. Turn K3 down until the LCD reads about TONE 66 %. If it already reads 66 % after the optional warm starter, do not lower it again. The local KI observed roughly 36–43 K3 detents from 100 % to 66 %, depending on rotation speed; stop by the LCD, not by counting detents.",
        "caption": "Lower TONE for warmer strings",
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 3",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD TONE about 66 %",
            "startValue": 127,
            "targetValue": 84,
            "sourceDerived": true,
            "derivedValueDelta": -43,
            "observedDetents": {
              "min": 36,
              "max": 43,
              "setup": "real Caravel 1.1 virtual firmware, factory STRING MACHINE TONE 100% to ~66%",
              "source": "PR #80 local KI review #7062464852",
              "accelerationDependent": true
            }
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "TONE ~66 %",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "EDIT 1"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-8",
        "stage": 0,
        "layer": "BUILD",
        "target": "FX",
        "title": "Find persistent track FILTER",
        "instruction": "In BUILD tap FX to enter the FX family, then tap FX through the pages until the exact title INSERT 2 appears. In the reviewed 1.1 build it is FX page 9 of 10: from the FX/SENDS page, eight further FX taps reach INSERT 2. K4's card must say FILT. Tap FX individually, allowing about 0.34 s between presses: faster taps (0.12–0.25 s) trigger the FX double-tap behavior instead of the next page. Stop when INSERT 2 is visible; do not confuse it with PLAY SPACE / FILTER.",
        "caption": "Find INSERT 2's track filter",
        "actions": [
          {
            "do": "press",
            "control": "FX",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "BUILD page INSERT 2; K4 card FILT",
            "observedNavigation": {
              "pagesFromFx": 8,
              "pageOrdinal": 9,
              "total": 10,
              "source": "PR #80 local KI 1.1 WASM review #7062464852"
            }
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "INSERT 2 · K4 FILT",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "INSERT 2"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-9",
        "stage": 0,
        "layer": "BUILD",
        "target": "KNOB 4",
        "title": "Set a dark low-pass cutoff",
        "instruction": "On BUILD FX / INSERT 2 turn K4 FILT left from OFF until the LCD reads 50 LP (LP 50 %). Source documentation places this low-pass around 1.4 kHz. The local KI observed roughly 27–32 K4 detents from OFF depending on acceleration; the displayed 50 LP is the destination, not a fixed detent count.",
        "caption": "Start with a gentle low-pass filter",
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 4",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD 50 LP",
            "startValue": 0,
            "targetValue": -32,
            "derivedValueDelta": -32,
            "sourceDerived": true,
            "observedDetents": {
              "min": 27,
              "max": 32,
              "setup": "real Caravel 1.1 virtual firmware, FILT OFF to 50 LP",
              "source": "PR #80 local KI review #7062464852",
              "accelerationDependent": true
            }
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "50 LP",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "INSERT 2"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "FX",
        "title": "Open a reverb send early",
        "instruction": "From BUILD FX / INSERT 2, tap FX twice: SLICER, then FX (the SENDS page). Confirm page title FX and that K4 is REV. Press FX once, pause about 0.34 s, then press again. Two very fast taps may toggle the last page instead of advancing; check the LCD title FX and K4 REV. This is sound preparation, not PLAY SPACE.",
        "caption": "Open a little room around the strings",
        "actions": [
          {
            "do": "press",
            "control": "FX",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "BUILD FX / SENDS page; K4 REV",
            "observedNavigation": {
              "pagesFromInsert2": 2,
              "source": "PR #80 local KI 1.1 WASM review #7062464852"
            }
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "FX · K4 REV",
          "track": 0,
          "firmwareObserved": false,
          "engine": 17
        },
        "id": "step-1-10",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "KNOB 4",
        "title": "Raise REV gently",
        "instruction": "On BUILD FX SENDS, K4 is REV. Turn it slightly up from the preset value, checking the LCD so the tail remains spacious rather than washed out.",
        "caption": "Reverb gives the strings depth",
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 4",
            "steps": null,
            "seconds": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "REV",
          "track": 0,
          "firmwareObserved": false,
          "engine": 17
        },
        "id": "step-1-11",
        "stage": 0,
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-env-open",
        "stage": 0,
        "layer": "BUILD",
        "title": "Open the slow-swell envelope",
        "target": "ENV",
        "instruction": "After setting REV, tap ENV in BUILD. Confirm that the BUILD page title reads ENV (the corresponding PLAY area is SHAPE). K1 must be ATK; if a different ENV page is visible, tap ENV until the ADSR page returns.",
        "caption": "Open the string attack envelope",
        "actions": [
          {
            "do": "press",
            "control": "ENV",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "BUILD page ENV, K1 ATK"
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "ENV · ATK",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "ENV"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-env-attack",
        "stage": 0,
        "layer": "BUILD",
        "title": "Make the strings rise slowly",
        "target": "KNOB 1",
        "instruction": "On BUILD / ENV turn K1 ATK right from the factory 476 ms until the LCD reads about 2.71 s. This is a slow swell before the first G–D chord. The local KI observed about 24 evenly spaced detents (0.12 s between turns), but encoder acceleration varies: use the LCD. If the optional warm starter already shows 2.71 s, leave it unchanged. Keep REL at its factory value for now; Stage 3 explores the release.",
        "caption": "Let the strings rise over two seconds",
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 1",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD ATK 2.71 s",
            "startValue": 85,
            "targetValue": 109,
            "sourceDerived": true,
            "observedDetents": {
              "approx": 24,
              "pattern": "0.12 s between uniform turns, starting from ATK 476 ms",
              "variableWithAcceleration": true,
              "source": "Local KI real virtual firmware measurement: PR #80 comment #7062922907"
            }
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "ATK 2.71 s",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "ENV"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "G3",
        "title": "Play an open fifth",
        "instruction": "Hold G3 + D4 together for at least 3.5–4 seconds so the new 2.71 s attack has time to bloom (G3 is computer X). Then release both and listen to the reverb tail. Check TONE around 66%, FILT 50 LP and ATK 2.71 s were prepared earlier. A single quick tap will not demonstrate the intended slow swell.",
        "caption": "Hear the G–D chord slowly bloom",
        "actions": [
          {
            "do": "hold",
            "control": "G3",
            "steps": null,
            "seconds": 4,
            "verified": false
          },
          {
            "do": "hold",
            "control": "D4",
            "steps": null,
            "seconds": 4,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "ENSEMB",
          "track": 0,
          "firmwareObserved": false,
          "engine": 17
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        },
        "id": "step-1-12",
        "stage": 0
      },
      {
        "id": "step-1-13",
        "stage": 0,
        "layer": "BUILD",
        "target": "FX",
        "title": "Return to INSERT 2",
        "instruction": "From BUILD / ENV, tap FX to return to the FX family and continue until the title INSERT 2 appears; K4 must say FILT, about 50 LP. If the first FX page is FX/SENDS, INSERT 2 is eight more FX taps. A saved last FX page may shorten the route; the LCD page title is decisive. Leave about 0.34 s between FX taps to avoid the firmware double-tap shortcut (verified locally on the real 1.1 WASM).",
        "caption": "Return to the filtered sound controls",
        "actions": [
          {
            "do": "press",
            "control": "FX",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "BUILD INSERT 2 page with K4 FILT",
            "observedNavigation": {
              "pagesFromFx": 8,
              "conditionalStart": "Only when opening at initial FX/SENDS",
              "source": "PR #80 local KI 1.1 WASM review #7062464852"
            }
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "INSERT 2 · K4 FILT",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "INSERT 2"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "id": "step-1-14",
        "stage": 0,
        "layer": "BUILD",
        "target": "KNOB 4",
        "title": "Open and close the cutoff",
        "instruction": "For a stronger demonstration, hold the G–D fifth one octave higher as G4 + D5 (mouse: Shift-click G4 and D5 to latch both, then Shift-click each again to release; touch: hold both). Wait for the 2.71 s attack. In BUILD / INSERT 2 turn K4 FILT from its starting 50 LP to 75 LP, then all the way to OFF, then return to 50 LP for the warm sound. Confirm each LCD destination; do not count detents. At 75 LP, the sound becomes quieter and darker; with the filter OFF, it opens up and sounds brighter. Return to 50 LP to restore warmth.",
        "caption": "Hear the cutoff open across the octave",
        "actions": [
          {
            "do": "hold",
            "control": "G4",
            "steps": null,
            "seconds": 5.5,
            "verified": false
          },
          {
            "do": "hold",
            "control": "D5",
            "steps": null,
            "seconds": 5.5,
            "verified": false
          },
          {
            "do": "turn",
            "control": "KNOB 4",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD FILT 75 LP",
            "startValue": -32,
            "targetValue": -48,
            "sourceDerived": true
          },
          {
            "do": "turn",
            "control": "KNOB 4",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD FILT OFF",
            "startValue": -48,
            "targetValue": 0,
            "sourceDerived": true
          },
          {
            "do": "turn",
            "control": "KNOB 4",
            "steps": null,
            "seconds": null,
            "verified": false,
            "until": "LCD FILT 50 LP",
            "startValue": 0,
            "targetValue": -32,
            "sourceDerived": true
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "75 LP → OFF → 50 LP",
          "track": 0,
          "engine": 17,
          "firmwareObserved": false,
          "pageTitle": "INSERT 2"
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        },
        "check": "The LCD has shown 75 LP → OFF → 50 LP while G4+D5 is sustained after the slow 2.71 s swell. Listen for the cutoff opening; comparative spectral results on these higher notes are not yet measured. End with 50 LP so the next stage inherits the warm filter."
      }
    ],
    "id": "stage-1",
    "hook": "Warm G–D strings swell over 2.7 seconds before their filter opens.",
    "result": "A sustained G3–D4 string fifth slowly swells; a wide cutoff sweep reveals its brighter side.",
    "startState": {
      "mode": "manual",
      "description": "BUILD T1 ENSEMBLE 6 STRING MACHINE (G-minor). Before the first G3+D4 chord, prepare TONE LCD about 66% (raw 84), persistent FILT 50 LP (raw -32), REV send and ENV ATK 2.71 s (raw 109; STRING MACHINE factory 476 ms, raw 85). The user-consented RAM starter applies all three warm/swelling values but does not play notes or restore SEQ/HARMONY.",
      "automaticRestore": false,
      "projectSlot": null
    },
    "estimatedMinutes": {
      "min": 4,
      "max": 7,
      "basis": "editorial estimate, not timed on actual WASM",
      "verified": false
    }
  },
  {
    "title": "Lay CELL AIR underneath",
    "steps": [
      {
        "layer": "BUILD",
        "target": "ALGORITHM",
        "title": "Select T2",
        "instruction": "Select T2 using ALGORITHM. Leave the ENSEMBLE 6 string bed on T1.",
        "caption": "Add a second track T2",
        "id": "step-2-1",
        "stage": 1,
        "actions": [
          {
            "do": "turn",
            "control": "ALGORITHM",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": null,
          "track": 1,
          "firmwareObserved": false
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "EDIT",
        "title": "Hold EDIT to choose DRONE",
        "instruction": "Hold EDIT (Shift-click to latch it with mouse). The EDIT selector has ENG on K1 and preset number on K2.",
        "caption": "T2 becomes a moving drone",
        "actions": [
          {
            "do": "hold",
            "control": "EDIT",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "id": "step-2-2",
        "stage": 1,
        "expected": {
          "layer": "BUILD",
          "lcdText": null,
          "track": 1,
          "firmwareObserved": false
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "KNOB 1",
        "title": "Set engine DRONE",
        "instruction": "With EDIT held, turn K1 ENG until DRONE is displayed.",
        "caption": "DRONE moves beneath the strings",
        "id": "step-2-3",
        "stage": 1,
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 1",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": null,
          "track": 1,
          "firmwareObserved": false,
          "engine": 16
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "KNOB 2",
        "title": "Choose 6 CELL AIR",
        "instruction": "Turn K2 to factory preset 6 CELL AIR (1-based number), verify the LCD, and release EDIT (Shift-click to unlatch on mouse).",
        "caption": "Preset 6 is CELL AIR",
        "id": "step-2-4",
        "stage": 1,
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 2",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "CELL AIR",
          "track": 1,
          "firmwareObserved": false,
          "engine": 16
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "FX",
        "title": "Open FX SENDS on T2",
        "instruction": "Tap FX in BUILD to find the SENDS group for T2. Unlike ENSEMBLE, DRONE dry output can feel mono; provide space with reverb.",
        "caption": "Give CELL AIR a shared room",
        "id": "step-2-5",
        "stage": 1,
        "actions": [
          {
            "do": "press",
            "control": "FX",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": null,
          "track": 1,
          "firmwareObserved": false,
          "engine": 16
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "KNOB 4",
        "title": "Add REV to CELL AIR",
        "instruction": "Increase K4 REV on T2 by a moderate amount; monitor the actual LCD value and overall level.",
        "caption": "The drone now has a long tail",
        "id": "step-2-6",
        "stage": 1,
        "actions": [
          {
            "do": "turn",
            "control": "KNOB 4",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": "CELL AIR",
          "track": 1,
          "firmwareObserved": false,
          "engine": 16
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      },
      {
        "layer": "BUILD",
        "target": "G3",
        "title": "Listen to the foundation",
        "instruction": "Hold G3 and D4 for an open fifth and compare to the ENSEMBLE layer. T2 should sit beneath T1 rather than overwhelm it.",
        "caption": "CELL AIR underpins the string pad",
        "check": "You hear a textured CELL AIR drone with a spatial tail. T1 remains ENSEMBLE 6 STRING MACHINE.",
        "id": "step-2-7",
        "stage": 1,
        "actions": [
          {
            "do": "press",
            "control": "G3",
            "seconds": null,
            "steps": null,
            "verified": false
          }
        ],
        "expected": {
          "layer": "BUILD",
          "lcdText": null,
          "track": 1,
          "firmwareObserved": false,
          "engine": 16
        },
        "capabilities": {
          "web": true,
          "fm1": "guided-control-reference",
          "videoSoundSource": "caravel-web-emulator"
        }
      }
    ],
    "id": "stage-2",
    "hook": "CELL AIR breathes beneath the warm G–D strings.",
    "result": "CELL AIR moves beneath the strings.",
    "startState": {
      "mode": "manual",
      "description": "BUILD. Select T2 and load DRONE 6 CELL AIR; if coming from a fresh emulator, first load T1 ENSEMBLE 6 STRING MACHINE for context.",
      "automaticRestore": false,
      "projectSlot": null
    },
    "estimatedMinutes": {
      "min": 2,
      "max": 4,
      "basis": "editorial estimate, not timed on actual WASM",
      "verified": false
    }
  }
];
export const TOUR_LESSONS = [];
export const TOUR_PRESETS = [
  {
    "engine": "ENSEMB",
    "number": 6,
    "name": "STRING MACHINE",
    "track": 0,
    "engineIndex": 17
  },
  {
    "engine": "DRONE",
    "number": 6,
    "name": "CELL AIR",
    "track": 1,
    "engineIndex": 16
  }
];
