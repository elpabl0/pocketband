/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band music brain (see pb_brain.h). Weight strings are 16 steps: '0' never, '9' always;
 * anything between is a hit whose priority decides at which ENERGY it starts to play. */
#include "pb_brain.h"

typedef struct {
    const char *name, *tag;
    uint8_t bpm[2], sw[2];
    uint8_t nsc, sc[3];
    uint8_t nprog, prog[4][4];
    uint8_t sev;                          /* percent of sketches whose chords carry a 7th */
    uint8_t k[8], s[8], c[8], h[8], o[8]; /* kick, snare, clap, hat, open hat (two steps a byte) */
    uint8_t rim[8], shk[8], tl[8], th[8], cb[8];   /* rim, shaker, low tom, high tom / conga, cowbell */
    uint8_t bw[8];                        /* bass rhythm; b_follow: the kick's instead */
    uint8_t b_oct, b_fifth, b_col, b_gate, b_slide, b_follow;   /* percents; gate 0 = to the next note */
    uint8_t cw[8]; uint8_t c_gate;        /* chord rhythm */
    uint8_t lw[8];                        /* lead rhythm */
    uint8_t aw[8]; uint8_t a_rate;        /* arp mask and step */
    uint8_t keys, lead, awake;            /* roles of tracks 2 and 3; bit 0 bass, 1 keys, 2 lead, 3 drums */
} pb_genre_t;

