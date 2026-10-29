# Caravel 0.7.0-rc6 — FM-1 Testversion

Diese Vorabversion enthält echte, installierbare FM-1-Firmware. Sie ist für den
angeforderten Gerätetest gedacht; ein Hör-, Last- und Installationstest am
physischen FM-1 steht noch aus. Der stabile 0.6.0-Release wird nicht ersetzt.

## Gegenüber rc5: Audioschleifen im Code-RAM

Der Gerätetest zeigt auch mit rc5 weiter Knarzen. Die neue Messung meldet
3576 überschrittene Audiofristen in 30 Sekunden, 92,0% mittlere Last und
108,1% Spitze; CPU2 läuft ohne Fehler. TIDE und CELL RNG knarzen laut
Rückmeldung gleichermaßen. Der Patch unterscheidet sich zwischen den
Aufzeichnungen, deshalb ist dies kein kontrollierter Versionsvergleich.

rc6 testet einen anderen Ansatz: Die rechenintensiven DRONE-, SEM-, HADES-,
STASIS- und HALL-Schleifen werden aus dem vorhandenen ausführbaren RAM
statt aus dem Flash ausgeführt. Das soll Flash-/Cache-Zugriffe auf dem
kritischen Audiopfad reduzieren. Ein tatsächlicher CPU-Gewinn ist erst am
FM-1 messbar; rc6 ist noch kein bestätigter Knarz-Fix.

Der Standardbuild belegt dafür rund 13 KiB bisher freien Code-RAM. Die
Audio-/Projekt-Datenbudgets, vier KiB Worker-Stack, Schutzabstände und der
separate Debug-Bereich werden nicht verkleinert. Klangformeln, Sample-Rate,
Anzahl der Zellen/Stimmen und Modulationsraten bleiben unverändert.

Die Platzierung erfolgt nach dem LLVM-Inlining. Aufrufe über die begrenzte
Flash/RAM-Sprungweite verwenden lokale volatile Funktionszeiger, nach dem
Vorbild des vorhandenen Flash-Treibers. Jede aufrufende Funktion verwendet
höchstens einen solchen temporären Stack-Platz. Die unveränderte strenge
Flash-off-Prüfung gilt weiterhin ausschließlich für den Flash-Treiber.

Der Build prüft die tatsächlichen ELF-Grenzen, den vollständigen Code im
Firmwarepaket, die begrenzte Startkopie vor IRQ-Freigabe, die Sichtbarkeitsbarriere,
das PC-Schutzfenster und getrennte Worker-Stacks. Ein nativer ABI-Test vergleicht
direkte und über Zeiger ausgeführte Aufrufketten mit einer Million Schleifen.
Zusätzlich werden RAM-DSP mit einem Kern und die XIP-Variante gebaut, bevor
der eigentliche Release erneut mit den Standardflags gebaut wird.

Bitte nach der Installation unter INFO rc6 prüfen und den bisherigen
Knarzfall mit Pair und ARP spielen. Sobald er wieder auftritt, dieselbe
30-Sekunden-Diagnose aufnehmen. rc5 bleibt als Rückweg verfügbar. Ein
weiterer TIDE-/CELL-RNG-Presetvergleich ist dafür nicht erforderlich.

## Audiolast gegenüber rc4: STASIS und DRONE

Die rc4-Gerätemessung meldet 3796 überschrittene Audiofristen in 30 Sekunden
(58% der gemessenen Abschnitte), 95,1% mittlere Last und 112,9% Spitze.
CPU2 läuft ohne Fehler. Aktiv sind SEM/HADES auf T1/T3, TIDE CELLRING mit
internem ARP und einer gehaltenen Note auf T2, sowie INSERT auf T3.
Das ist ein belegter Überlastungsfall; rc5 ist ein Optimierungskandidat,
noch kein bestätigter Knarz-Fix auf dem echten Gerät.

- STASIS: Der Zielcompiler darf die pro Sample ausgeführten Aufnahme- und
  Grain-Lesehelfer jetzt wie Audiocode optimieren. Die vier Grain-Leser sind
  im Sampleloop eingebettet. Der alte Größenmodus hatte verschachtelte
  Funktionsaufrufe erzeugt; deren Arbeit bleibt erhalten, der Aufrufaufwand fällt weg.
- DRONE CELLS: Bereits exakt erreichte Pitch- und Einblend-Endwerte brauchen
  bis zum nächsten Block keinen erneuten Glättungsschritt. Bewegte Werte
  werden weiterhin für jedes Sample aktualisiert. Der Q23-Follower verwendet
  eine vorzeichenkorrekte Multiplikation ohne zusätzlichen Vorzeichenpfad.
- DRONE PAIRS/CHAIN/RING: Divisionen durch Zweierpotenzen werden exakt mit
  Vorzeichenkorrektur ersetzt; die gemeinsame Drift-Rate wird einmal pro
  Sample statt viermal berechnet. Abklingen und Modulation bleiben unverändert.

