# Contributing to Caravel

Thanks for helping! Caravel is a fork of Felucca 1.6.1 (see [NOTICE](NOTICE)); Felucca's active development has ended, and Caravel
is not affiliated with Hügelton Instruments or M-VAVE. Please send everything about Caravel to **this repository** (https://github.com/bonszaimusic/caravel), not to
Felucca's repositories:

| What | Where |
|---|---|
| A bug you can reproduce | this repository's Issues: one issue per bug |
| A feature idea, a question, a demo | this repository's Discussions, if enabled, otherwise an issue labelled as such |

## Reporting a bug

Please include the Caravel version (hold HOME > ABOUT), your computer's OS and browser if the editor or
installer is involved, the steps to reproduce it, what you expected and what happened. A photo or a short
video helps.

## Pull requests

Pull requests are welcome, including AI-assisted ones, with the same bar for all:

- keep it small and focused on one thing;
- make sure you understand the change and can explain it;
- test it on a real FM-1;
- run `tests/run_tests.sh` (see [BUILDING.md](BUILDING.md));
- new files carry the GPL-3.0-only SPDX header and `Copyright (C) 2026 bonszaimusic` (or your own line); keep the existing
  copyright lines of files you change.

By contributing you agree that your contribution is licensed under GPL-3.0-only.

The ANALOG, LOFI, TRIO, WHEEL and NOISE engines and the code they share (`firmware/engines/`, a git submodule) live in
[Felucca-Engines](https://github.com/hugelton/Felucca-Engines), under the MIT licence.

The web editor and installer are in `web/` of this repository. The separate `hugelton/Felucca-WebApp` is Felucca's.
