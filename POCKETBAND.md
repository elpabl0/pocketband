# Pocket Band

A generator on top of SLOOP 2.8.5 for the M-VAVE FM-1. Pick a place on a map of genre towns and it
writes a whole four-track sketch (drums, bass, keys, lead) in one key and tempo, with sounds and a
drum kit to match. Then re-roll any member, change its role, borrow it from another genre, or change
the energy of the whole band. Everything it writes is an ordinary SLOOP pattern, sound and tempo:
leave Pocket Band and edit, perform and save it with SLOOP as usual.

Status: built and tested on a computer (see Tests). **Not yet run on a real FM-1.**

## Opening it

On SLOOP's TRACKS screen, tap **HOME**. Tap HOME again for SLOOP's visualiser, and once more for
TRACKS. **SELECT** turns between Pocket Band's three screens. **PLAY** starts and stops as always,
and the layers you hold (FX punch-in effects, GLO mutes and fills) keep working on top.

## MAP

| Control | Does |
| --- | --- |
| KNOB 1 / KNOB 2 | walk east-west / north-south |
| KNOB 3 | jump to the next town |
| OCT+ | travel: write a new sketch for this spot and start playing |

Standing in a town gives that genre pure. Standing between two towns blends them: the tempo and swing
sit between the two, and each member comes from one town or the other. The 14 towns: house, techno,
acid, boom bap, trap, drum and bass, dub, synthwave, chiptune, ambient, funk, lo-fi, dembow, garage.

## BAND

| Control | Does |
| --- | --- |
| ALGORITHM | choose the member (drums, bass, keys, lead) |
| KNOB 1 IDEA | right: a new part for this member; left: back through earlier ones |
| KNOB 2 ENERGY | how much the whole band plays; quiet hits drop out first |
| KNOB 3 ROLE | keys: chords, pad or arp; lead: lead line or arp |
| KNOB 4 FROM | borrow this member (its part and its sound) from any town |
| PRESETS | the member's sound, from SLOOP's whole list (drums: the kits) |
| OCT+ | put the member to sleep / wake it (SLOOP's mute) |
| OCT- | the genre's next chord progression; bass, keys and lead follow |

FIT is a rough score of how well the member's part sits with the others: few collisions with the
other parts, the bass with the kick, chord tones on the beats.

IDEA and ENERGY write the part again from its seed. Notes you edited by hand in that part are
replaced, so shape the sketch here first and edit in SLOOP afterwards.

## CODE

Every sketch has a six-letter code. KNOB 1 picks a letter, KNOB 2 changes it, OCT+ travels to the
code. A code rebuilds the sketch as it was generated (place, key, tempo, every part); it does not
hold later changes. Save those as a SLOOP section or project.

## Building

`sh build-pocketband.sh` (toolchain and SDK as in BUILDING.md). SLOOP's app slot was nearly full,
so this build gives up two cosmetic things for the roughly 14 KB the generator needs: the boot logo
is drawn as text, and SLOOP's pages have no parameter icons.

## Tests

`sh tests/run_tests.sh` runs SLOOP's suite plus two for Pocket Band:

- `tests/pb_brain_test.c`: the generator alone. Same seed, same music; notes in range and in the
  scale; more energy never removes a hit; codes round-trip.
- `tests/pocketband_test.c`: the real overlay over SLOOP's real UI, sequencer and engines on the
  host. Travels to all 14 towns (audible, no clipping, every note in scale), drives every knob and
  button of the three screens, and runs 6000 frames of random use. Screens and audio land in
  `build/host/pb/`.

## Files

- `firmware/src/pb_brain.c`, `pb_brain.h`: genre profiles and the pattern generators. Plain C,
  integers only, no dependencies.
- `firmware/src/pocketband.c`: the overlay, and writing a sketch into SLOOP's tracks.
- Hooks: `felucca.c` (include), `ui_draw.c` (draw), `ui_input.c` (input, the HOME cycle).

GPL-3.0-only, as SLOOP and Felucca.