#define PB_Z {0, 0, 0, 0, 0, 0, 0, 0}
static const pb_genre_t PB_G[PB_GENRES] = {
    {"HOUSE", "HOUSE", {120, 126}, {0, 8}, 2, {PB_SC_MIN, PB_SC_DOR}, 4, {{0, 5, 2, 6}, {0, 3, 5, 4}, {0, 0, 5, 6}, {0, 2, 6, 3}}, 50,
     {0x90,0x00,0x90,0x00,0x90,0x00,0x90,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x12}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, {0x53,0x52,0x53,0x52,0x53,0x52,0x53,0x52}, {0x00,0x90,0x00,0x90,0x00,0x90,0x00,0x90},
     {0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x10}, {0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36}, {0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x01}, {0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00}, PB_Z,
     {0x00,0x70,0x10,0x70,0x00,0x70,0x10,0x72}, 15, 10, 5, 2, 0, 0, {0x00,0x00,0x70,0x03,0x00,0x07,0x00,0x30}, 1, {0x70,0x20,0x50,0x30,0x20,0x70,0x50,0x20}, {0x90,0x99,0x90,0x99,0x90,0x99,0x90,0x99}, 1, PB_CHORD, PB_ARP, 15},
    {"TECHNO", "TECHN", {128, 138}, {0, 4}, 2, {PB_SC_PHRY, PB_SC_MIN}, 3, {{0, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 5, 6}}, 10,
     {0x90,0x00,0x90,0x00,0x90,0x00,0x90,0x00}, PB_Z, {0x00,0x00,0x70,0x00,0x00,0x00,0x70,0x00}, {0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52}, {0x00,0x90,0x00,0x90,0x00,0x90,0x00,0x90},
     {0x00,0x10,0x00,0x01,0x00,0x10,0x01,0x00}, PB_Z, {0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x10}, {0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x10},
     {0x05,0x97,0x05,0x97,0x05,0x97,0x05,0x97}, 5, 0, 5, 1, 0, 0, {0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x90}, 1, {0x30,0x00,0x00,0x10,0x00,0x30,0x00,0x10}, {0x90,0x09,0x00,0x90,0x90,0x09,0x00,0x90}, 1, PB_CHORD, PB_ARP, 13},
    {"ACID", "ACID", {124, 134}, {0, 8}, 2, {PB_SC_MIN, PB_SC_PHRY}, 3, {{0, 0, 0, 0}, {0, 0, 6, 0}, {0, 0, 5, 6}}, 20,
     {0x90,0x00,0x90,0x00,0x90,0x00,0x90,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x21}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, {0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52}, {0x00,0x90,0x00,0x90,0x00,0x90,0x00,0x90},
     {0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x10}, {0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30}, PB_Z, {0x00,0x10,0x00,0x00,0x00,0x10,0x00,0x00},
     {0x95,0x77,0x95,0x77,0x95,0x77,0x95,0x77}, 20, 10, 25, 1, 25, 0, {0x00,0x00,0x00,0x00,0x00,0x90,0x00,0x00}, 1, {0x30,0x00,0x20,0x00,0x10,0x00,0x20,0x00}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 1, PB_CHORD, PB_ARP, 9},
    {"BOOM BAP", "BOOM", {84, 96}, {14, 24}, 2, {PB_SC_MIN, PB_SC_DOR}, 3, {{0, 3, 6, 2}, {0, 5, 3, 4}, {1, 4, 0, 0}}, 90,
     {0x90,0x00,0x00,0x03,0x90,0x20,0x00,0x00}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, PB_Z, {0x90,0x50,0x90,0x50,0x90,0x50,0x90,0x50}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20},
     {0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x00}, {0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30}, PB_Z, PB_Z, PB_Z,
     PB_Z, 10, 15, 10, 0, 0, 1, {0x90,0x00,0x00,0x00,0x00,0x30,0x00,0x00}, 0, {0x50,0x20,0x30,0x20,0x50,0x20,0x30,0x20}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 2, PB_CHORD, PB_LEAD, 15},
    {"TRAP", "TRAP", {132, 150}, {0, 4}, 3, {PB_SC_MIN, PB_SC_PHRY, PB_SC_HARM}, 3, {{0, 0, 5, 4}, {0, 5, 0, 6}, {0, 3, 0, 4}}, 30,
     {0x90,0x00,0x00,0x00,0x00,0x30,0x09,0x00}, {0x00,0x00,0x00,0x00,0x90,0x00,0x00,0x03}, {0x00,0x00,0x00,0x00,0x90,0x00,0x00,0x00}, {0x92,0x92,0x93,0x92,0x92,0x92,0x93,0x96}, {0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00},
     {0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00}, PB_Z, PB_Z, PB_Z, {0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00},
     PB_Z, 5, 10, 5, 0, 20, 1, {0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 0, {0x70,0x00,0x30,0x00,0x50,0x00,0x20,0x30}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 2, PB_CHORD, PB_LEAD, 15},
    {"DRUM N BASS", "DNB", {168, 176}, {0, 6}, 2, {PB_SC_MIN, PB_SC_DOR}, 3, {{0, 0, 5, 6}, {0, 5, 2, 6}, {0, 3, 5, 4}}, 50,
     {0x90,0x00,0x00,0x00,0x00,0x93,0x00,0x00}, {0x00,0x00,0x90,0x02,0x00,0x00,0x90,0x20}, PB_Z, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30},
     {0x00,0x10,0x00,0x01,0x00,0x00,0x01,0x00}, {0x35,0x35,0x35,0x35,0x35,0x35,0x35,0x35}, PB_Z, PB_Z, PB_Z,
     {0x90,0x00,0x00,0x00,0x00,0x30,0x00,0x90}, 5, 15, 10, 0, 0, 0, {0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 0, {0x50,0x00,0x20,0x00,0x30,0x00,0x10,0x00}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 2, PB_PAD, PB_LEAD, 15},
    {"DUB", "DUB", {68, 80}, {8, 16}, 2, {PB_SC_MIN, PB_SC_DOR}, 3, {{0, 0, 3, 3}, {0, 6, 0, 6}, {0, 3, 0, 4}}, 30,
     {0x20,0x00,0x00,0x00,0x90,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x90,0x00,0x00,0x00}, PB_Z, {0x90,0x93,0x90,0x93,0x90,0x93,0x90,0x93}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20},
     {0x00,0x00,0x90,0x00,0x00,0x00,0x10,0x00}, {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20}, PB_Z, {0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00}, PB_Z,
     {0x90,0x02,0x30,0x00,0x90,0x02,0x00,0x30}, 0, 25, 20, 0, 0, 0, {0x00,0x90,0x00,0x90,0x00,0x90,0x00,0x90}, 1, {0x50,0x00,0x20,0x00,0x30,0x00,0x00,0x00}, {0x00,0x90,0x00,0x90,0x00,0x90,0x00,0x90}, 2, PB_CHORD, PB_LEAD, 11},
    {"SYNTHWAVE", "SYNTH", {96, 116}, {0, 2}, 2, {PB_SC_MIN, PB_SC_DOR}, 4, {{5, 6, 0, 0}, {0, 5, 2, 6}, {0, 6, 5, 6}, {0, 2, 5, 4}}, 30,
     {0x90,0x00,0x90,0x00,0x90,0x00,0x90,0x00}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, PB_Z, {0x90,0x50,0x90,0x50,0x90,0x50,0x90,0x50}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x50},
     PB_Z, PB_Z, {0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x30}, {0x00,0x00,0x00,0x00,0x00,0x30,0x00,0x00}, PB_Z,
     {0x92,0x92,0x92,0x92,0x92,0x92,0x92,0x92}, 10, 5, 0, 1, 0, 0, {0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 0, {0x70,0x30,0x50,0x30,0x70,0x30,0x50,0x20}, {0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99}, 1, PB_PAD, PB_LEAD, 15},
    {"CHIPTUNE", "CHIP", {132, 158}, {0, 2}, 2, {PB_SC_MAJ, PB_SC_MIN}, 4, {{0, 4, 5, 3}, {0, 3, 4, 4}, {5, 3, 0, 4}, {0, 5, 3, 4}}, 0,
     {0x90,0x00,0x30,0x00,0x90,0x00,0x30,0x30}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, PB_Z, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, PB_Z,
     {0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x10}, PB_Z, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30}, PB_Z, PB_Z,
     {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 50, 10, 0, 1, 0, 0, {0x90,0x09,0x00,0x90,0x90,0x09,0x00,0x90}, 1, {0x90,0x50,0x70,0x50,0x90,0x50,0x70,0x50}, {0x99,0x99,0x99,0x99,0x99,0x99,0x99,0x99}, 1, PB_ARP, PB_LEAD, 15},
    {"AMBIENT", "AMBNT", {62, 80}, {0, 0}, 2, {PB_SC_MAJ, PB_SC_LYD}, 3, {{0, 3, 0, 4}, {0, 5, 3, 4}, {3, 0, 4, 5}}, 80,
     {0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, PB_Z, PB_Z, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}, PB_Z,
     PB_Z, {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10}, PB_Z, PB_Z, {0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00},
     {0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 0, 0, 0, 0, 0, 0, {0x90,0x00,0x00,0x00,0x30,0x00,0x00,0x00}, 0, {0x50,0x00,0x20,0x00,0x30,0x00,0x10,0x00}, {0x90,0x90,0x00,0x90,0x90,0x00,0x00,0x90}, 2, PB_PAD, PB_LEAD, 7},
    {"FUNK", "FUNK", {104, 118}, {4, 12}, 2, {PB_SC_DOR, PB_SC_MIX}, 4, {{0, 3, 0, 3}, {1, 4, 0, 0}, {0, 0, 3, 4}, {0, 6, 3, 0}}, 80,
     {0x90,0x00,0x00,0x03,0x90,0x20,0x00,0x00}, {0x00,0x00,0x90,0x02,0x00,0x00,0x90,0x20}, PB_Z, {0x95,0x97,0x95,0x97,0x95,0x97,0x95,0x97}, {0x00,0x00,0x00,0x00,0x00,0x50,0x00,0x50},
     PB_Z, {0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53}, PB_Z, {0x00,0x30,0x00,0x03,0x00,0x30,0x00,0x03}, {0x00,0x00,0x10,0x00,0x00,0x00,0x10,0x00},
     {0x90,0x03,0x09,0x00,0x30,0x90,0x03,0x90}, 25, 15, 15, 1, 0, 0, {0x00,0x90,0x03,0x00,0x90,0x03,0x09,0x00}, 1, {0x50,0x00,0x30,0x20,0x50,0x00,0x30,0x20}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 2, PB_CHORD, PB_LEAD, 15},
    {"LO-FI", "LOFI", {70, 86}, {16, 26}, 2, {PB_SC_MAJ, PB_SC_DOR}, 3, {{1, 4, 0, 5}, {0, 5, 1, 4}, {3, 2, 1, 0}}, 100,
     {0x90,0x00,0x00,0x02,0x90,0x10,0x00,0x00}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, PB_Z, {0x90,0x50,0x90,0x50,0x90,0x50,0x90,0x50}, PB_Z,
     {0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00}, {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20}, PB_Z, PB_Z, PB_Z,
     PB_Z, 5, 20, 15, 0, 0, 1, {0x90,0x00,0x00,0x00,0x00,0x20,0x00,0x00}, 0, {0x50,0x10,0x20,0x10,0x40,0x10,0x20,0x10}, {0x90,0x90,0x00,0x90,0x90,0x90,0x00,0x90}, 2, PB_CHORD, PB_LEAD, 15},
    {"DEMBOW", "DEMBO", {90, 100}, {0, 4}, 2, {PB_SC_MIN, PB_SC_HARM}, 3, {{0, 5, 2, 6}, {0, 3, 4, 4}, {0, 6, 5, 4}}, 20,
     {0x90,0x00,0x90,0x00,0x90,0x00,0x90,0x00}, {0x00,0x09,0x00,0x90,0x00,0x09,0x00,0x90}, PB_Z, {0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30},
     {0x00,0x09,0x00,0x90,0x00,0x09,0x00,0x90}, {0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30}, PB_Z, PB_Z, {0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x00},
     {0x90,0x03,0x00,0x90,0x90,0x03,0x00,0x90}, 10, 15, 5, 2, 0, 0, {0x00,0x09,0x00,0x90,0x00,0x09,0x00,0x90}, 1, {0x70,0x20,0x30,0x20,0x50,0x20,0x30,0x00}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 2, PB_CHORD, PB_LEAD, 15},
    {"GARAGE", "GARAG", {130, 136}, {12, 22}, 2, {PB_SC_MIN, PB_SC_DOR}, 3, {{0, 5, 3, 4}, {0, 2, 5, 6}, {1, 4, 0, 0}}, 70,
     {0x90,0x00,0x00,0x03,0x00,0x90,0x00,0x00}, {0x00,0x00,0x90,0x00,0x00,0x00,0x90,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x90,0x00}, {0x30,0x90,0x30,0x90,0x30,0x90,0x30,0x92}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30},
     {0x00,0x10,0x00,0x01,0x00,0x00,0x00,0x10}, {0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25}, PB_Z, PB_Z, PB_Z,
     {0x90,0x00,0x03,0x00,0x90,0x30,0x03,0x00}, 15, 10, 10, 2, 0, 0, {0x00,0x30,0x00,0x09,0x00,0x30,0x00,0x09}, 1, {0x50,0x20,0x00,0x20,0x30,0x20,0x00,0x20}, {0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90}, 2, PB_CHORD, PB_LEAD, 15}
};
static const uint8_t PB_SCALE[PB_SCALES][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10},
    {0, 2, 3, 5, 7, 8, 11}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11}
};