Ein eingefrorener rc4-Renderer und die vorhandene CELL-Referenz prüfen PCM
und sämtliche 352 State-Bytes mit zufälligen Blockgrößen und Modulationen.
Der ARP-Lasttest umfasst eine und zwei gehaltene Noten und vergleicht die
serielle mit der automatischen Dual-Core-Ausgabe. Native Laufzeiten und
statische Zielcode-Zählungen sind keine FM-1-Lastmessung.

Die rc5-Optimierungen und RETURN pro Track/Zeile bleiben in rc6 enthalten.
Die aktuelle Anleitung zum Gerätetest steht im Abschnitt zu rc6 oben.

## Korrektur gegenüber rc3: RETURN pro Track/Zeile

RETURN verwendet bei SCOPE TRACK den Ausgangszustand des gewählten Tracks,
bei ROW den der gewählten HOME-Zeile. Nach RANDOM auf T1 und anschließend T2
kann deshalb jeder Track zu seinem eigenen Ausgangszustand zurückfahren.
Trackwechsel allein lenkt eine laufende Bewegung nicht um. Eine neue RANDOM-
oder RETURN-Aktion wechselt den aktiven Bereich; es läuft weiterhin nur eine
Morph-Bewegung gleichzeitig. ALL bleibt ein gemeinsamer Rückweg über alle Tracks.
Manuelle Eingriffe behalten Vorrang, auch wenn der betreffende Track zwischenzeitlich
verlassen wurde. FREEZE auf einem anderen Track/ROW greift nicht in die laufende
Bewegung ein.

Die Rückwege teilen sich die vorhandenen 128 Parameterplätze. Bei voller
Belegung erscheint RETURN MEMORY FULL; andere Rückwege werden nicht verworfen.
Nach abgeschlossenem RETURN sind seine Plätze wieder frei. Wie die bisherige
Random-Session werden die Rückwege durch SCOPE-Wechsel, Snapshot-Store/Recall,
Projektwechsel oder Neustart zurückgesetzt. Die vier S1–S4-Snapshots bleiben
unverändert: GO Sx ruft die beim Speichern festgelegten Tracks ab und ist kein
trackrelativer RETURN. Alle Daten bleiben flüchtig.

Ein zusätzlicher Hosttest umfasst SEM/HADES-Pair T1/T3 plus TIDE mit internem
ARP auf T2 und zwei gehaltenen Noten. ARP-Tempo, Rate und STASIS-Fenster sind
repräsentative Annahmen, weil die bisherige USB-Diagnose sie nicht exportiert.
Die neue rc3-Messung zeigt weiterhin Überlastungen (101,3% Spitze, 7 Audiofristen
in 30 Sekunden), bei einer anderen Engine-/FX-Konfiguration als der erste Test.
rc4 enthält keine zusätzliche DSP-/Lastkorrektur und bestätigt das Knarzen nicht
als behoben.

## Korrektur gegenüber rc2

Der Dual-Core-Planer berücksichtigt neue Stimmen bereits vor ihrem ersten
Audioblock. Bisher nutzte er den Aufwand des vorigen Blocks: Eine neu gespielte
DRONE galt kurz noch als stiller Track und konnte zusammen mit einer SEM auf
einem Kern landen. Die neue Schätzung liest aktive Stimmen nach der
Notenverarbeitung, bevor einer der beiden Kerne mit dem Rendern beginnt.
Kein Engine-, Envelope-, Grain- oder Klangparameter wird dafür verändert.

Der Startfall SEM-Pair T1/T3 plus DRONE T2 ist als Regression erfasst; der Test
scheitert mit rc2. Serial-, automatische und wechselnde Kernaufteilung müssen
identische PCM-Ausgaben liefern. Ein Last-/Hörvergleich auf dem echten FM-1
steht aus: rc3 ist ein gezielter Testkandidat gegen die kurze Startspitze,
kein bestätigter Nachweis, dass der Knarzer vollständig behoben ist.

## Korrektur gegenüber rc1

RANDOM übernimmt bei einem neuen Start den aktuell gewählten Track bzw. die
aktive HOME-Zeile. Ein bereits laufender Morph bleibt an seine ursprünglichen
Ziele gebunden; der Trackwechsel allein lenkt ihn nicht um. Innerhalb desselben
Bereichs bleiben RETURN und der eingefangene Ausgangszustand erhalten.

SEM auf Track 1/3 im Pair plus DRONE auf Track 2 und STASIS HOLD wird als
Dreitrack-Regression geprüft. Das ist kein Nachweis ausreichender Rechenreserve
auf dem FM-1. Das gemeldete Knarzen ist noch nicht als behoben bestätigt.
Für die Lastdiagnose im privaten Android-Installer unter „Knistern und CPU-Last
messen“ einen 10-Sekunden-Lauf mit denselben Presets und gehaltenem STASIS
aufzeichnen, während die zusätzliche DRONE gespielt wird. Das JSON-Protokoll
enthält Audiofristen, verspätete Puffer und Spitzenlast.

