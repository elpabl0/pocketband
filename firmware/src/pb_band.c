/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band: the band. The map of genre towns, the four members' sprites, and writing a sketch (patterns,
 * sounds, kit, key, tempo) into the four tracks from the music brain (pb_brain.c). */
#include "pb_brain.c"

enum { PS_STAGE, PS_MAP, PS_AUDITION, PS_GRID, PS_SOUND, PS_JAM, PS_PERFORM, PS_KEY, PS_VIS, PS_CART, PS_N };
static struct {
    uint8_t scr, have, tiles_ok;
    uint8_t energy;                  /* 0..20 */
    uint8_t town, cur;
    uint8_t idea[NTRK];              /* which idea of the part's seed plays */
    uint8_t fit[NTRK];
    int16_t mx, my;                  /* the walker on the map, pixels */
    char entry[7];
    pb_song_t s;
} pb = {.energy = 12, .mx = 92, .my = 116, .town = 9, .entry = "AAAAAA"};
static uint8_t pb_tiles[30 * 20];             /* (ordinary RAM: the pool keeps its headroom) */
static pb_ev_t pb_part[NPART][PB_STEPS];
static pb_dstep_t pb_drum[PB_STEPS];

/* genre town on the map: genre, tile x, tile y (30 x 20 tiles of 8 px under the header) */
static const uint8_t PB_TOWN[PB_GENRES][3] = {
    {9, 3, 2}, {11, 10, 3}, {3, 17, 2}, {4, 25, 3}, {6, 5, 7}, {10, 12, 7}, {12, 19, 7},
    {13, 26, 8}, {7, 3, 12}, {0, 11, 12}, {1, 18, 12}, {5, 25, 13}, {8, 7, 17}, {2, 15, 17}
};
/* each genre's sounds by role (bass, chord, pad, lead, arp: names of SLOOP's factory bank) and its drum kit */
static const char *const PB_SND[PB_GENRES][6] = {
    {"FM BASS", "MIN7 STAB", "WARM PAD", "SUPERSAW", "FM PLUCK", "HOUSE"},
    {"MONO BASS", "RAVE STAB", "DARK STR", "SYNC LEAD", "RESO PLUCK", "TECHNO"},
    {"ACID 303", "DUB CHORD", "ATMOS PAD", "SQR LEAD", "RESO PLUCK", "606"},
    {"DEEP BASS", "RHODES", "SOUL OOH", "LOFI FLUTE", "VIBES", "BOOMBAP"},
    {"808 SLIDE", "M1 PIANO", "GLASS PAD", "TRAP BELL", "TRAP PLUCK", "TRAP"},
    {"REESE", "SOFT KEYS", "DARK STR", "GLASS BELL", "FM PLUCK", "JUNGLE"},
    {"SUB BASS", "REGGAE ORG", "WARM PAD", "PAN FLUTE", "MUTED GTR", "VINTAGE"},
    {"SAW BASS", "SYN BRASS", "ENSEMBLE", "FAT LEAD", "FM PLUCK", "SYNTHWV"},
    {"NES BASS", "CHIP CHORD", "PULSE PAD", "GAME LEAD", "8BIT ARP", "CHIP"},
    {"SUB BASS", "SOFT KEYS", "SHIMMER", "GLASS BELL", "KALIMBA", "AMBIENT"},
    {"FUNK BASS", "CLAV", "SOUL ORGAN", "G-FUNK LD", "MUTED GTR", "DISCO"},
    {"DEEP BASS", "LOFI KEYS", "LOFI CLOUD", "LOFI FLUTE", "VIBES", "LO-FI"},
    {"808 BOOM", "NYLON GTR", "VOX PAD", "PAN FLUTE", "MARIMBA", "DEMBOW"},
    {"REESE", "HOUSE ORGN", "DARK STR", "PLUGG BELL", "VIBES", "GARAGE"}
};
static const uint8_t PB_SCALE_OF[PB_SCALES] = {1, 2, 3, 4, 7, 8, 9};   /* PB_SC_* -> SLOOP's P_SCALE (N_SCALE) */
static const char *const PB_ROLE[PB_ROLES] = {"BASS", "CHORDS", "PAD", "LEAD", "ARP"};
static const char *const PB_MEMBER[NTRK] = {"BASS", "KEYS", "LEAD", "DRUMS"};
static const char *const PB_NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const PB_SCN[PB_SCALES] = {"MAJ", "MIN", "DOR", "MIX", "HARM", "PHRY", "LYD"};