/* ---- random numbers (mulberry32) ---- */
static uint32_t pb_rnd(uint32_t *st)
{
    uint32_t t = (*st += 0x6D2B79F5u);
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}
static uint32_t pb_rn(uint32_t *st, uint32_t n) { return ((pb_rnd(st) >> 16) * n) >> 16; }   /* 0..n-1, n < 65536 */
static uint32_t pb_pct(uint32_t *st, uint32_t p) { return pb_rn(st, 100u) < p ? 1u : 0u; }
/* a weight's priority, 1..255: '9' always plays, the others get a per-hit spread so that patterns differ */
static uint32_t pb_prio(uint32_t w, uint32_t *st)
{
    uint32_t p;
    if (w >= 9u) return 255u;
    p = (w * 28u * (141u + pb_rn(st, 154u))) >> 8;
    return p > 250u ? 250u : p ? p : 1u;
}
static uint32_t pb_wt(const uint8_t *w, uint32_t s) { return ((uint32_t)w[(s & 15u) >> 1] >> ((s & 1u) ? 0u : 4u)) & 15u; }

const char *pb_genre_name(uint32_t g) { return PB_G[g % PB_GENRES].name; }
const char *pb_genre_tag(uint32_t g) { return PB_G[g % PB_GENRES].tag; }

