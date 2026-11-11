# Pocket Band: how to use it

For Pocket Band 0.4 on the M-VAVE FM-1. A step-by-step demo is in [DEMO.md](DEMO.md).

Version 0.2 has been run on a real FM-1. The features added in 0.3 and 0.4 (the run, sketch memory,
build and drop, morph, chain, skins, tap, tour, the void, code words) are tested on a computer only.

## The idea

You have a band of four: **drums, bass, keys, lead**. You pick a place on a map of genre towns and the
band writes a sketch for that place: four parts in one key and tempo, with sounds to match. From there
you change members, sounds, key and energy, perform it, edit it, and save it.

## One rule

**A button opens its screen. Press the same button again and it does that screen's main action.**

The four knobs under the screen are named along the bottom of every screen, with their current values.

| Button | Screen | Press again to |
| --- | --- | --- |
| HOME | Stage | wake or sleep the selected member |
| GLO | Map | travel: write a new sketch |
| EDIT | Audition | hire the player you picked |
| SEQ | Grid | put or remove a note |
| ENV | Sound | go to the next page |
| LFO | Jam | change one part now |
| FX | Perform | start a build and drop |
| SEL | Key | get the next chord set |
| ARP | Visuals | next mode (in the run: start) |
| SAVE | Cart | do the selected thing |

## Controls that work on every screen

| Control | Does |
| --- | --- |
| ALGORITHM knob | selects the band member: drums, bass, keys, lead, round |
| PRESETS knob | changes the selected member's sound (drums: the kit) |
| SELECT knob | tempo, 40 to 240 |
| The 27 keys | play the selected member, in the sketch's key |
| OCT− / OCT+ | move the keys down or up an octave |
| PLAY | start and stop |
| REC | record (see Recording) |
| MASTER knob | volume |

Exceptions: on Perform the keys are effects and OCT−/OCT+ are throws; on Jam OCT−/OCT+ judge the last
change; in the run they duck and jump. With the drums selected, holding OCT− or OCT+ while you hit a
key plays a quiet or a hard hit.

The top bar shows the screen name, the tempo, a square while recording (red: notes, gold: tap) and a
triangle that flashes on the beat while playing. Messages appear in the top bar for a moment.

## First minute

1. Press **GLO**. The map opens with the band standing in House.
2. Press **GLO** again. A House sketch is written and starts playing, and you are on the Stage.
3. Turn **KNOB 1**. The selected member gets a new part. Turn it back to get the old one.
4. Turn **ALGORITHM** to pick another member and do the same.
5. Press **GLO**, turn **KNOB 1** and **KNOB 2** to walk somewhere else, press **GLO** to travel.

## The screens

### Stage (HOME)

The band. A sleeping member (muted) is greyed with a "z".

| Knob | Does |
| --- | --- |
| 1 IDEA | right: a new part for this member. Left: back through earlier ones. Wakes the member. |
| 2 ENERGY | how much the whole band plays. Quiet hits drop out first. |
| 3 ROLE | keys: chords, pad or arp. Lead: lead or arp. Bass and drums have one role. |
| 4 FROM | borrow this member, part and sound, from any town. |

**HOME again:** wake or sleep the member.

The screen also shows the member's sound, where it is from, FIT (how well its part sits with the
others, for bass, keys and lead) and the sketch's code.

IDEA, ENERGY, ROLE and FROM write the part again from the generator. Notes you edited by hand in that
part are replaced, so shape the sketch first and edit afterwards.

### Map (GLO)

| Knob | Does |
| --- | --- |
| 1 EAST | walk west or east |
| 2 SOUTH | walk north or south |
| 3 TOWN | jump to the next town |

**GLO again:** travel. A new sketch is written for this spot and starts playing.

Standing in a town gives that genre pure. Between two towns you get a blend: the bar shows the mix,
the tempo and swing sit between the two, and each member comes from one town or the other. Each
journey is different, even to the same spot.

