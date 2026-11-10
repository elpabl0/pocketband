/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band: a generator on top of SLOOP. A full-screen overlay (as the visualiser is) with three
 * screens: the MAP (pick a place between genre towns and travel: a whole sketch is written into the four
 * tracks), the BAND (re-roll one member's part, change its role or where it comes from, the energy of the
 * whole band) and the CODE (the six-letter password of a sketch). Everything it writes is an ordinary SLOOP
 * pattern, sound and tempo: leave the overlay and edit it as any other.
 * HOME on the TRACKS screen opens it; HOME again goes on to the visualiser. */
#include "pb_brain.c"

enum { PBS_MAP, PBS_BAND, PBS_CODE, PBS_N };
static struct {
    uint8_t on, shown_last, screen, have, tiles_ok;
    uint8_t energy;                  /* 0..20 */
    uint8_t town, cur;
    uint8_t idea[NTRK];              /* which idea of the part's seed plays */
    uint8_t fit[NTRK];
    int16_t mx, my;                  /* the walker on the map, pixels */
    char entry[7];
    pb_song_t s;
} pb = {.energy = 12, .mx = 92, .my = 132, .town = 9, .entry = "AAAAAA"};
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
    pb.screen = PBS_BAND;
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

/* ---- drawing: two 240 x 120 bands, as the visualiser ---- */
UI_SMALL static void pb_head(const char *title)
{
    char b[24];
    cv_rect(0, 0, 240, 16, PBC_INK);
    pb_txt(3, 0, title, PBC_BG);
    fmt_int(b, song.g[G_BPM]);
    cv_text(236 - text_w(&FONT_S, b) - (song.playing ? 12 : 0), 0, &FONT_S, b, PBC_BG);
    if (song.playing) {
        int32_t i;
        for (i = 0; i < 5; i++)
            cv_rect(228 + i, 3 + i, 1, 10 - 2 * i, PBC_HOT);
    }
}
UI_SMALL static void pb_draw_map(void)
{
    uint32_t x, y, i, a, b, bl, f = ui.frame >> 4;
    char line[40];
    pb_head("WORLD MAP");
    for (y = 0; y < 20u; y++)
        for (x = 0; x < 30u; x++) {
            int32_t px = (int32_t)x * 8, py = 16 + (int32_t)y * 8;
            uint32_t tt = pb_tiles[y * 30u + x];
            if (py + 8 + cv_oy <= 0 || py + cv_oy >= 120)
                continue;
            if (tt == 0u) {
                uint32_t o = (f + x + y * 3u) & 7u;
                cv_rect(px, py, 8, 8, PBC_DIM);
                cv_rect(px + (int32_t)o, py + 2, 2, 1, PBC_MID);
                cv_rect(px + (int32_t)((o + 4u) & 7u), py + 6, 2, 1, PBC_MID);
            } else if (tt == 1u) {
                cv_pset(px + 2, py + 3, PBC_DIM); cv_pset(px + 6, py + 5, PBC_DIM);
            } else if (tt == 2u) {
                if ((x * 7u + y * 3u) % 3u == 0u)
                    cv_pset(px + 3, py + 5, PBC_DIM);
            } else if (tt == 3u) {
                cv_rect(px + 3, py + 1, 2, 1, PBC_MID); cv_rect(px + 2, py + 2, 4, 2, PBC_MID);
                cv_rect(px + 1, py + 4, 6, 2, PBC_MID); cv_rect(px + 3, py + 6, 2, 2, PBC_DIM);
            } else {
                cv_line(px, py + 7, px + 4, py + 1, PBC_INK); cv_line(px + 4, py + 1, px + 7, py + 7, PBC_INK);
                cv_rect(px + 3, py + 4, 2, 3, PBC_MID);
            }
        }
    pb_mix(&a, &b, &bl);
    for (i = 0; i < PB_GENRES; i++) {
        int32_t cx = PB_TOWN[i][1] * 8, cy = 16 + PB_TOWN[i][2] * 8;
        int hot = i == a || (bl && i == b);
        cv_rect(cx, cy + 3, 8, 5, hot && (ui.frame >> 3 & 1u) ? RGB(255, 255, 255) : PBC_GOLD);
        cv_rect(cx + 1, cy + 1, 6, 2, PBC_HOT); cv_rect(cx + 3, cy, 2, 1, PBC_HOT); cv_rect(cx + 3, cy + 5, 2, 3, PBC_BG);
    }
    cv_line(pb.mx, pb.my, PB_TOWN[a][1] * 8 + 4, 16 + PB_TOWN[a][2] * 8 + 4, RGB(255, 255, 255));
    if (bl)
        cv_line(pb.mx, pb.my, PB_TOWN[b][1] * 8 + 4, 16 + PB_TOWN[b][2] * 8 + 4, PBC_HOT);
    pb_sprite(2, pb.mx - 8, pb.my - 10 - (int32_t)(ui.frame >> 3 & 1u), 1, 0);
    for (i = 0; i < 2u; i++) {               /* the two towns in play carry their names */
        uint32_t tw = i ? b : a;
        const char *n = pb_genre_tag(PB_TOWN[tw][0]);
        int32_t w = text_w(&FONT_S, n), lx = clamp(PB_TOWN[tw][1] * 8 + 4 - w / 2, 0, 240 - w), ly = 16 + PB_TOWN[tw][2] * 8 + 9;
        if (i && !bl)
            break;
        cv_rect(lx - 1, ly, w + 2, 14, PBC_BG);
        cv_text(lx, ly - 1, &FONT_S, n, i ? PBC_HOT : PBC_INK);
    }
    cv_rect(0, 176, 240, 64, PBC_BG);
    cv_rect(0, 176, 240, 1, PBC_INK);
    str_cpy(line, pb_genre_name(PB_TOWN[a][0]), 20);
    if (bl) {
        str_cpy(line + str_len(line), " + ", 4);
        str_cpy(line + str_len(line), pb_genre_name(PB_TOWN[b][0]), 14);
    }
    pb_txt(3, 178, line, PBC_INK);
    if (bl) {
        cv_rect(3, 197, 100, 6, PBC_DIM);
        cv_rect(3, 197, (int32_t)(100u - bl * 100u / 14u), 6, PBC_INK);
        cv_rect(3 + (int32_t)(100u - bl * 100u / 14u), 197, (int32_t)(bl * 100u / 14u), 6, PBC_HOT);
    } else
        pb_txt(3, 192, "PURE", PBC_MID);
    pb_txt(124, 192, "OCT+ TRAVEL", (ui.frame >> 4 & 1u) ? RGB(255, 255, 255) : PBC_GOLD);
    pb_txt(3, 208, "K1 EAST K2 SOUTH K3 TOWN", PBC_MID);
    pb_txt(3, 223, "SELECT: BAND / CODE", PBC_MID);
}
UI_SMALL static void pb_draw_band(void)
{
    uint32_t t, sel = song.sel % NTRK;
    int32_t i;
    char line[40];
    static const uint8_t ORDER[NTRK] = {TRK_DRUM, 0, 1, 2};       /* on stage: drums, bass, keys, lead */
    str_cpy(line, pb_genre_name(pb.s.ga), 14);
    if (pb.s.blend) {
        str_cpy(line + str_len(line), "+", 2);
        str_cpy(line + str_len(line), pb_genre_tag(pb.s.gb), 6);
    }
    pb_head(line);
    for (t = 0; t < NTRK; t++) {
        uint32_t k = ORDER[t];
        int32_t x = (int32_t)t * 60, bounce = 0;
        int asleep = trk[k].p[P_MUTE] != 0;
        if (song.playing && !asleep && (k == TRK_DRUM ? clk_pos < BEAT_U / 4u : trk[k].seq_n != 0))
            bounce = 3;
        for (i = 0; i < 15; i++)
            cv_rect(x + i * 4, 62 + (i & 1) * 2, 4, 2, PBC_DIM);
        pb_sprite(k, x + 14, 26 - bounce + (asleep ? 2 : 0), 2, asleep);
        if (asleep)
            pb_txt(x + 46, 18 - (int32_t)((ui.frame >> 4) % 3u) * 2, "z", PBC_MID);
        pb_txtc(x + 30, 66, PB_MEMBER[k], asleep ? PBC_MID : PBC_INK);
        pb_txtc(x + 30, 80, k == TRK_DRUM ? "KIT" : PB_ROLE[pb.s.role[k] % PB_ROLES], PBC_MID);
        if (k == sel) {
            cv_rect(x + 1, 18, 58, 1, RGB(255, 255, 255)); cv_rect(x + 1, 96, 58, 1, RGB(255, 255, 255));
            cv_rect(x + 1, 18, 1, 79, RGB(255, 255, 255)); cv_rect(x + 58, 18, 1, 79, RGB(255, 255, 255));
        }
    }
    cv_rect(0, 100, 240, 1, PBC_MID);
    pb_txt(3, 103, "FROM", PBC_MID);
    pb_txt(43, 103, pb_genre_name(pb.s.src[sel]), PBC_INK);
    pb_txt(3, 119, "SOUND", PBC_MID);
    pb_txt(51, 119, sel == TRK_DRUM ? DRUM_KIT_NAMES[drum_kit()] : ENGINES[trk[sel].eng_req % NENGINES]->presets[trk[sel].preset % ENGINES[trk[sel].eng_req % NENGINES]->npresets].name, PBC_GOLD);
    pb_txt(3, 135, "IDEA", PBC_MID);
    pb_num(43, 135, pb.idea[sel] + 1, PBC_INK);
    if (sel != TRK_DRUM) {
        pb_txt(83, 135, "FIT", PBC_MID);
        cv_rect(115, 140, 60, 6, PBC_DIM);
        cv_rect(115, 140, (int32_t)(pb.fit[sel] > 40u ? (pb.fit[sel] - 40u) : 0u), 6, pb.fit[sel] > 85u ? PBC_MINT : PBC_GOLD);
        pb_num(183, 135, pb.fit[sel], PBC_INK);
    }
    pb_txt(3, 151, "ENERGY", PBC_MID);
    for (t = 0; t < 20u; t++)
        cv_rect(59 + (int32_t)t * 9, 154, 7, 9, t < pb.energy ? PBC_HOT : PBC_DIM);
    line[0] = 0;
    str_cpy(line, PB_NOTE[pb.s.root % 12u], 3);
    str_cpy(line + str_len(line), " ", 2);
    str_cpy(line + str_len(line), PB_SCN[pb.s.scale % PB_SCALES], 5);
    pb_txt(3, 167, "KEY", PBC_MID);
    pb_txt(35, 167, line, PBC_INK);
    pb_txt(123, 167, "CODE", PBC_MID);
    pb_txt(163, 167, pb.entry, PBC_MINT);
    cv_rect(0, 186, 240, 1, PBC_MID);
    pb_txt(3, 189, "K1 IDEA K2 ENERGY K3 ROLE", PBC_MID);
    pb_txt(3, 204, "K4 FROM  PRESETS: SOUND", PBC_MID);
    pb_txt(3, 219, "OCT+ WAKE  OCT- NEW CHORDS", PBC_MID);
}
UI_SMALL static void pb_draw_code(void)
{
    uint32_t i;
    char pw[7], one[2] = {0, 0};
    pb_head("CODE");
    pb_txt(8, 24, "THIS SKETCH", PBC_MID);
    if (pb.have) {
        pb_encode(&pb.s, pw);
        for (i = 0; i < 6u; i++) {
            one[0] = pw[i];
            cv_text(36 + (int32_t)i * 30, 44, &FONT_L, one, RGB(255, 255, 255));
        }
    } else
        pb_txt(8, 48, "NONE YET: TRAVEL FIRST", PBC_INK);
    pb_txt(8, 92, "ENTER A CODE", PBC_MID);
    for (i = 0; i < 6u; i++) {
        one[0] = pb.entry[i];
        cv_rect(30 + (int32_t)i * 30, 112, 26, 36, i == pb.cur ? PBC_GOLD : PBC_DIM);
        cv_text(35 + (int32_t)i * 30, 114, &FONT_L, one, i == pb.cur ? PBC_BG : PBC_INK);
    }
    pb_txt(8, 160, "OCT+ TRAVEL TO THIS CODE", (ui.frame >> 4 & 1u) ? RGB(255, 255, 255) : PBC_GOLD);
    pb_txt(8, 189, "K1 SPOT  K2 LETTER", PBC_MID);
    pb_txt(8, 204, "A CODE REBUILDS THE SKETCH", PBC_MID);
    pb_txt(8, 219, "AS IT WAS MADE, NOT EDITS", PBC_MID);
}
static int pb_shown(void) { return pb.on && !ui.home && !ui.menu && cur_page()->scope == SC_TRK; }
/* the overlay was on screen and is not any more (another page, the menu): it closes; 1 = clear the screen */
static int pb_left(void)
{
    if (!pb.shown_last || pb_shown())
        return 0;
    pb.shown_last = 0;
    pb.on = 0;
    return 1;
}
UI_SMALL static void pb_draw(void)
{
    uint32_t pass;
    if (!pb.tiles_ok)
        pb_map_build();
    if (!pb.shown_last || ui.force) {
        pb.shown_last = 1;
        lcd_fill(0, 0, 240, 240, PBC_BG);
    } else if (ui.frame & 1u) {
        return;
    }
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, 120, PBC_BG);
        cv_oy = -(int32_t)(pass * 120u);
        if (pb.screen == PBS_MAP || !pb.have)
            pb.screen == PBS_CODE ? pb_draw_code() : pb_draw_map();
        else
            pb.screen == PBS_BAND ? pb_draw_band() : pb_draw_code();
        if (ui.msg_t && !pass) {
            cv_rect(0, 0, 240, 16, PBC_GOLD);
            pb_txtc(120, 0, ui.msg, PBC_BG);
        }
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
}