/* ---- pitch ---- */
uint32_t pb_note(const pb_song_t *s, int32_t deg)
{
    int32_t ro = s->root > 6u ? (int32_t)s->root - 12 : (int32_t)s->root;
    int32_t oct = (deg + 700) / 7 - 100, i = deg - oct * 7, n = ro + 12 * oct + PB_SCALE[s->scale % PB_SCALES][i];
    return (uint32_t)(n < 0 ? 0 : n > 127 ? 127 : n);
}
static int32_t pb_note_deg(const pb_song_t *s, uint32_t note)
{
    int32_t ro = s->root > 6u ? (int32_t)s->root - 12 : (int32_t)s->root, rel = (int32_t)note - ro;
    int32_t oct = (rel + 1200) / 12 - 100, pc = rel - 12 * oct, best = 0, bd = 99, i;
    for (i = 0; i < 7; i++) {
        int32_t d = (int32_t)PB_SCALE[s->scale % PB_SCALES][i] - pc;
        if (d < 0) d = -d;
        if (d < bd) { bd = d; best = i; }
    }
    return 12 - pc < bd ? (oct + 1) * 7 : oct * 7 + best;
}
uint32_t pb_snap(const pb_song_t *s, uint32_t note) { return pb_note(s, pb_note_deg(s, note)); }
static uint32_t pb_fold(uint32_t n, uint32_t lo, uint32_t hi)
{
    while (n >= hi) n -= 12u;
    while (n < lo) n += 12u;
    return n;
}
static int pb_ct(int32_t d, int32_t c) { int32_t m = ((d - c) % 7 + 7) % 7; return m == 0 || m == 2 || m == 4; }
static int32_t pb_snapc(int32_t d, int32_t c)
{
    int32_t o;
    for (o = 0; o < 4; o++) {
        if (pb_ct(d + o, c)) return d + o;
        if (pb_ct(d - o, c)) return d - o;
    }
    return d;
}