The 16 towns: Ambient, Lo-fi, Boom bap, Trap, Dub, Funk, Dembow, Garage, Synthwave, House, Techno,
Drum and bass, Chiptune, Acid, Afrobeats, Drill.

### Audition (EDIT)

Three other players for the selected member: one from the same town, one from the sketch's other town,
and a wildcard. Each has a FIT score (for drums, a hit count).

| Knob | Does |
| --- | --- |
| 1 PICK | step through NOW and the three players. The picked one plays in the mix. |
| 2 MORE | three different players |

**EDIT again:** hire the picked player. If you leave without hiring, the member goes back to how it
was.

### Grid (SEQ)

The selected member's part, one bar of 16 steps at a time. Four bars in all.

| Knob | Does |
| --- | --- |
| 1 STEP | move the cursor along the steps (it runs on into the next bar) |
| 2 NOTE | move the cursor up and down: notes of the scale, or the drum sounds |
| 3 LENGTH | how many steps a new note lasts (not on drums) |
| 4 BAR | jump a bar |

**SEQ again:** put a note at the cursor, or remove the one there. On a step that already has notes it
adds to the chord, up to four.

While stopped, pressing a key writes that note at the cursor and moves on a step (not on drums). A
white marker shows the playing step.

### Sound (ENV)

Four knobs for the selected member's sound, on three pages. **ENV again:** next page.

| Page | Bass, keys, lead | Drums |
| --- | --- | --- |
| TONE | the sound's own four main controls (names change with the sound) | kit, level, filter, pan |
| SHAPE | attack, release, filter, level | reverb, echo, duck, dust |
| SPACE | reverb, echo, chorus, pan | room size, echo time, echo feedback, echo mix |

The filter knob closes a low-pass to the left and opens a high-pass to the right; the middle is off.
The last three on the drums' SPACE page are the reverb and echo the whole band shares.

### Jam (LFO)

| Knob | Does |
| --- | --- |
| 1 EVERY | the band changes one part by itself every 8, 4 or 2 bars. OFF to stop. |
| 2 ENERGY | as on the Stage |
| 3 MORPH | leans every part towards its next idea, 0 to 100. The same value always gives the same notes. |
| 4 TOUR | every 32 or 16 bars the band walks on along the map and plays a new sketch there |

**LFO again:** change one part now.

After a change the screen names it. **OCT−** kills it (the part goes back), **OCT+** keeps it.

The jam and the tour keep running while you are on other screens.

### Perform (FX)

The 27 keys are punch-in effects. Hold a key for as long as you want the effect.

White keys, low to high: loop 4, loop 8, loop 16, loop 32, stutter, reverse, stop, half speed, low
(low-pass), high (high-pass), phone, crush, alias, gate, echo, wobble.

Black keys, low to high: kill low, kill mid, kill high, rewind, octave up, flanger, auto-pan, freeze,
loop 8 triplet, mono, fade out.

| Control | Does |
| --- | --- |
| KNOB 1 FILTER | filter on everything: left low-pass, right high-pass |
| KNOB 2 DUST | lo-fi crackle and grit |
| KNOB 3 DUCK | the kick pushes the other parts down |
| KNOB 4 FADE | how gently effects come in and out |
| OCT− held | reverb throw |
| OCT+ held | echo throw |

**FX again:** build and drop. From the next bar you get two bars of snare roll with the bass out and
the filter rising, then the drop: drums back, filter as it was, everyone awake. The music must be
playing.

### Key (SEL)

| Knob | Does |
| --- | --- |
| 1 ROOT | the key's root note |
| 2 SCALE | major, minor, dorian, mixolydian, harmonic minor, phrygian, lydian |
| 3 SWING | 0 is straight |
| 4 CHORDS | the next chord set |

**SEL again:** the next chord set.

The keyboard picture shows which notes are in the scale; the root is marked. The four boxes are the
chord of each bar, with the playing bar lit. Changing root, scale or chords writes the bass, keys and
lead again in the new key.

### Visuals (ARP)

**KNOB 1** or **ARP again** steps through four modes: scope, meters, stars, and the run.

