# Caravel 0.7.0-rc1 — FM-1 Testversion

Diese Vorabversion enthält echte, installierbare FM-1-Firmware. Sie ist für den
angeforderten Gerätetest gedacht; ein Hör-, Last- und Installationstest am
physischen FM-1 steht noch aus. Der stabile 0.6.0-Release wird nicht ersetzt.

## Installation und Datensicherung

1. Projekte und Einstellungen vor dem Update exportieren und die bisherige
   funktionierende Firmware für den Rückweg bereithalten.
2. `caravel-0.7.0-rc1.fwsc` herunterladen. Im bestehenden Caravel-Webinstaller
   die lokale Datei auswählen und das Update am per USB angeschlossenen FM-1
   durchführen. Die `.fwsc` enthält App und Update-Loader.
3. Nach dem Neustart unter INFO die Versionsanzeige `v0.7.0-rc1` prüfen.
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
`sh build.sh --binary-version 0.7.0-rc1` nachbauen. Der Ziel-Einstiegspunkt
aktiviert ACTION, Random Morph und erweitertes STASIS für diese Vorabversion.
Die automatischen Tests ersetzen keine Messung auf dem echten FM-1.