/* ---- the sketch ---- */
void pb_make_song(pb_song_t *s, uint32_t ga, uint32_t gb, uint32_t blend, uint32_t seed)
{
    uint32_t st, t, i, a, b;
    const pb_genre_t *A, *B, *P;
    ga %= PB_GENRES; gb %= PB_GENRES; if (blend > 7u) blend = 7u;
    seed &= 0x7FFFFu;
    A = &PB_G[ga]; B = &PB_G[gb];
    st = (seed + 1u) * 7919u + ga * 131u + gb * 17u + blend;
    t = blend * 100u / 14u;                                  /* percent of the way to B, 0..50 */
    s->ga = (uint8_t)ga; s->gb = (uint8_t)gb; s->blend = (uint8_t)blend; s->seed = seed;
    a = A->bpm[0] + pb_rn(&st, A->bpm[1] - A->bpm[0] + 1u); b = B->bpm[0] + pb_rn(&st, B->bpm[1] - B->bpm[0] + 1u);
    s->bpm = (uint16_t)((a * (100u - t) + b * t + 50u) / 100u);
    a = A->sw[0] + pb_rn(&st, A->sw[1] - A->sw[0] + 1u); b = B->sw[0] + pb_rn(&st, B->sw[1] - B->sw[0] + 1u);
    s->swing = (uint8_t)((a * (100u - t) + b * t + 50u) / 100u);
    s->root = (uint8_t)pb_rn(&st, 12u);
    s->scale = A->sc[pb_rn(&st, A->nsc)];
    s->prog_src = (uint8_t)(pb_pct(&st, t) ? gb : ga);
    P = &PB_G[s->prog_src];
    s->prog_set = (uint8_t)pb_rn(&st, P->nprog);
    for (i = 0; i < 4u; i++) s->prog[i] = P->prog[s->prog_set][i];
    s->sev = (uint8_t)pb_pct(&st, A->sev);
    for (i = 0; i < 4u; i++) s->src[i] = (uint8_t)(pb_pct(&st, t) ? gb : ga);
    if (blend && s->src[0] == ga && s->src[1] == ga && s->src[2] == ga && s->src[3] == ga) s->src[2] = s->src[0] = (uint8_t)gb;
    s->role[0] = PB_BASS; s->role[1] = PB_G[s->src[1]].keys; s->role[2] = PB_G[s->src[2]].lead;
    s->awake = 0;
    for (i = 0; i < 4u; i++) {
        s->tseed[i] = pb_rnd(&st);
        if (PB_G[s->src[i]].awake & (1u << i)) s->awake |= (uint8_t)(1u << i);
    }
    if (!s->awake) s->awake = 1u;
}
void pb_next_chords(pb_song_t *s)
{
    const pb_genre_t *P = &PB_G[s->prog_src % PB_GENRES];
    uint32_t i;
    s->prog_set = (uint8_t)((s->prog_set + 1u) % P->nprog);
    for (i = 0; i < 4u; i++) s->prog[i] = P->prog[s->prog_set][i];
}

/* ---- drums ---- */
void pb_gen_drums(const pb_song_t *s, uint32_t genre, uint32_t seed, uint32_t energy, pb_dstep_t out[PB_STEPS])
{
    static const uint8_t LANE[10] = {0, 2, 3, 4, 5, 7, 13, 9, 10, 15};
    const pb_genre_t *g = &PB_G[genre % PB_GENRES];
    const uint8_t *rows[10];
    uint32_t st = seed, thr = 255u - (energy > 255u ? 255u : energy), v, r, i, b;
    pb_dstep_t bar[2][16];
    (void)s;
    rows[0] = g->k; rows[1] = g->s; rows[2] = g->c; rows[3] = g->h; rows[4] = g->o;
    rows[5] = g->rim; rows[6] = g->shk; rows[7] = g->tl; rows[8] = g->th; rows[9] = g->cb;
    for (v = 0; v < 2u; v++) {                 /* two bar variants: bars 1 and 3, bars 2 and 4 */
        for (i = 0; i < 16u; i++) bar[v][i].on = bar[v][i].ghost = 0;
        for (r = 0; r < 10u; r++)
            for (i = 0; i < 16u; i++) {
                uint32_t w = pb_wt(rows[r], i);
                if (w && pb_prio(w, &st) >= thr) {
                    bar[v][i].on |= (uint16_t)(1u << LANE[r]);
                    if (w < 4u) bar[v][i].ghost |= (uint16_t)(1u << LANE[r]);
                }
            }
    }
    for (b = 0; b < 4u; b++) for (i = 0; i < 16u; i++) out[b * 16u + i] = bar[b & 1u][i];
    for (i = 60; i < 64u; i++) {               /* a fill into the loop point, at higher energy */
        uint32_t take = pb_pct(&st, 70u), lane = (uint32_t)("\2\2\11\12"[pb_rn(&st, 4u)]), p = 115u + pb_rn(&st, 77u);
        if (take && p >= thr) out[i].on |= (uint16_t)(1u << lane);
    }
}