**The run** is a game. The selected member runs along the drum part that is playing.

- A kick is a block: **jump**. A snare or clap is a beam: **duck**.
- High keys (the upper half of the keyboard) or OCT+ jump. Low keys or OCT− duck.
- **ARP**, or any key, starts it.
- You have three lives. A miss costs one and puts a band member to sleep, so the music thins.
- Twelve clean in a row wakes a member and gives a life back.
- Your best score is kept.

The keys play no notes while the run is showing. Leave with any other screen button.

### Cart (SAVE)

**KNOB 1** picks the page. **SAVE again** does the page's action.

**CODE.** Every sketch has a six-letter code that rebuilds it exactly as it was generated. KNOB 2 picks
a letter position, KNOB 3 changes the letter, SAVE travels to that code. Codes use A to Z without I and
O, and 2 to 9. A code does not hold changes you made afterwards.

**SLOTS.** Four slots, A to D, hold the notes and sounds as they are now, edits included. KNOB 2 picks
the slot, KNOB 3 the action:

- STORE: keep the current sketch in the slot.
- PLAY: while playing, the slot takes over from the next bar; while stopped, it loads.
- CHAIN: the stored slots play one after another, round and round. Needs two stored slots and the
  music playing.

A loaded slot plays and can be edited, but the Stage's IDEA and ENERGY knobs still belong to the last
sketch you travelled to.

**SETUP.** KNOB 2 picks the item, KNOB 3 sets it.

| Item | Values |
| --- | --- |
| PANEL LIGHTS | off, low, mid, high |
| KEY LIGHTS | off or on (any value above OFF lights the keys) |
| CLICK | off, while recording, always |
| CLOCK FROM | own, USB, jack |
| MIDI OUT | the keys, or the parts as they play |
| USB AUDIO | auto, 44.1, 48 kHz (from the next power-on) |
| SPEAKER LOW CUT | on, off |
| SECOND CORE | on, off |
| SKIN | six colour sets |
| CALIBRATE | SAVE: teach each button and knob again (stopped only) |
| NEW SKETCHBOOK | SAVE: empty the four parts |
| ABOUT | version and credits. SAVE: the band takes a bow. |

## Recording

REC has three presses.

1. **REC:** record. What you play on the keys goes into the selected member's part, on the nearest
   step, in the sketch's key. If the music is stopped, it starts.
2. **REC again:** tap. The part is emptied, and any key you tap in time becomes a note picked for you
   from that bar's chord:
   - bass: root, fifth or octave, by how high the key is
   - keys: the whole chord
   - lead: a note that rises with the key
   - drums: kick, snare, hat on the low, middle and high third of the keyboard
3. **REC a third time:** off.

PLAY to stop also ends recording.

## Switching off

The sketch is remembered: its code, each member's idea, energy, morph, map position, tour setting, skin
and best run come back with the music at the next power-on. It is written once you have stopped the
music and left the FM-1 alone for a short while (allow about twenty seconds), so stop before you switch
off.

## Hidden things

- Walk off the south-east corner of the map and travel. A "?" blinks when you are close.
- Four codes are words, not places: FASTER, SLEEPY, PARTY2, SECRET. Enter PARTY2 again to end it.

## At power-on

| Hold | Does |
| --- | --- |
| OCT− alone | USB rescue: the FM-1 waits for an install. Use this if it will not start. |
| OCT− and OCT+ | calibration: teach each button and knob |

While running and stopped, holding OCT− and OCT+ together for five seconds puts the FM-1 in update
mode. It counts down in the top bar first; let go to cancel.

## Good to know

- Four members is the limit; the engines underneath have four tracks.
- No user samples: this firmware does not talk to SLOOP's web editor.
- The sketch's record uses the last 8 KB of SLOOP's fourth user-sample slot. If you had loaded a
  sample longer than 72 KB there with SLOOP, it would lose its tail.
- To go back: install SLOOP from its own page, or the original firmware with M-VAVE's M-UPGRADE.