## Installation und Datensicherung

1. Projekte und Einstellungen vor dem Update exportieren und die bisherige
   funktionierende Firmware für den Rückweg bereithalten.
2. Das vollständige `Caravel-0.7.0-rc6-FM1-Testpaket.zip` herunterladen und
   entpacken. Am Mac `FM1-Installer.command` öffnen; der Starter benötigt Python 3
   und öffnet den passenden 0.7.0-Installer in Google Chrome. Das Terminal bleibt
   bis zum Abschluss offen. Alternativ im entpackten Ordner ausführen:
   `python3 -m http.server 8070 --bind 127.0.0.1 --directory Installer`
   und in Chrome `http://127.0.0.1:8070/webapp/installer/` öffnen.
   FM-1 per USB anschließen und dort verbinden, sichern und installieren.
   Der enthaltene Installer ist an das geprüfte 0.7.0-Paket gebunden; der
   öffentliche 0.6.0-Installer wird nicht ersetzt. Die einzelne `.fwsc` unter
   `firmware/` enthält App und Update-Loader für alternative Installationswege.
3. Nach dem Neustart unter INFO die Versionsanzeige `v0.7.0-rc6` prüfen.
4. Zunächst mit einem leeren Projekt testen; gewünschte alte Projekte erst
   danach laden. Eine fehlgeschlagene Installation kann den FM-1-Transporter
   zur Wiederherstellung erfordern; Downgrade allein garantiert keine Rettung.

Die unveränderten 0.6.0-Profilgrenzen gelten: HALL/STASIS statt ROOM/SPRING,
SLICE-Sampleengine entfällt, SLICER-Insert bleibt erhalten. Bei einem betroffenen
Altprojekt zeigt der Dialog die Ersetzungen an: CANCEL erhält den Zustand,
TEST lädt eine konvertierte Arbeitskopie. Originale sichern und Testkopien auf
einem anderen Platz speichern. Tempo-DELAY und STASIS teilen ihren Audiopuffer.

## Neu und gezielt zu testen

- FX: FX SENDS/DELAY → CHORUS/REVERB → STASIS → INSERT → SLICER.
- STASIS zeigt GRAIN oben und LAYER unten gleichzeitig. SELECT wechselt die
  aktive Zeile, K1–K4 bedienen die markierte Zeile. LAYER: MODE / AMT / TIME /
  Aktion, OCT+ startet oder bricht ADD/BLEND ab. Drehen allein startet nichts.
- REVERB TYPE=STASIS: CAPT LIVE/HOLD, STR und MOTN. GRAIN: 93/186/372 ms,
  SCAN, HQ/LONG/DRONE-Aufnahmefenster und REVERSE. In Reverse LIVE wird der
  SCAN-Bereich bei Bedarf begrenzt, damit Aufnahme und Wiedergabe nicht
  kollidieren; HOLD hat den vollen erfassten Bereich.
- Audio: weiche Grain-Übergänge, Vorfilter für LONG/DRONE, Schutz vor
  Überschreiben noch gespielter Reverse-Grains und weicher Aufnahmestart.
- HOME zweimal kurz: ACTION an/aus. Die fünf höchsten Keyboard-Tasten sind
  BANK und A1–A4. BANK kurz wechselt RANDOM/STASIS/PAIR/LIVE; BANK halten öffnet
  SETUP. In RANDOM startet A1 Random, A2 Freeze/Resume, A3 ruft den gewählten
  Snapshot per Morph auf. A4 kurz wählt S1–S4; mindestens 0,5 s halten speichert.
- `S1*` ist belegt, `S1-` leer. Alle vier Slots behalten je 128 Ziele, sind
  ausschließlich im RAM und gehen beim Neustart verloren. Ein Snapshot ist
  kein vollständiges Projekt oder eingefrorenes STASIS-Audio.
- Random verändert LEVEL/Gain nicht; Cutoff-Ziele liegen bei 150–6000 Hz.
  Manuell bewegte Parameter verlassen die laufende Morph-Bewegung.
- HOME hinzufügen: Originalregler bewegen, GLO halten und HOME tippen.

Bitte zunächst Grain/SCAN/Reverse bei LIVE und HOLD, wiederholtes ADD/BLEND,
STOP/PLAY sowie vier Tracks unter hoher Last prüfen. Auf regelmäßige Knackser,
unerwartete Pegeländerungen, hängende Noten und die CPU-Anzeige achten.

## Quellstand und Prüfung

Der Release enthält passendes vollständiges Quellarchiv einschließlich
Felucca-Engines, Prüfsummen, Buildnachweis und Lizenztexte. Das Quellarchiv
lässt sich mit den Voraussetzungen aus BUILDING.md über
`sh build.sh --binary-version 0.7.0-rc6` nachbauen. Der Ziel-Einstiegspunkt
aktiviert ACTION, Random Morph und erweitertes STASIS für diese Vorabversion.
Die automatischen Tests ersetzen keine Messung auf dem echten FM-1.