/* ---- melodic parts ---- */
static void pb_clear(pb_ev_t out[PB_STEPS])
{
    uint32_t i, k;
    for (i = 0; i < PB_STEPS; i++) { for (k = 0; k < 4u; k++) out[i].note[k] = 0; out[i].n = 0; out[i].vel = 0; out[i].gate = 1; out[i].slide = 0; }
}
static void pb_put(pb_ev_t *e, uint32_t note, uint32_t vel, uint32_t gate)
{
    if (e->n >= 4u) return;
    e->note[e->n++] = (uint8_t)note; e->vel = (uint8_t)vel; e->gate = (uint8_t)(gate < 1u ? 1u : gate > 16u ? 16u : gate);
}
/* a gate of 0 means "until the next note of the part": fix those up once the part is complete */
static void pb_open(pb_ev_t out[PB_STEPS], uint32_t max)
{
    uint32_t i, j;
    for (i = 0; i < PB_STEPS; i++) {
        if (!out[i].n || out[i].gate != 16u || !(out[i].slide & 0x80u)) continue;
        out[i].slide &= 0x7Fu;
        for (j = i + 1u; j < PB_STEPS && (j & ~15u) == (i & ~15u) && !out[j].n; j++) {}
        out[i].gate = (uint8_t)(j - i > max ? max : j - i);
    }
}
static void pb_g_bass(const pb_song_t *s, const pb_genre_t *g, uint32_t st, uint32_t thr, pb_ev_t out[PB_STEPS])
{
    const uint8_t *w = g->b_follow ? PB_G[s->src[3] % PB_GENRES].k : g->bw;
    struct { uint8_t s, on, acc, sl; int8_t off; } m[16];
    uint32_t n = 0, i, bar;
    static const int8_t COL[4] = {2, 6, -1, 3};
    for (i = 0; i < 16u; i++) {
        uint32_t wv = pb_wt(w, i), p, r;
        int32_t off = 0;
        if (!wv) continue;
        p = pb_prio(wv, &st);
        if (g->b_follow && p < 128u) p = 128u;
        r = pb_rn(&st, 100u);
        if (i) {
            if (r < g->b_oct) off = 7;
            else if (r < (uint32_t)g->b_oct + g->b_fifth) off = 4;
            else if (r < (uint32_t)g->b_oct + g->b_fifth + g->b_col) off = COL[pb_rn(&st, 4u)];
        }
        m[n].s = (uint8_t)i; m[n].off = (int8_t)off; m[n].on = (uint8_t)(p >= thr || !i);
        m[n].sl = (uint8_t)pb_pct(&st, g->b_slide); m[n].acc = (uint8_t)pb_pct(&st, 30u); n++;
    }
    if (!n) { m[0].s = 0; m[0].off = 0; m[0].on = 1; m[0].sl = 0; m[0].acc = 1; n = 1; }
    for (bar = 0; bar < 4u; bar++) {
        int32_t c = s->prog[bar];
        uint32_t r0 = pb_fold(pb_note(s, 21 + c), 28u, 40u);
        for (i = 0; i < n; i++) {
            pb_ev_t *e = &out[bar * 16u + m[i].s];
            if (!m[i].on) continue;
            pb_put(e, r0 + pb_note(s, 21 + c + m[i].off) - pb_note(s, 21 + c), m[i].acc ? 118u : 96u, g->b_gate ? g->b_gate : 16u);
            e->slide = (uint8_t)(m[i].sl | (g->b_gate ? 0u : 0x80u));
        }
    }
    if (pb_pct(&st, 60u) && !out[62].n && 140u >= thr) pb_put(&out[62], pb_fold(pb_note(s, 21 + (int32_t)s->prog[0] - 1), 28u, 40u), 90u, 2u);
    pb_open(out, 16u);
}
static uint32_t pb_tones(const pb_song_t *s, int32_t c, uint32_t lo, uint32_t hi, uint8_t t[4])
{
    static const int8_t T[4] = {0, 2, 4, 6};
    uint32_t n = s->sev ? 4u : 3u, i;
    for (i = 0; i < n; i++) t[i] = (uint8_t)pb_fold(pb_note(s, 28 + c + T[i]), lo, hi);
    return n;
}
static void pb_g_chord(const pb_song_t *s, const pb_genre_t *g, uint32_t st, uint32_t thr, pb_ev_t out[PB_STEPS])
{
    uint8_t on[16], t[4];
    uint32_t i, bar, k, n;
    for (i = 0; i < 16u; i++) { uint32_t wv = pb_wt(g->cw, i); on[i] = (uint8_t)(wv && (pb_prio(wv, &st) >= thr || !i)); }
    for (bar = 0; bar < 4u; bar++) {
        n = pb_tones(s, s->prog[bar], 53u, 65u, t);
        for (i = 0; i < 16u; i++) {
            pb_ev_t *e = &out[bar * 16u + i];
            if (!on[i]) continue;
            for (k = 0; k < n; k++) pb_put(e, t[k], 84u, g->c_gate ? g->c_gate : 16u);
            if (!g->c_gate) e->slide = 0x80u;
        }
    }
    pb_open(out, 16u);
}
static void pb_g_pad(const pb_song_t *s, uint32_t st, pb_ev_t out[PB_STEPS])
{
    uint8_t t[4];
    uint32_t bar, k, n, wide = pb_pct(&st, 50u);
    for (bar = 0; bar < 4u; bar++) {
        pb_ev_t *e = &out[bar * 16u];
        n = pb_tones(s, s->prog[bar], 60u, 72u, t);
        if (wide) t[1] = (uint8_t)(t[1] + 12u > 79u ? t[1] : t[1] + 12u);
        if (n < 4u) pb_put(e, pb_fold(pb_note(s, 28 + (int32_t)s->prog[bar]), 48u, 60u), 66u, 16u);
        for (k = 0; k < n; k++) pb_put(e, t[k], 62u, 16u);
    }
}
static void pb_lead_half(const pb_song_t *s, const pb_genre_t *g, uint32_t st, uint32_t thr, uint32_t bar0, int final, pb_ev_t out[PB_STEPS])
{
    static const int8_t START[3] = {0, 2, 4}, JUMP[7] = {-2, -1, -1, 1, 1, 2, 0};
    struct { uint8_t s, p; } on[32];
    uint32_t n = 0, i, b;
    int32_t d = START[pb_rn(&st, 3u)];
    if (pb_pct(&st, 30u)) d += 7;
    for (b = 0; b < 2u; b++) for (i = 0; i < 16u; i++) {
        uint32_t wv = pb_wt(g->lw, i);
        if (wv) { on[n].s = (uint8_t)(b * 16u + i); on[n].p = (uint8_t)pb_prio(wv, &st); n++; }
    }
    for (i = 0; i < n; i++) {
        int32_t c = s->prog[bar0 + (on[i].s >> 4)], jump = JUMP[pb_rn(&st, 7u)], dd;
        uint32_t shortn = pb_pct(&st, 30u), beat = (on[i].s & 3u) == 0u, p = on[i].p, gate, next = i + 1u < n ? on[i + 1u].s : 32u;
        if (i) d += jump;
        if (beat || !i) d = pb_snapc(d, c);
        if (d < -2) d = -2;
        if (d > 11) d = 11;
        gate = next - on[i].s; if (gate > 4u) gate = 4u;
        if (shortn) gate = 1u;
        dd = d;
        if (final && i == n - 1u) { dd = pb_snapc(d, c); gate = 4u; }
        if (beat && p < 200u) p = 200u;      /* notes on the beat survive low energy */
        if (!i) p = 255u;                    /* and the phrase always has its first note */
        if (p >= thr) pb_put(&out[bar0 * 16u + on[i].s], pb_note(s, 35 + dd), beat ? 108u : 90u, gate);
    }
}
static void pb_g_arp(const pb_song_t *s, const pb_genre_t *g, uint32_t st, uint32_t thr, pb_ev_t out[PB_STEPS])
{
    static const uint8_t SEQ[5][8] = {{0, 1, 2, 3, 0, 1, 2, 3}, {3, 2, 1, 0, 3, 2, 1, 0}, {0, 1, 2, 3, 2, 1, 0, 1}, {0, 2, 1, 3, 0, 2, 1, 3}, {0, 3, 1, 2, 0, 2, 1, 3}};
    static const int8_t TRI[4] = {0, 2, 4, 7}, SEV[4] = {0, 2, 4, 6};
    const int8_t *t = s->sev ? SEV : TRI;
    const uint8_t *seq = SEQ[pb_rn(&st, 5u)];
    int32_t hi = pb_pct(&st, 40u) ? 7 : 0;
    uint32_t bar, i, k, rate = g->a_rate ? g->a_rate : 1u;
    for (bar = 0; bar < 4u; bar++)
        for (i = 0, k = 0; i < 16u; i += rate, k++) {
            uint32_t strong = rate == 1u ? (i & 1u) == 0u : (i & 3u) == 0u;
            if (!pb_wt(g->aw, i) || (strong ? 255u : 140u) < thr) continue;
            pb_put(&out[bar * 16u + i], pb_fold(pb_note(s, 35 + (int32_t)s->prog[bar] + t[seq[k & 7u]] + ((k & 7u) > 3u ? hi : 0)), 60u, 86u),
                (i & 3u) ? 80u : 100u, rate);
        }
}
void pb_gen_part(const pb_song_t *s, uint32_t role, uint32_t genre, uint32_t seed, uint32_t energy, pb_ev_t out[PB_STEPS])
{
    const pb_genre_t *g = &PB_G[genre % PB_GENRES];
    uint32_t thr = 255u - (energy > 255u ? 255u : energy);
    pb_clear(out);
    switch (role) {
    case PB_BASS: pb_g_bass(s, g, seed, thr, out); break;
    case PB_CHORD: pb_g_chord(s, g, seed, thr, out); break;
    case PB_PAD: pb_g_pad(s, seed, out); break;
    case PB_LEAD: pb_lead_half(s, g, seed, thr, 0, 0, out); pb_lead_half(s, g, seed, thr, 2, 1, out); break;   /* same seed twice: */
    default: pb_g_arp(s, g, seed, thr, out); break;                                                      /* the phrase repeats over new chords */
    }
}

