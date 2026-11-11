# Credits

Pocket Band is a firmware of its own, with its own interface and music generator, but it stands on
other people's work. Everything that makes sound, keeps time, stores data, talks USB and MIDI, drives the
hardware and installs updates comes from the two projects below. Thank you.

## What Pocket Band is built on

- **[SLOOP](https://github.com/isod89/sloop-fm1) 2.8.5** by **isod89**. Pocket Band started from SLOOP's
  source tree: its twelve sound engines, 153 sounds, drum kits, four-track sequencer, punch-in effects,
  project storage, USB audio and MIDI, hardware layer, update loader, USB rescue, build tools and host tests.
  SLOOP's screens, web editor and site are not part of Pocket Band.
- **[Felucca](https://github.com/hugelton/Felucca)** by **Leo Kuroshita** (@kurogedelic), **Hügelton
  Instruments**, which SLOOP is based on: the firmware's architecture, the engines, the sequencer, the
  hardware layer, the update loader and the installer.

Both are GPL-3.0-only, and so is Pocket Band. The copyright notices in the source files are theirs and
stay as they are; the comments that say which version or person a piece of code came from stay too.

## What is new in Pocket Band

The music generator (`firmware/src/pb_brain.c`), the band, map and sketch logic (`pb_band.c`), every
screen (`pb_ui.c`), the sketch memory (`pb_store.c`), their tests, the guide and the browser prototype.
`firmware/src/pb_core.c` is SLOOP's interface code with the screens taken out.

## SLOOP's credits

From the "Credits and thanks" of SLOOP 2.8.5's README, unchanged. They credit the work Pocket Band
inherits (some of it, such as the web editor, is not in Pocket Band).

- **[Felucca](https://github.com/hugelton/Felucca)** by **Leo Kuroshita** (@kurogedelic) / **Hügelton Instruments** — the engines, the sequencer, the editor, the installer, in 2.3 the USB audio input, the MIDI clock, the knob reading and many fixes, in 2.4 the FM6 engine and its DX7 patch editor (Felucca 1.0 / 1.0.1), and in 2.6 the INSERT effects, LFO 2 (SYNC / TRIG / POL), ENV SYNC, the ANALOG filter types, note LEN and step velocity, MIDI LEARN, the live drum mutes, the latched-arp fix (#191), the 1/4D delay and REV / ECHO THROW (Felucca 1.4 / 1.4.1 / 1.5 / 1.5.1), and in 2.7 the arp modes REPEAT, DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK and CHORD, and ORD in the order you pressed (Felucca 1.4 / 1.5.1). Thank you.
- **The Felucca community, who asked for those features first** (Felucca discussions and issues): **Wulgaren** (insert effects with dry / wet, #78; the arp report, #191), **etyrnal** (the lo-fi bit effect, #177; envelopes in note values, #175), **benjaminguerrer** (LFO sync, #154), **gospodindilly-ops** (the ANALOG filter types, #104), **Armstrong1980** (#178) and **oMauNaProducer** (#173) (note length, step velocity), **lostfictions** and **planist79** (MIDI LEARN, #170).
- **@renebohne** — the played-note key lights (pull request #11) and, in 2.6, MIDI Program Change (Felucca pull request #179).
- **ChanceTheMaker** and **keremimo** — the TRS MIDI input fix (Felucca Salt) and contributions to the MIDI clock.
- **andrerezemini** ([sloop-lab](https://github.com/andrerezemini/sloop-lab)) — in 2.7 the sound on the FM-1's second core and 38 KB of RAM back (pull request #111), on the dual-core start of **Charles Vestal** ([X0X](https://github.com/charlesvestal/fm1-x0x)) and **keremimo** ([Melodee](https://github.com/keremimo/melodee)).
- **juanjiant-bit** ([JIANT FM](https://github.com/juanjiant-bit/JIANT)) — in 2.7 the progressive knob acceleration (KNOB ACCEL) and the LEVEL / PAN glide come from JIANT's.
- **renebohne** — in 2.7 MIDI CC 74 sweeps each engine's own cutoff or brightness, after his pull request #71 (Felucca 1.1.5's standard CC map).
- **@ordenador** (Mario Faúndez Vidal) — the web editor's local audio preview: the firmware's own engines and sequencer in the browser (WebAssembly), with the playable piano / pads and the inline sequence editing (pull request #120).
- **Everyone who installed SLOOP, made music with it, commented, reported a bug or asked for a feature** — most of each update comes from your messages: parameter locks, DX7 patches, dotted delays and longer steps (2.4); in 2.6 a MIDI channel per track, sustain and pitch bend, step entry from a MIDI keyboard, TEXTURE, the fixed crackles, the web editor bug with the same value on two tracks, and the MIDI learn backup. You asked, tested and reported them.
- Samples: Versilian Studios VSCO-2 CE and VCSL, Sonic Pi (all CC0). Font: Terminus and Terminus Bold (SIL OFL 1.1). Icons: Fukiai (MIT, Hügelton Instruments). PHASE engine after CrispyZebra (GPL); VOICE after klattsch (MIT); FM6: msfa from Dexed by Google Inc. and Pascal Gauthier (Apache-2.0).
- Interface ideas after teenage engineering's pocket operators and EP-133, Elektron's step entry and Akai's MPC (swing, note repeat, erase).

## Third-party material

Licences and where each piece lives: [LICENSING.md](LICENSING.md). Sample provenance:
`assets/samples-cc0/ATTRIBUTION.txt` and `CREDITS.txt` there.