/* the overlay's own colours (a fixed skin: the song's theme does not reach it) */
#define PBC_BG RGB(11, 16, 32)
#define PBC_DIM RGB(31, 42, 77)
#define PBC_MID RGB(74, 95, 168)
#define PBC_INK RGB(223, 231, 255)
#define PBC_HOT RGB(255, 93, 143)
#define PBC_GOLD RGB(255, 209, 102)
#define PBC_MINT RGB(77, 224, 192)
static const uint16_t PB_BODY[NTRK] = {PBC_MINT, PBC_GOLD, PBC_HOT, PBC_HOT};

/* 16 x 16 sprites, a nibble a pixel: 0 none, 1 outline, 2 body, 3 highlight, 4 shade, 5 eye. Bass, keys, lead, drums
 * (drawn in tools-free form: see the browser prototype's gfx.js for the same art as text) */
static const uint8_t PB_ART[NTRK][16][8] = {
    {{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x01,0x11,0x11,0x10,0x00,0x00}, {0x00,0x01,0x12,0x22,0x22,0x21,0x10,0x00}, {0x00,0x12,0x22,0x22,0x22,0x22,0x21,0x00}, {0x01,0x22,0x32,0x22,0x22,0x22,0x22,0x10}, {0x01,0x22,0x22,0x22,0x22,0x22,0x22,0x10}, {0x12,0x22,0x52,0x22,0x22,0x25,0x22,0x21}, {0x12,0x22,0x52,0x22,0x22,0x25,0x22,0x21}, {0x12,0x22,0x22,0x21,0x12,0x22,0x22,0x21}, {0x12,0x22,0x22,0x22,0x22,0x22,0x22,0x21}, {0x14,0x22,0x22,0x22,0x22,0x22,0x22,0x41}, {0x01,0x44,0x22,0x22,0x22,0x22,0x44,0x10}, {0x00,0x11,0x44,0x44,0x44,0x44,0x11,0x00}, {0x00,0x00,0x11,0x11,0x11,0x11,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {{0x00,0x10,0x00,0x00,0x00,0x01,0x00,0x00}, {0x01,0x21,0x00,0x00,0x00,0x12,0x10,0x00}, {0x01,0x22,0x10,0x00,0x01,0x22,0x10,0x00}, {0x01,0x22,0x21,0x11,0x12,0x22,0x10,0x00}, {0x01,0x22,0x22,0x22,0x22,0x22,0x10,0x00}, {0x01,0x22,0x52,0x22,0x25,0x22,0x10,0x00}, {0x01,0x22,0x52,0x22,0x25,0x22,0x10,0x00}, {0x01,0x22,0x22,0x33,0x22,0x22,0x10,0x00}, {0x01,0x22,0x22,0x22,0x22,0x22,0x10,0x10}, {0x00,0x12,0x22,0x22,0x22,0x21,0x01,0x21}, {0x00,0x12,0x22,0x22,0x22,0x21,0x12,0x10}, {0x01,0x22,0x22,0x22,0x22,0x22,0x21,0x00}, {0x01,0x24,0x22,0x22,0x22,0x42,0x10,0x00}, {0x01,0x24,0x21,0x11,0x12,0x42,0x10,0x00}, {0x01,0x11,0x10,0x00,0x01,0x11,0x10,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {{0x00,0x00,0x00,0x01,0x10,0x00,0x00,0x00}, {0x00,0x00,0x00,0x12,0x21,0x00,0x00,0x00}, {0x00,0x00,0x00,0x12,0x21,0x00,0x00,0x00}, {0x01,0x11,0x11,0x22,0x22,0x11,0x11,0x10}, {0x01,0x22,0x22,0x22,0x22,0x22,0x22,0x10}, {0x00,0x12,0x25,0x22,0x22,0x52,0x21,0x00}, {0x00,0x01,0x25,0x22,0x22,0x52,0x10,0x00}, {0x00,0x01,0x22,0x23,0x32,0x22,0x10,0x00}, {0x00,0x12,0x22,0x22,0x22,0x22,0x21,0x00}, {0x00,0x12,0x22,0x22,0x22,0x22,0x21,0x00}, {0x01,0x22,0x22,0x21,0x12,0x22,0x22,0x10}, {0x01,0x22,0x21,0x10,0x01,0x12,0x22,0x10}, {0x01,0x21,0x10,0x00,0x00,0x01,0x12,0x10}, {0x01,0x10,0x00,0x00,0x00,0x00,0x01,0x10}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {{0x00,0x00,0x00,0x10,0x01,0x00,0x00,0x00}, {0x00,0x00,0x00,0x10,0x01,0x00,0x00,0x00}, {0x00,0x01,0x11,0x11,0x11,0x11,0x10,0x00}, {0x00,0x12,0x22,0x22,0x22,0x22,0x21,0x00}, {0x00,0x12,0x33,0x22,0x22,0x33,0x21,0x00}, {0x00,0x12,0x35,0x22,0x22,0x53,0x21,0x00}, {0x00,0x12,0x22,0x22,0x22,0x22,0x21,0x00}, {0x00,0x12,0x21,0x11,0x11,0x12,0x21,0x00}, {0x00,0x12,0x22,0x22,0x22,0x22,0x21,0x00}, {0x00,0x01,0x11,0x11,0x11,0x11,0x10,0x00}, {0x00,0x11,0x42,0x22,0x22,0x24,0x11,0x00}, {0x01,0x21,0x42,0x22,0x22,0x24,0x12,0x10}, {0x01,0x11,0x42,0x22,0x22,0x24,0x11,0x10}, {0x00,0x00,0x11,0x11,0x11,0x11,0x00,0x00}, {0x00,0x00,0x12,0x10,0x01,0x21,0x00,0x00}, {0x00,0x00,0x11,0x10,0x01,0x11,0x00,0x00}},
};
UI_SMALL static void pb_sprite(uint32_t i, int32_t x, int32_t y, int32_t sc, int asleep)
{
    uint16_t body = asleep ? PBC_MID : PB_BODY[i % NTRK];
    uint32_t r, c;
    for (r = 0; r < 16u; r++)
        for (c = 0; c < 16u; c++) {
            uint32_t ch = (PB_ART[i % NTRK][r][c >> 1] >> ((c & 1u) ? 0u : 4u)) & 15u;
            uint16_t col;
            if (!ch)
                continue;
            col = ch == 1u ? PBC_INK : ch == 2u ? body : ch == 3u ? (asleep ? PBC_DIM : RGB(255, 255, 255))
                : ch == 4u ? (asleep ? PBC_DIM : PBC_MID) : (asleep && !(r & 1u) ? body : PBC_INK);
            cv_rect(x + (int32_t)c * sc, y + (int32_t)r * sc, sc, sc, col);
        }
}
UI_SMALL static void pb_txt(int32_t x, int32_t y, const char *s, uint16_t c) { cv_text(x, y, &FONT_S, s, c); }
UI_SMALL static void pb_txtc(int32_t cx, int32_t y, const char *s, uint16_t c) { cv_text(cx - text_w(&FONT_S, s) / 2, y, &FONT_S, s, c); }
UI_SMALL static void pb_num(int32_t x, int32_t y, int32_t v, uint16_t c)
{
    char b[12];
    fmt_int(b, v);
    pb_txt(x, y, b, c);
}

/* ---- the map ---- */
UI_SMALL static uint32_t pb_hash(uint32_t a, uint32_t b)
{
    uint32_t h = (a * 0x9E3779B1u) ^ ((b + 0x7F4A7C15u) * 0x85EBCA6Bu);
    h ^= h >> 13; h *= 0x5BD1E995u; h ^= h >> 15;
    return h & 255u;
}
UI_SMALL static void pb_map_build(void)
{
    uint32_t x, y, i;
    for (y = 0; y < 20u; y++)
        for (x = 0; x < 30u; x++) {          /* value noise on a 5-tile lattice, a little grain, flat land round the towns */
            uint32_t x0 = x / 5u, y0 = y / 5u, u = x % 5u, v = y % 5u, near = 99;
            uint32_t a = pb_hash(x0 * 31u + 7u, y0 * 17u + 3u), b = pb_hash(x0 * 31u + 38u, y0 * 17u + 3u);
            uint32_t c = pb_hash(x0 * 31u + 7u, y0 * 17u + 20u), d = pb_hash(x0 * 31u + 38u, y0 * 17u + 20u);
            uint32_t h = ((a * (5u - u) + b * u) * (5u - v) + (c * (5u - u) + d * u) * v) / 25u + pb_hash(x, y) / 4u;
            for (i = 0; i < PB_GENRES; i++) {
                uint32_t dx = x > PB_TOWN[i][1] ? x - PB_TOWN[i][1] : PB_TOWN[i][1] - x, dy = y > PB_TOWN[i][2] ? y - PB_TOWN[i][2] : PB_TOWN[i][2] - y;
                if (dx + dy < near)
                    near = dx + dy;
            }
            pb_tiles[y * 30u + x] = (uint8_t)(near <= 1u ? 2u : h < 112u ? 0u : h < 128u ? 1u : h < 200u ? 2u : h < 240u ? 3u : 4u);
        }
    pb.tiles_ok = 1;
}
UI_SMALL static uint32_t pb_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 14;
    for (; b; b >>= 1)
        if ((r + b) * (r + b) <= v)
            r += b;
    return r;
}
/* the two nearest towns and how far towards the second the walker stands (0 = in the first one) */
UI_SMALL static void pb_mix(uint32_t *ta, uint32_t *tb, uint32_t *blend)
{
    uint32_t i, d1 = 0xFFFFFFFFu, d2 = 0xFFFFFFFFu, a = 0, b = 0;
    for (i = 0; i < PB_GENRES; i++) {
        int32_t dx = PB_TOWN[i][1] * 8 + 4 - pb.mx, dy = 16 + PB_TOWN[i][2] * 8 + 4 - pb.my;
        uint32_t d = (uint32_t)(dx * dx + dy * dy);
        if (d < d1) { d2 = d1; b = a; d1 = d; a = i; }
        else if (d < d2) { d2 = d; b = i; }
    }
    d1 = pb_isqrt(d1); d2 = pb_isqrt(d2);
    *ta = a; *tb = b;
    *blend = d1 < 14u ? 0u : (uint32_t)clamp((int32_t)((d1 * 14u + (d1 + d2) / 2u) / (d1 + d2)), 1, 7);
}

/* ---- writing a sketch into SLOOP ---- */
UI_SMALL static uint32_t pb_energy(void) { return 51u + (uint32_t)pb.energy * 204u / 20u; }
UI_SMALL static uint32_t pb_seed_of(uint32_t t) { return pb.s.tseed[t % NTRK] + (uint32_t)pb.idea[t % NTRK] * 0x9E3779B1u; }
UI_SMALL static void pb_fits(void)
{
    uint32_t t;
    for (t = 0; t < NPART; t++)
        pb.fit[t] = (uint8_t)pb_fit(&pb.s, pb.s.role[t], pb_part[t], pb_drum, pb_part[(t + 1u) % NPART], pb_part[(t + 2u) % NPART]);
}
/* one member's part: generated outside the audio interrupt, written with it off */
UI_SMALL static void pb_write(uint32_t t)
{
    track_t *T = &trk[t % NTRK];
    uint32_t i, k, tie = 0;
    if (t == TRK_DRUM) {
        pb_gen_drums(&pb.s, pb.s.src[3], pb_seed_of(3), pb_energy(), pb_drum);
        fm1_irq_off();
        locks_restore(T);
        steps_clear(T);
        for (i = 0; i < PB_STEPS; i++) {
            dstep_t *d = &T->dstep[i];
            d->on[0] = (uint8_t)pb_drum[i].on;
            d->on[1] = (uint8_t)(pb_drum[i].on >> 8);
            for (k = 0; k < DRUM_LANES; k++)
                if ((pb_drum[i].ghost >> k) & 1u)
                    d->lvl[k / 4u] |= (uint8_t)(LV_GHOST << (2u * (k % 4u)));
        }
    } else {
        pb_ev_t *e = pb_part[t % NPART];
        pb_gen_part(&pb.s, pb.s.role[t], pb.s.src[t], pb_seed_of(t), pb_energy(), e);
        fm1_irq_off();
        locks_restore(T);
        steps_clear(T);
        for (i = 0; i < PB_STEPS; i++) {
            step_t *st = &T->step[i];
            if (e[i].n) {
                memset(st, 0, sizeof *st);
                memcpy(st->note, e[i].note, 4);
                st->n = e[i].n;
                st->time = ST_NOTE;
                st->vel = e[i].vel;
                st->flags = (uint8_t)((e[i].vel >= 110u ? SF_ACCENT : 0u) | ((e[i].slide & 1u) ? SF_SLIDE : 0u));
                tie = (uint32_t)e[i].gate - 1u;
            } else if (tie) {
                memset(st, 0, sizeof *st);
                st->time = ST_TIE;
                tie--;
            }
        }
    }
    T->p[P_SLEN] = NSTEP;
    T->p[P_SDIV] = 2;
    T->seq_active = 1;
    if (t != TRK_DRUM)
        panic_req |= (uint8_t)(1u << t);     /* a note the old pattern held is let go (as a sound change does) */
    fm1_irq_on();
}
UI_SMALL static void pb_sound(uint32_t t)
{
    const char *name = PB_SND[pb.s.src[t % NTRK] % PB_GENRES][t == TRK_DRUM ? 5u : pb.s.role[t]];
    track_t *T = &trk[t % NTRK];
    uint32_t i;
    if (t == TRK_DRUM) {
        for (i = 0; i < DRUM_KITS; i++)
            if (str_eq(DRUM_KIT_NAMES[i], name))
                T->p[P_E0] = (int16_t)i;
        return;
    }
    if (!bank_ready)
        bank_resolve();
    for (i = 0; i < NBANK; i++)
        if (str_eq(BANK[i].name, name) && bank_pi[i] != 0xFF) {
            if (BANK[i].e != T->eng_req)
                set_engine_of(T, BANK[i].e);
            apply_preset_to(T, bank_pi[i]);
            break;
        }
    T->p[P_AMODE] = 0;                       /* the part is written out: no arpeggiator, no chord mode on top */
    T->p[P_CHORD] = 0;
    if (pb.s.role[t] == PB_CHORD || pb.s.role[t] == PB_PAD)
        T->p[P_VOICE] = V_POLY;
}
UI_SMALL static void pb_key(void)
{
    uint32_t t;
    for (t = 0; t < NPART; t++) {
        trk[t].p[P_ROOT] = pb.s.root;
        trk[t].p[P_SCALE] = PB_SCALE_OF[pb.s.scale % PB_SCALES];
    }
}
UI_SMALL static void pb_travel(uint32_t ga, uint32_t gb, uint32_t blend, uint32_t seed)
{
    uint32_t t;
    pb_make_song(&pb.s, ga, gb, blend, seed);
    memset(pb.idea, 0, sizeof pb.idea);
    song.g[G_BPM] = (int16_t)clamp(pb.s.bpm, GP[G_BPM].min, GP[G_BPM].max);
    song.g[G_SWING] = (int16_t)clamp(pb.s.swing * 2, 0, 100);
    pb_key();
    for (t = 0; t < NTRK; t++) {
        pb_sound(t);
        trk[t].p[P_MUTE] = (int16_t)(!((pb.s.awake >> t) & 1u));
    }
    pb_write(TRK_DRUM);                      /* the drums first: a bass may follow the kick */
    for (t = 0; t < NPART; t++)
        pb_write(t);
    pb_fits();
    pb_encode(&pb.s, pb.entry);
    pb.have = 1;
    pb.scr = PS_STAGE;
    if (!song.playing)
        transport_req = 1;
    ui_say(pb_genre_name(ga), blend ? " MIX" : "");
}
UI_SMALL static void pb_go(void)
{
    uint32_t a, b, bl;
    pb_mix(&a, &b, &bl);
    pb_travel(PB_TOWN[a][0], bl ? PB_TOWN[b][0] : PB_TOWN[a][0], bl, (fm1_ticks() * 2654435761u) >> 13);
}
UI_SMALL static void pb_rewrite_all(void)
{
    uint32_t t;
    pb_write(TRK_DRUM);
    for (t = 0; t < NPART; t++)
        pb_write(t);
    pb_fits();
}

