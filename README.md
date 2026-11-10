# Pocket Band

A firmware for the M-VAVE FM-1 that writes music with you. Pick a place on a map of genre towns and it
writes a four-piece sketch (drums, bass, keys, lead) in one key and tempo, with sounds to match. Then
re-roll any member, audition replacements, change the key, let the band jam, play punch-in effects on
the keys, edit any part step by step, and keep what you like in four save slots.

**Status: 0.2, experimental.** It is tested on a computer against the real sound engines and sequencer
(see Tests). The standalone interface has not yet been run on a real FM-1. Install at your own risk.

Pocket Band is its own firmware with its own interface. Underneath, the sound engines, sequencer,
storage, USB and hardware drivers are those of [SLOOP 2.8.5](https://github.com/isod89/sloop-fm1) by
isod89, which is based on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton
Instruments). None of SLOOP's screens remain, and SLOOP's web editor does not talk to this firmware.
SLOOP's own README is kept as [SLOOP-README.md](SLOOP-README.md) for reference on the engines.

## How it works

One rule: **a button opens its screen; pressed again, it does that screen's main action.**

| Button | Screen | Knobs 1 to 4 | Button again |
| --- | --- | --- | --- |
| HOME | **Stage**: the band | idea, energy, role, from | wake / sleep the member |
| GLO | **Map**: 14 genre towns | east, south, town | travel: a new sketch |
| EDIT | **Audition**: three other players, ranked by fit | pick, more | hire the one you picked |
| SEQ | **Grid**: the member's part, step by step | step, note, length, bar | put / remove a note |
| ENV | **Sound**: three pages of four knobs | (the page's four) | next page |
| LFO | **Jam**: the band changes its own parts | how often, energy | change a part now |
| FX | **Perform**: the 27 keys are effects | filter, dust, duck, fade | wake / sleep the member |
| SEL | **Key**: key, scale, swing, chords | root, scale, swing, chords | next chord set |
| ARP | **Visuals**: scope, meters, stars | mode | next mode |
| SAVE | **Cart**: code, slots, setup | page, then that page's | do it |

On every screen:

- **ALGORITHM** picks the band member, **PRESETS** its sound, **SELECT** the tempo.
- The **keys** play the selected member, in the sketch's key. On Perform they are the effects.
- **OCT− / OCT+** move the keys an octave. On Jam they kill / keep the last change; on Perform they
  are the reverb and echo throws; on the drums, held, they play a ghost or a hard hit.
- **PLAY** starts and stops. **REC** records what you play into the member's part.

Notes on a few screens:

- **Map.** In a town you get that genre pure. Between two towns you get a blend: the tempo and swing sit
  between the two and each member comes from one town or the other.
- **Stage.** IDEA and ENERGY write a part again from its seed, so hand edits to that part are replaced.
  Shape the sketch first, edit afterwards.
- **Cart.** A six-letter code rebuilds a sketch exactly as it was generated. Later edits live in the
  four slots A to D, which can also be played from the next bar while the music runs. SETUP holds the
  lights, click, clock source, USB audio rate, calibration, a new empty sketchbook, and ABOUT.

## Install

Open the installer page from a release in Chrome or Edge on a computer, connect the FM-1 with a USB-C
data cable (no hub), and press INSTALL. If the FM-1 does not start afterwards, hold OCT− alone while
switching it on (USB rescue) and install again. To leave, install SLOOP from its own page, or the
original firmware with M-VAVE's M-UPGRADE.

Saved data: Pocket Band keeps SLOOP's storage format, so slots and settings survive moving between
the two. A sketch's code, ideas and energy are not stored in a slot; a loaded slot plays and can be
edited, but its IDEA and ENERGY knobs belong to the last sketch you travelled to.

## Build

Linux x86-64 (or Docker / WSL), the JieLi toolchain and three SDK files: see [BUILDING.md](BUILDING.md).

```sh
sh build.sh
python3 tools/make_pocketband_installer.py . build/felucca.fwsc pocket-band-installer.html
```

## Tests

`sh tests/run_tests.sh` runs the engine and sequencer tests inherited from SLOOP, plus:

- `tests/pb_brain_test.c`: the generator alone: same seed, same music; notes in range and in the
  scale; more energy never removes a hit; codes round-trip.
- `tests/pb_fw_test.c`: the whole interface over the real sequencer and engines on the host: every
  screen, knob and button, every town rendered to audio, slots, codes, and 12,000 frames of random
  input, also under AddressSanitizer and UBSan. Screens land in `build/host/pb/`.

## Source map

- `firmware/src/pb_brain.c`: genre profiles and pattern generators (plain C, integers only).
- `firmware/src/pb_band.c`: the map, the members, writing a sketch into the tracks.
- `firmware/src/pb_ui.c`: every screen, the buttons, the LEDs, the boot screen.
- `firmware/src/pb_core.c`: sound loading, undo, calibration (taken from SLOOP's interface code).
- Everything else under `firmware/` is SLOOP / Felucca.
- `prototype/`: the browser prototype the design started from.

## Licence

GPL-3.0-only, as SLOOP and Felucca; see [LICENSE](LICENSE), [LICENSING.md](LICENSING.md) and
`LICENSES/`. "M-VAVE" and "FM-1" are trademarks of their owners; this is independent firmware, not
affiliated with or supported by them, nor by the SLOOP or Felucca authors.
