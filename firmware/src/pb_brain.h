/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band music brain for the FM-1: genre profiles and pattern generators.
 * Freestanding C99: integers only, no allocation, no library calls. The same seed always
 * gives the same music, so a sketch is fully described by (genre A, genre B, blend, seed). */
#ifndef PB_BRAIN_H
#define PB_BRAIN_H
#include <stdint.h>

#define PB_STEPS 64u
#define PB_GENRES 14u
enum { PB_BASS, PB_CHORD, PB_PAD, PB_LEAD, PB_ARP, PB_ROLES };

typedef struct {            /* one step of a melodic part */
    uint8_t note[4];        /* MIDI notes, n of them (0 = empty step) */
    uint8_t n, vel;
    uint8_t gate;           /* length in steps, 1..16 */
    uint8_t slide;
} pb_ev_t;

typedef struct {            /* one step of the drum part: bit per lane (kick, kick2, snare, clap, hat, open hat,
                             * pedal, rim, snare2, low tom, high tom, crash, ride, shaker, conga, cowbell) */
    uint16_t on, ghost;
} pb_dstep_t;

typedef struct {
    uint8_t ga, gb, blend;  /* where on the map: two genres and 0..7 of the way from A to B */
    uint32_t seed;          /* 19 bits */
    uint16_t bpm;
    uint8_t swing;          /* percent of a step the off-beats are late, 0..33 */
    uint8_t root;           /* 0..11 */
    uint8_t scale;          /* PB_SC_* */
    uint8_t prog[4];        /* one scale degree (0..6) per bar */
    uint8_t prog_set;       /* which of the genre's progressions */
    uint8_t prog_src;       /* genre the progressions come from */
    uint8_t sev;            /* chords carry a 7th */
    uint8_t src[4];         /* genre of each track: bass, keys, lead, drums */
    uint8_t role[3];        /* PB_* of the three melodic tracks */
    uint32_t tseed[4];
    uint8_t awake;          /* bit per track: plays from the start */
} pb_song_t;

enum { PB_SC_MAJ, PB_SC_MIN, PB_SC_DOR, PB_SC_MIX, PB_SC_HARM, PB_SC_PHRY, PB_SC_LYD, PB_SCALES };

const char *pb_genre_name(uint32_t g);
const char *pb_genre_tag(uint32_t g);        /* five characters or fewer, for the map */
void pb_make_song(pb_song_t *s, uint32_t ga, uint32_t gb, uint32_t blend, uint32_t seed);
/* energy 0..255: how much of each part plays; the low-priority hits drop out first */
void pb_gen_drums(const pb_song_t *s, uint32_t genre, uint32_t seed, uint32_t energy, pb_dstep_t out[PB_STEPS]);
void pb_gen_part(const pb_song_t *s, uint32_t role, uint32_t genre, uint32_t seed, uint32_t energy, pb_ev_t out[PB_STEPS]);
void pb_next_chords(pb_song_t *s);           /* the genre's next progression */
uint32_t pb_note(const pb_song_t *s, int32_t degree);     /* scale degree (7 per octave, 35 = middle C's octave) -> MIDI */
uint32_t pb_snap(const pb_song_t *s, uint32_t note);      /* nearest note of the scale */
/* 40..99: how well a candidate part sits with the drums and the other two parts */
uint32_t pb_fit(const pb_song_t *s, uint32_t role, const pb_ev_t cand[PB_STEPS], const pb_dstep_t drums[PB_STEPS],
                const pb_ev_t *other_a, const pb_ev_t *other_b);
void pb_encode(const pb_song_t *s, char out[7]);          /* the six-letter password */
int pb_decode(const char *pw, uint32_t *ga, uint32_t *gb, uint32_t *blend, uint32_t *seed);   /* 0 = not a password */
#endif
