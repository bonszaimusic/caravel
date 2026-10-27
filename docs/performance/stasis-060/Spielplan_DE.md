# Variante 4b — stärkerer und längerer Höhepunkt

Die bewährte erste Hälfte bleibt erhalten. Danach folgen weitere Reglerfahrten einzeln nacheinander. Die Zeiten beziehen sich auf den Start der 30-Sekunden-Hörprobe beziehungsweise der beiliegenden MIDI-Datei. Zwischen den Fahrten stehen die Regler still.

## 1. Grundsetup vor dem Start

Track 1 und 2 bilden das SEM-Paar, Track 3 spielt die Drone. MIDI IN auf CH1–4 stellen, Clock intern auf 90 BPM. Die Noten-Datei sendet nur auf Kanal 1 und Kanal 3: Track 2 bekommt seine Noten über das Pair. Zusätzliche Clock-/Start-/Stop-Sendungen des MIDI-Abspielprogramms ausschalten. Die Datei über einen MIDI-Ausgang auf Caravel leiten; ein normaler Audio-Player spielt die enthaltenen Noten nicht auf dem FM-1.

Das beiliegende HOME_Setup.caravel enthält das vollständige Startsetup des nativen Emulators. Es ist kein als direkt installierbar geprüftes Hardwareprojekt. Für den manuellen Aufbau die folgenden Werte setzen; die nativen Presets sind jeweils Ausgangspunkte.

| Einstellung | Track 1 | Track 2 | Track 3 |
| --- | --- | --- | --- |
| Engine / Ausgangspreset | SEM / WARM PAD | SEM / WARM PAD | DRONE / STILL AIR |
| LVL | −8,0 dB | −8,0 dB, vom Pair gemeinsam geregelt | −6,0 dB |
| VOICE / SPREAD | POLY / 0 | POLY / 0 | Preset beibehalten |
| ATK | 139 ms | 442 ms | 591 ms |
| DEC | WARM-PAD-Wert beibehalten | WARM-PAD-Wert beibehalten | STILL-AIR-Wert beibehalten |
| SUS / REL | 91 % / 591 ms | 91 % / 591 ms | 91 % / 790 ms |
| DIST / CHOR / DLY | 0 / 0 / 0 | 0 / 0 / 0 | 22 % / 0 / 0 |
| REV | 25 % | 19 % | 22 % |
| ENV → FLT / LFO → PIT, FLT, SHP | 0 / alle 0 | 0 / alle 0 | 0 / alle 0 |
| PAN / Track FILTER | Mitte / OFF | Mitte / OFF | rechts 19 % / HP 12 % |

SEM auf beiden Tracks: OSC1 TRI, OSC2 SAW, DTN 6 ct, MIX 44 %, RES 13 %, STATE 0 %, VINT 16 %, Filter TYPE STATE. CUT: Track 1 **1,0 kHz**, Track 2 **206 Hz**. Die Anzeige 1,0 kHz entspricht intern 1055 Hz.

Drone auf Track 3: SPREAD 0 %, DETUNE 9 %, COUPLE 8 %, TOPO PAIRS, CHAR 22 %, DRIFT 6 %, RATE 8 %, TONE 52 %. DIST bleibt durchgehend bei 22 %. So entsteht ein leicht gesättigter, stabiler Ton; kein sich laufend veränderndes TIDE-Netzwerk.

SLICER nur auf Track 1: SLCR GATE, PAT 1 (`x.x.x.x.x.x.x.x.`), RATE 1/8, DEPTH zunächst 0 %. Auf Track 2 und 3 bleibt er aus.

STASIS: FX > REVERB TYPE STASIS, CAPT LIVE, STR **8 %**, MOTN **9 %**. FX > LAYER schon vor dem Start auf MODE BLEND und TIME **3 s** vorbereiten. AMT bleibt für BLEND ohne Wirkung. Danach FX > REVERB als zuletzt geöffnete FX-Seite hinterlassen, bevor HOME geöffnet wird.

PAIR: Partner Track 2, PAIR ON, MORPH **100 % Track 2**, MOTION OFF. LATCH und ARP bleiben aus. MASTER im Render: 900/1023, ungefähr 88 % des Potentiometerbereichs. Eine konstante Export-Lautstärkeanhebung ist separat in der Cue-Datei dokumentiert.

## 2. HOME-Reihen

| Reihe | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| 1 | Pair MORPH | Track 3 LVL | Track 2 CUT | Paar-LVL |
| 2 | Track 1 SLCR | PAT | RATE | DEPTH |
| 3 | Track 1 CUT | Track 2 REV | Track 1 REV | Track 3 DIST |