/* ---- input: the seven knobs, OCT- / OCT+, PLAY. HOME (tap) goes on to the visualiser ---- */
UI_SMALL static void pb_input(uint32_t pressed, int home_tap)
{
    int32_t s, k[4];
    uint32_t i, sel, scr, oct_up = (pressed >> panel.btn[B_OCTUP]) & 1u, oct_dn = (pressed >> panel.btn[B_OCTDN]) & 1u;
    if (home_tap) {
        pb.on = 0;
        pb.shown_last = 0;
        ui.force = 1;
        vis_open();
        return;
    }
    if ((pressed >> panel.btn[B_PLAY]) & 1u)
        transport_req = song.playing ? 2 : 1;
    if ((s = panel_enc(EN_SELECT)) != 0)
        pb.screen = (uint8_t)((pb.screen + (s > 0 ? 1u : PBS_N - 1u)) % PBS_N);
    scr = pb.screen;
    if (scr == PBS_BAND && !pb.have)
        scr = PBS_MAP;
    if ((s = panel_enc(EN_ALGO)) != 0 && scr == PBS_BAND) {       /* the member, in stage order */
        static const uint8_t NEXT[NTRK] = {1, 2, 3, 0}, PREV[NTRK] = {3, 0, 1, 2};   /* drums, bass, keys, lead, round */
        track_select(s > 0 ? NEXT[song.sel % NTRK] : PREV[song.sel % NTRK]);
    }
    sel = song.sel % NTRK;
    s = panel_enc(EN_PRESET);
    if (s && scr == PBS_BAND) {              /* the member's sound: SLOOP's whole list, or the kits */
        uint32_t total, cur = preset_pos(&total);
        if (sel == TRK_DRUM)
            TDRUM->p[P_E0] = (int16_t)clamp(TDRUM->p[P_E0] + s, 0, DRUM_KITS - 1);
        else if (total)
            preset_go((uint32_t)(((int32_t)cur + s % (int32_t)total + (int32_t)total) % (int32_t)total));
    }
    for (i = 0; i < 4u; i++)
        k[i] = panel_enc(EN_K1 + i);
    if (scr == PBS_MAP) {
        if (k[0]) pb.mx = (int16_t)clamp(pb.mx + k[0] * 4, 4, 236);
        if (k[1]) pb.my = (int16_t)clamp(pb.my + k[1] * 4, 20, 172);
        if (k[2]) {
            pb.town = (uint8_t)((pb.town + (k[2] > 0 ? 1u : PB_GENRES - 1u)) % PB_GENRES);
            pb.mx = (int16_t)(PB_TOWN[pb.town][1] * 8 + 4);
            pb.my = (int16_t)(16 + PB_TOWN[pb.town][2] * 8 + 4);
        }
        if (oct_up)
            pb_go();
    } else if (scr == PBS_BAND) {
        if (k[0]) {                          /* another idea for this member: right is new, left goes back */
            pb.idea[sel] = (uint8_t)clamp(pb.idea[sel] + (k[0] > 0 ? 1 : -1), 0, 98);
            trk[sel].p[P_MUTE] = 0;
            pb_write(sel);
            pb_fits();
        }
        if (k[1]) {                          /* the whole band's energy: every part is written again from its seed */
            int32_t e = clamp(pb.energy + (k[1] > 0 ? 1 : -1), 1, 20);
            if (e != pb.energy) {
                pb.energy = (uint8_t)e;
                pb_rewrite_all();
            }
        }
        if (k[2] && sel == 1u) {             /* keys: chords, a pad or an arp; lead: a lead line or an arp */
            static const uint8_t R[3] = {PB_CHORD, PB_PAD, PB_ARP};
            uint32_t at = pb.s.role[1] == PB_PAD ? 1u : pb.s.role[1] == PB_ARP ? 2u : 0u;
            pb.s.role[1] = R[(at + (k[2] > 0 ? 1u : 2u)) % 3u];
            pb_sound(1); pb_write(1); pb_fits();
        } else if (k[2] && sel == 2u) {
            pb.s.role[2] = pb.s.role[2] == PB_LEAD ? PB_ARP : PB_LEAD;
            pb_sound(2); pb_write(2); pb_fits();
        } else if (k[2])
            ui_message(sel == TRK_DRUM ? "DRUMS HAVE ONE ROLE" : "BASS HAS ONE ROLE");
        if (k[3]) {                          /* borrow this member from any town */
            pb.s.src[sel] = (uint8_t)((pb.s.src[sel] + (k[3] > 0 ? 1u : PB_GENRES - 1u)) % PB_GENRES);
            pb_sound(sel);
            pb_write(sel);
            if (sel == TRK_DRUM && pb.s.role[0] == PB_BASS)
                pb_write(0);                 /* (a bass that follows the kick follows the new one) */
            pb_fits();
        }
        if (oct_up) {
            trk[sel].p[P_MUTE] = (int16_t)!trk[sel].p[P_MUTE];
            ui_say(PB_MEMBER[sel], trk[sel].p[P_MUTE] ? " SLEEPS" : " WAKES UP");
        }
        if (oct_dn) {
            pb_next_chords(&pb.s);
            for (i = 0; i < NPART; i++)
                pb_write(i);
            pb_fits();
            ui_message("NEW CHORDS");
        }
    } else {
        if (k[0]) pb.cur = (uint8_t)((pb.cur + (k[0] > 0 ? 1u : 5u)) % 6u);
        if (k[1]) {
            uint32_t at = 0;
            while (at < 32u && PB_ALPHA[at] != pb.entry[pb.cur]) at++;
            pb.entry[pb.cur] = PB_ALPHA[(at % 32u + (k[1] > 0 ? 1u : 31u)) % 32u];
        }
        if (oct_up) {
            uint32_t ga, gb, bl, seed;
            if (pb_decode(pb.entry, &ga, &gb, &bl, &seed))
                pb_travel(ga, gb, bl, seed);
            else
                ui_message("NO SUCH PLACE");
        }
    }
}