/* ---- recommendations ---- */
uint32_t pb_fit(const pb_song_t *s, uint32_t role, const pb_ev_t c[PB_STEPS], const pb_dstep_t dr[PB_STEPS],
                const pb_ev_t *oa, const pb_ev_t *ob)
{
    static const uint8_t TARGET[PB_ROLES] = {22, 10, 4, 16, 40};
    uint32_t n = 0, kick = 0, clash = 0, ct = 0, cn = 0, sum = 0, cnt = 0, i, k, dens, score, others = 0, d;
    for (i = 0; i < PB_STEPS; i++) {
        if (!c[i].n) continue;
        n++;
        if (dr && (dr[i].on & 3u)) kick++;
        if (oa && oa[i].n) clash++;
        if (ob && ob[i].n) clash++;
        for (k = 0; k < c[i].n; k++) { sum += c[i].note[k]; cnt++; }
        if (!(i & 3u)) { cn++; if (pb_ct(pb_note_deg(s, c[i].note[0]), s->prog[i >> 4])) ct++; }
    }
    if (!n) return 40u;
    d = n > TARGET[role % PB_ROLES] ? n - TARGET[role % PB_ROLES] : TARGET[role % PB_ROLES] - n;
    dens = d >= TARGET[role % PB_ROLES] + 6u ? 0u : 100u - d * 100u / (TARGET[role % PB_ROLES] + 6u);
    score = 45u + 22u * dens / 100u;
    others = (oa ? 1u : 0u) + (ob ? 1u : 0u);
    if (others) score += (role == PB_BASS ? 4u : 12u) * (100u - clash * 100u / (n * others)) / 100u;
    if (role == PB_BASS) score += 12u * kick / n;
    if (cn) score += 12u * ct / cn;
    for (k = 0; k < 2u; k++) {
        const pb_ev_t *o = k ? ob : oa;
        uint32_t s2 = 0, c2 = 0, j, m, mean = sum / cnt, far;
        if (!o) continue;
        for (i = 0; i < PB_STEPS; i++) for (j = 0; j < o[i].n; j++) { s2 += o[i].note[j]; c2++; }
        if (!c2) continue;
        m = s2 / c2; far = m > mean ? m - mean : mean - m;
        score += far > 6u ? 4u : far > 3u ? 2u : 0u;
    }
    return score > 99u ? 99u : score < 40u ? 40u : score;
}

/* ---- passwords ---- */
static const char PB_ALPHA[33] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
void pb_encode(const pb_song_t *s, char out[7])
{
    uint32_t v = s->ga | ((uint32_t)s->gb << 4) | ((uint32_t)s->blend << 8) | ((s->seed & 0x7FFFFu) << 11), i;
    for (i = 0; i < 6u; i++) { out[i] = PB_ALPHA[v & 31u]; v >>= 5; }
    out[6] = 0;
}
int pb_decode(const char *pw, uint32_t *ga, uint32_t *gb, uint32_t *blend, uint32_t *seed)
{
    uint32_t v = 0, i, k;
    for (i = 6; i-- > 0u;) {
        for (k = 0; k < 32u && PB_ALPHA[k] != pw[i]; k++) {}
        if (k == 32u) return 0;
        v = (v << 5) | k;
    }
    *ga = v & 15u; *gb = (v >> 4) & 15u; *blend = (v >> 8) & 7u; *seed = v >> 11;
    return *ga < PB_GENRES && *gb < PB_GENRES;
}