Die ausführliche Einrichtung über Quellenregler, GLO+HOME und EDIT CELLS steht in Caravel_HOME_Performance_Bank.md. SELECT wechselt die Reihe. Während der vier Fahrten bleibt Track 1 ausgewählt.

## 3. Das wird wann gedreht

Bis einschließlich Sekunde 21 ist der Klang gegenüber Variante 4 unverändert. Derselbe konstante Exportpegel wird verwendet. Die neue zweite Hälfte steht darunter; der MASTER-Anstieg ist Teil der tatsächlichen nativen Performance.

| Zeit | Seite / Regler | Start → Ziel |
| --- | --- | --- |
| 10–13 s | HOME Reihe 1, K1 MORPH | 100 → 35 % Track 2 |
| 13,5–15 s | HOME Reihe 2, K4 DEPTH | 0 → 44 % |
| 15,5–17 s | FX REVERB, K3 STR | 8 → 47 % |
| 17,2 s | FX REVERB, K2 CAPT | LIVE → HOLD |
| 18–19,2 s | HOME Reihe 3, K1 CUT | 1,0 kHz → 322 Hz |
| 20 s | FX LAYER: BLEND, TIME 3 s | K4 nach rechts, dann OCT+ |
| **21,1–22,1 s** | **HOME Reihe 3, K1 CUT** | **322 Hz → 1,9 kHz** |
| **22,4–23,3 s** | **HOME Reihe 2, K4 DEPTH** | **44 → 25 %**, weniger starkes Zerschneiden |
| **23,6–24,5 s** | **FX REVERB, K3 STR** | **47 → 63 %** |
| **24,8–25,6 s** | Physischer **MASTER** | ungefähr **88 % → 100 %** Potentiometerstellung, etwa +2,2 dB vor dem nativen Master-Limiter |
| **27,7 s** | **PLAY/STOP** | STASIS freigeben, native Hallfahne |
| **28,8–29,8 s** | Physischer **MASTER** | **100 % → ganz zu**, kontrollierter Abschluss |

Die Bewegungen laufen nacheinander; jeweils weich anfangen und weich am Ziel ankommen. Es gibt keine zeitlich überlappenden Reglerfahrten. Die Zahlen zum MASTER beschreiben die physische Potentiometerstellung, nicht eine auf HOME angezeigte Prozentzahl. Bei 24,8 s sind alle anderen Fahrten beendet; bis zum letzten MASTER-Ausblenden nichts zusätzlich verändern.

## 4. Notenwiedergabe

**Caravel_060_V4b_Notes_90BPM.mid** enthält nur die Noten auf Kanal 1 und 3 sowie Textmarken und Tempo. Keine CC- oder SysEx-Befehle: Regler, HOLD, BLEND und PLAY/STOP werden selbst bedient. MIDI-Nummern sind maßgeblich; Oktavnamen können je nach Software anders erscheinen.

| Track / Kanal | MIDI-Note | Anschlag | Start | Loslassen |
| --- | --- | --- | --- | --- |
| Track 1 / Kanal 1, Track 2 folgt dem Pair | 38 (D) | 92 | 0,2 s | **26,8 s** |
| Track 1 / Kanal 1 | 45 (A) | 80 | 3,8 s | **26,4 s** |
| Track 1 / Kanal 1 | 53 (F) | 86 | 15,2 s | 20,8 s |
| Track 1 / Kanal 1 | **53 (F), erneuter Einsatz** | **100** | **21,7 s** | **26,0 s** |
| Track 3 / Kanal 3 | 50 (D, Drone) | 100 | 6,5 s | **27,0 s** |

Nach dem kurzen Absetzen der Terz tritt F bei 21,7 s kräftiger wieder ein. Die Quinte, der Bass und die Drone bleiben jetzt länger stehen. Anschließend werden die Noten nacheinander losgelassen; erst danach wird STASIS beendet. Die native MASTER-Bewegung am Schluss verhindert einen abrupt abgeschnittenen Dateischluss.

## 5. Dateien und Grenzen

Das HOME-Startsetup ist unverändert zur Variante 4. Die Aufnahme wurde mit dem originalen Caravel-DSP nativ gerendert, nicht auf einem FM-1 aufgenommen. Der manuelle Spielplan und MIDI-Ablauf wurden noch nicht an Hardware getestet. Die genaue Renderquelle und die Zeitdaten stehen im Paket; eine Handbewegung muss nicht die digitale S-Kurve samplegenau treffen.
