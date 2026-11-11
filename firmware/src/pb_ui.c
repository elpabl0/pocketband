/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band: the whole interface of the firmware.
 *
 * One rule everywhere: a button opens its screen; pressed again it does that screen's main action.
 *   HOME  STAGE     the band: a new idea for a member, the band's energy, a member's role, where it comes from
 *   GLO   MAP       walk between the genre towns, travel: a whole new sketch
 *   EDIT  AUDITION  three other players for the member, ranked by how well they fit, heard in the mix
 *   SEQ   GRID      the member's part, step by step
 *   ENV   SOUND     the member's sound on four knobs, three pages
 *   LFO   JAM       the band changes its own parts every few bars; keep or kill each change
 *   FX    PERFORM   the 27 keys are punch-in effects; filter, dust, throws
 *   SEL   KEY       key, scale, swing, the chords
 *   ARP   VISUALS   scope, meters, stars
 *   SAVE  CART      the sketch's code, the four save slots, the FM-1's settings
 * ALGORITHM picks the member, PRESETS its sound, SELECT the tempo, the keys play the member, on every screen
 * (PERFORM's keys are the effects). PLAY and REC as printed. KNOB 1..4 are named along the bottom. */
#include "pb_core.c"
#include "pb_band.c"
static void pb_store_tick(void);            /* pb_store.c */
static void pb_store_boot(void);
static dstep_t pb_keep[NSTEP];               /* the drums as they were, while a build plays its roll */

static const uint8_t PS_BTN[PS_N] = {B_HOME, B_GLO, B_EDIT, B_SEQ, B_ENV, B_LFO, B_FX, B_SCL, B_ARP, B_SAVE};
static const char *const PS_NAME[PS_N] = {"STAGE", "MAP", "AUDITION", "GRID", "SOUND", "JAM", "PERFORM", "KEY", "VISUALS", "CART"};

static struct {
    /* AUDITION */
    uint8_t a_pick, a_salt, a_trk;
    uint8_t a_src[4], a_idea[4], a_role[4], a_fit[4];     /* [0]: the member as it is */
    /* GRID */
    uint8_t g_step, g_len, g_lane;
    int16_t g_row, g_base;
    /* SOUND */
    uint8_t s_page;
    /* JAM */
    uint8_t j_rate, j_wait, j_trk, j_prev;
    uint32_t j_bar;
    /* VISUALS */
    uint8_t v_mode;
    int16_t star[40][3];
    uint16_t meter[NTRK];
    /* PERFORM: the build into a drop */
    uint8_t b_on, b_mute;
    int16_t b_filt;
    uint32_t b_beat0;
    /* VISUALS: the run */
    uint8_t r_on, r_lives, r_flash, r_jv, r_dv;
    uint16_t r_score, r_best, r_combo;
    uint32_t r_jump, r_duck, r_last, r_off;
    /* TAP: REC's second mode; the tour; the party */
    uint8_t t_on, j_tour, party;
    struct { uint8_t on, trk, note; uint32_t t0; } off[6];
    /* CART */
    uint8_t c_page, c_slot, c_act, c_item;
    /* frame */
    uint8_t drawn, thr;
    uint32_t tap_ms[4];
    uint8_t tap_n;
} pu = {.g_len = 2, .j_rate = 0};

/* ------------------------------------------------------------------ helpers --- */
UI_SMALL static void pb_cpy(char *d, const char *s, uint32_t max) { str_cpy(d, s, max); }
UI_SMALL static void pb_cat(char *d, const char *s, uint32_t max)
{
    uint32_t n = str_len(d);
    if (n < max)
        str_cpy(d + n, s, max - n);
}
UI_SMALL static void pb_catn(char *d, int32_t v, uint32_t max)
{
    char b[12];
    fmt_int(b, v);
    pb_cat(d, b, max);
}
UI_SMALL static const char *pb_sound_name(uint32_t t)
{
    const engine_t *e = ENGINES[trk[t % NTRK].eng_req % NENGINES];
    return t == TRK_DRUM ? DRUM_KIT_NAMES[drum_kit()] : e->presets[trk[t].preset % e->npresets].name;
}
/* the member's sound: the next / the one before in the factory list (by kind: basses, keys, pads...), or the kits */
UI_SMALL static void pb_sound_step(uint32_t t, int32_t d)
{
    track_t *T = &trk[t % NTRK];
    uint32_t i, cur = 0;
    if (t == TRK_DRUM) {
        T->p[P_E0] = (int16_t)clamp(T->p[P_E0] + d, 0, DRUM_KITS - 1);
        return;
    }
    if (!bank_ready)
        bank_resolve();
    for (i = 0; i < NBANK; i++)
        if (BANK[i].e == T->eng_req && bank_pi[i] == T->preset)
            cur = i;
    for (i = 0; i < NBANK; i++) {            /* (an entry whose preset this build lacks is stepped over) */
        cur = (uint32_t)(((int32_t)cur + (d > 0 ? 1 : -1) + (int32_t)NBANK) % (int32_t)NBANK);
        if (bank_pi[cur] != 0xFF)
            break;
    }
    if (BANK[cur].e != T->eng_req)
        set_engine_of(T, BANK[cur].e);
    apply_preset_to(T, bank_pi[cur]);
    T->p[P_AMODE] = 0;
    T->p[P_CHORD] = 0;
}
/* a knob on a value with a descriptor: big ranges move faster */
UI_SMALL static void pb_turn(int16_t *v, const param_desc_t *d, int32_t s)
{
    int32_t range = d->max - d->min;
    *v = (int16_t)clamp(*v + s * (d->names || range <= 24 ? 1 : range > 100 ? 3 : 2), d->min, d->max);
}
UI_SMALL static void pb_val(char *out, const int16_t *v, const param_desc_t *d)
{
    const char *unit = "";
    char b[12];
    param_format(d, *v, b, &unit);
    pb_cpy(out, b, 8);
}
UI_SMALL static void gr_focus(void);
UI_SMALL static void pb_select(uint32_t t)
{
    if (t >= NTRK || t == song.sel)
        return;
    song.sel = (uint8_t)t;
    rec_follow(t);
    gr_focus();                              /* (the GRID's window and cursor are the member's) */
    ui.force = 1;
}

/* ------------------------------------------------------------------ frame --- */
UI_SMALL static void pb_head(const char *title)
{
    char b[12];
    int32_t x, i;
    if (ui.msg_t) {
        cv_rect(0, 0, 240, 16, PBC_GOLD);
        pb_txtc(120, 0, ui.msg, PBC_BG);
        return;
    }
    cv_rect(0, 0, 240, 16, PBC_INK);
    pb_txt(3, 0, title, PBC_BG);
    fmt_int(b, song.g[G_BPM]);
    x = 236 - text_w(&FONT_S, b) - 22;
    cv_text(x, 0, &FONT_S, b, PBC_BG);
    if (song.rec || rec_wait)
        cv_rect(218, 4, 8, 8, (rec_wait && (ui.frame >> 3 & 1u)) ? PBC_INK : pu.t_on ? PBC_GOLD : PBC_HOT);
    if (song.playing)
        for (i = 0; i < 5; i++)
            cv_rect(229 + i, 3 + i, 1, 10 - 2 * i, clk_pos < BEAT_U / 4u ? PBC_HOT : PBC_BG);
}
/* the four knobs' names and values along the bottom */
UI_SMALL static void pb_foot(const char *const l[4], char v[4][8])
{
    uint32_t i;
    cv_rect(0, 206, 240, 34, PBC_DIM);
    cv_rect(0, 206, 240, 1, PBC_INK);
    for (i = 0; i < 4u; i++) {
        if (!l[i])
            continue;
        pb_txtc(30 + (int32_t)i * 60, 207, l[i], PBC_INK);
        pb_txtc(30 + (int32_t)i * 60, 223, v[i], PBC_WHITE);
        if (i)
            cv_rect((int32_t)i * 60, 210, 1, 28, PBC_MID);
    }
}
UI_SMALL static void pb_band_row(int32_t y, int32_t sc)        /* the four members in stage order, bouncing */
{
    static const uint8_t ORDER[NTRK] = {TRK_DRUM, 0, 1, 2};
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        uint32_t k = ORDER[t];
        int asleep = trk[k].p[P_MUTE] != 0;
        int32_t x = (int32_t)t * 60 + 30 - 8 * sc, bounce = 0;
        if (song.playing && !asleep && (k == TRK_DRUM ? clk_pos < BEAT_U / 4u : trk[k].seq_n != 0))
            bounce = 3;
        pb_sprite(k, x, y - bounce + (asleep ? 2 : 0), sc, asleep);
        if (asleep)
            pb_txt(x + 16 * sc, y - 8 - (int32_t)((ui.frame >> 4) % 3u) * 2, "z", PBC_MID);
    }
}

/* ------------------------------------------------------------------ STAGE --- */
UI_SMALL static void st_draw(void)
{
    static const uint8_t ORDER[NTRK] = {TRK_DRUM, 0, 1, 2};
    static const char *const L[4] = {"IDEA", "ENERGY", "ROLE", "FROM"};
    uint32_t t, sel = song.sel % NTRK;
    int32_t i;
    char line[32], v[4][8];
    pb_cpy(line, pb.have ? pb_genre_name(pb.s.ga) : "STAGE", 14);
    if (pb.have && pb.s.blend) {
        pb_cat(line, "+", sizeof line);
        pb_cat(line, pb_genre_tag(pb.s.gb), sizeof line);
    }
    pb_head(line);
    pb_band_row(26, 2);
    for (t = 0; t < NTRK; t++) {
        uint32_t k = ORDER[t];
        int32_t x = (int32_t)t * 60;
        int asleep = trk[k].p[P_MUTE] != 0;
        for (i = 0; i < 15; i++)
            cv_rect(x + i * 4, 62 + (i & 1) * 2, 4, 2, PBC_DIM);
        pb_txtc(x + 30, 66, PB_MEMBER[k], asleep ? PBC_MID : PBC_INK);
        pb_txtc(x + 30, 80, k == TRK_DRUM ? "KIT" : PB_ROLE[pb.s.role[k] % PB_ROLES], PBC_MID);
        if (k == sel) {
            cv_rect(x + 1, 18, 58, 1, PBC_WHITE); cv_rect(x + 1, 96, 58, 1, PBC_WHITE);
            cv_rect(x + 1, 18, 1, 79, PBC_WHITE); cv_rect(x + 58, 18, 1, 79, PBC_WHITE);
        }
    }
    cv_rect(0, 100, 240, 1, PBC_MID);
    pb_txt(3, 104, "SOUND", PBC_MID);
    pb_txt(51, 104, pb_sound_name(sel), PBC_GOLD);
    if (!pb.have) {
        pb_txt(3, 126, "NO SKETCH YET.", PBC_INK);
        pb_txt(3, 142, "PRESS GLO FOR THE MAP,", PBC_MID);
        pb_txt(3, 158, "THEN GLO AGAIN TO TRAVEL.", PBC_MID);
    } else {
        pb_txt(3, 122, "FROM", PBC_MID);
        pb_txt(43, 122, pb_genre_name(pb.s.src[sel]), PBC_INK);
        if (sel != TRK_DRUM) {
            pb_txt(3, 140, "FIT", PBC_MID);
            cv_rect(35, 145, 60, 6, PBC_DIM);
            cv_rect(35, 145, (int32_t)(pb.fit[sel] > 40u ? pb.fit[sel] - 40u : 0u), 6, pb.fit[sel] > 85u ? PBC_MINT : PBC_GOLD);
            pb_num(103, 140, pb.fit[sel], PBC_INK);
        }
        pb_txt(139, 140, "CODE", PBC_MID);
        pb_txt(179, 140, pb.entry, PBC_MINT);
        for (t = 0; t < 20u; t++)
            cv_rect(3 + (int32_t)t * 12, 162, 10, 8, t < pb.energy ? PBC_HOT : PBC_DIM);
    }
    pb_txt(3, 180, "HOME: WAKE / SLEEP", PBC_MID);
    fmt_int(v[0], pb.idea[sel] + 1);
    fmt_int(v[1], pb.energy * 5);
    pb_cpy(v[2], sel == TRK_DRUM ? "KIT" : PB_ROLE[pb.s.role[sel] % PB_ROLES], 8);
    pb_cpy(v[3], pb_genre_tag(pb.s.src[sel]), 8);
    pb_foot(L, v);
}
UI_SMALL static int pb_need_sketch(void)
{
    if (pb.have)
        return 0;
    ui_message("NO SKETCH: GLO, THEN GLO");
    return 1;
}
UI_SMALL static void st_knob(uint32_t k, int32_t s)
{
    uint32_t sel = song.sel % NTRK, i;
    if (pb_need_sketch())
        return;
    if (k == 0u) {                           /* another idea: right is new, left goes back */
        pb.idea[sel] = (uint8_t)clamp(pb.idea[sel] + (s > 0 ? 1 : -1), 0, 98);
        trk[sel].p[P_MUTE] = 0;
        pb_write(sel);
        pb_fits();
    } else if (k == 1u) {                    /* the whole band: every part written again from its seed */
        int32_t e = clamp(pb.energy + (s > 0 ? 1 : -1), 1, 20);
        if (e != pb.energy) {
            pb.energy = (uint8_t)e;
            pb_rewrite_all();
        }
    } else if (k == 2u) {
        if (sel == 1u) {
            static const uint8_t R[3] = {PB_CHORD, PB_PAD, PB_ARP};
            uint32_t at = pb.s.role[1] == PB_PAD ? 1u : pb.s.role[1] == PB_ARP ? 2u : 0u;
            pb.s.role[1] = R[(at + (s > 0 ? 1u : 2u)) % 3u];
        } else if (sel == 2u)
            pb.s.role[2] = pb.s.role[2] == PB_LEAD ? PB_ARP : PB_LEAD;
        else {
            ui_message(sel == TRK_DRUM ? "DRUMS HAVE ONE ROLE" : "BASS HAS ONE ROLE");
            return;
        }
        pb_sound(sel); pb_write(sel); pb_fits();
    } else {                                 /* borrow this member from any town */
        pb.s.src[sel] = (uint8_t)((pb.s.src[sel] + (s > 0 ? 1u : PB_GENRES - 1u)) % PB_GENRES);
        pb_sound(sel);
        pb_write(sel);
        if (sel == TRK_DRUM)
            pb_write(0);                     /* (a bass that follows the kick follows the new one) */
        for (i = 0; i < 1u; i++)
            pb_fits();
    }
}
UI_SMALL static void pb_wake_toggle(void)
{
    uint32_t sel = song.sel % NTRK;
    trk[sel].p[P_MUTE] = (int16_t)!trk[sel].p[P_MUTE];
    ui_say(PB_MEMBER[sel], trk[sel].p[P_MUTE] ? " SLEEPS" : " WAKES UP");
}

/* ------------------------------------------------------------------ MAP --- */
UI_SMALL static void mp_draw(void)
{
    static const char *const L[4] = {"EAST", "SOUTH", "TOWN", 0};
    uint32_t x, y, i, a, b, bl, f = ui.frame >> 4;
    char line[40], v[4][8];
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
        int hot = !pb_at_void() && (i == a || (bl && i == b));
        cv_rect(cx, cy + 3, 8, 5, hot && (ui.frame >> 3 & 1u) ? PBC_WHITE : PBC_GOLD);
        cv_rect(cx + 1, cy + 1, 6, 2, PBC_HOT); cv_rect(cx + 3, cy, 2, 1, PBC_HOT); cv_rect(cx + 3, cy + 5, 2, 3, PBC_BG);
    }
    if (!pb_at_void())
        cv_line(pb.mx, pb.my, PB_TOWN[a][1] * 8 + 4, 16 + PB_TOWN[a][2] * 8 + 4, PBC_WHITE);
    if (bl && !pb_at_void())
        cv_line(pb.mx, pb.my, PB_TOWN[b][1] * 8 + 4, 16 + PB_TOWN[b][2] * 8 + 4, PBC_HOT);
    pb_sprite(2, pb.mx - 8, pb.my - 10 - (int32_t)(ui.frame >> 3 & 1u), 1, 0);
    for (i = 0; i < 2u; i++) {               /* the two towns in play carry their names */
        uint32_t tw = i ? b : a;
        const char *n = pb_genre_tag(PB_TOWN[tw][0]);
        int32_t w = text_w(&FONT_S, n), lx = clamp(PB_TOWN[tw][1] * 8 + 4 - w / 2, 0, 240 - w), ly = 16 + PB_TOWN[tw][2] * 8 + 9;
        if ((i && !bl) || pb_at_void())
            break;
        cv_rect(lx - 1, ly, w + 2, 14, PBC_BG);
        cv_text(lx, ly - 1, &FONT_S, n, i ? PBC_HOT : PBC_INK);
    }
    if (pb.mx > 176 && pb.my > 120 && (ui.frame >> 3 & 1u))
        pb_txt(228, 160, "?", pb_at_void() ? PBC_WHITE : PBC_MID);   /* something off the corner */
    cv_rect(0, 176, 240, 30, PBC_BG);
    cv_rect(0, 176, 240, 1, PBC_INK);
    pb_cpy(line, pb_at_void() ? "??? THE VOID" : pb_genre_name(PB_TOWN[a][0]), 20);
    if (pb_at_void())
        bl = 0;
    if (bl) {
        pb_cat(line, " + ", sizeof line);
        pb_cat(line, pb_genre_name(PB_TOWN[b][0]), sizeof line);
    }
    pb_txt(3, 177, line, PBC_INK);
    if (bl) {
        cv_rect(3, 196, 100, 5, PBC_DIM);
        cv_rect(3, 196, (int32_t)(100u - bl * 100u / 14u), 5, PBC_INK);
        cv_rect(3 + (int32_t)(100u - bl * 100u / 14u), 196, (int32_t)(bl * 100u / 14u), 5, PBC_HOT);
    } else
        pb_txt(3, 190, pb_at_void() ? "NO MAP HERE" : "PURE", PBC_MID);
    pb_txt(124, 190, "GLO: TRAVEL", (ui.frame >> 4 & 1u) ? PBC_WHITE : PBC_GOLD);
    fmt_int(v[0], pb.mx);
    fmt_int(v[1], pb.my);
    pb_cpy(v[2], pb_genre_tag(PB_TOWN[pb.town][0]), 8);
    v[3][0] = 0;
    pb_foot(L, v);
}
UI_SMALL static void mp_knob(uint32_t k, int32_t s)
{
    if (k == 0u) pb.mx = (int16_t)clamp(pb.mx + s * 4, 4, 236);
    else if (k == 1u) pb.my = (int16_t)clamp(pb.my + s * 4, 20, 172);
    else if (k == 2u) {
        pb.town = (uint8_t)((pb.town + (s > 0 ? 1u : PB_GENRES - 1u)) % PB_GENRES);
        pb.mx = (int16_t)(PB_TOWN[pb.town][1] * 8 + 4);
        pb.my = (int16_t)(16 + PB_TOWN[pb.town][2] * 8 + 4);
    }
}

/* ------------------------------------------------------------------ AUDITION --- */
UI_SMALL static uint32_t au_fit(uint32_t t, uint32_t src, uint32_t idea, uint32_t role)
{
    uint32_t seed = pb.s.tseed[t] + idea * 0x9E3779B1u, i, n = 0;
    if (t == TRK_DRUM) {
        pb_gen_drums(&pb.s, src, seed, pb_energy(), pb_dtmp);
        for (i = 0; i < PB_STEPS; i++)
            n += pb_dtmp[i].on != 0;
        return 40u + (n > 59u ? 59u : n);
    }
    pb_gen_part(&pb.s, role, src, seed, pb_energy(), pb_tmp);
    return pb_fit(&pb.s, role, pb_tmp, pb_drum, pb_part[(t + 1u) % NPART], pb_part[(t + 2u) % NPART]);
}
UI_SMALL static uint32_t au_role(uint32_t t, uint32_t g)
{
    return t == 0u ? PB_BASS : t == 1u ? PB_G[g % PB_GENRES].keys : PB_G[g % PB_GENRES].lead;
}
UI_SMALL static void au_build(void)           /* three others: the same town, the sketch's other town, a wildcard */
{
    uint32_t t = song.sel % NTRK, i, other, wild;
    pu.a_trk = (uint8_t)t;
    pu.a_pick = 0;
    pu.a_src[0] = pb.s.src[t]; pu.a_idea[0] = pb.idea[t]; pu.a_role[0] = t < NPART ? pb.s.role[t] : 0;
    other = pb.s.ga != pb.s.src[t] ? pb.s.ga : pb.s.gb != pb.s.src[t] ? pb.s.gb : (pb.s.src[t] + 5u + pu.a_salt) % PB_GENRES;
    wild = (pb.s.src[t] + 1u + pb_hash(pb.s.seed + t, pu.a_salt) % (PB_GENRES - 1u)) % PB_GENRES;
    if (wild == other)
        wild = (wild + 3u) % PB_GENRES;
    pu.a_src[1] = pb.s.src[t]; pu.a_src[2] = (uint8_t)other; pu.a_src[3] = (uint8_t)wild;
    for (i = 1; i < 4u; i++) {
        pu.a_idea[i] = (uint8_t)((pb.idea[t] + 1u + pu.a_salt * 3u + i) % 99u);
        pu.a_role[i] = (uint8_t)(i == 1u ? pu.a_role[0] : au_role(t, pu.a_src[i]));
    }
    for (i = 0; i < 4u; i++)
        pu.a_fit[i] = (uint8_t)au_fit(t, pu.a_src[i], pu.a_idea[i], pu.a_role[i]);
}
UI_SMALL static void au_hear(uint32_t pick)   /* that one plays in the mix now */
{
    uint32_t t = pu.a_trk;
    pb.s.src[t] = pu.a_src[pick]; pb.idea[t] = pu.a_idea[pick];
    if (t < NPART)
        pb.s.role[t] = pu.a_role[pick];
    pb_sound(t);
    pb_write(t);
    if (t == TRK_DRUM)
        pb_write(0);
    pb_fits();
    pu.a_pick = (uint8_t)pick;
}
UI_SMALL static void au_leave(void)
{
    if (pb.have && pu.a_pick)
        au_hear(0);                          /* (not hired: the member as it was) */
}
UI_SMALL static void au_draw(void)
{
    static const char *const L[4] = {"PICK", "MORE", 0, 0};
    uint32_t i, t = pu.a_trk;
    char line[32], v[4][8];
    pb_cpy(line, "AUDITION: ", sizeof line);
    pb_cat(line, PB_MEMBER[t], sizeof line);
    pb_head(line);
    for (i = 0; i < 4u; i++) {
        int32_t y = 19 + (int32_t)i * 46;
        int hot = pu.a_pick == i;
        cv_rect(1, y, 238, 44, hot ? PBC_DIM : PBC_BG);
        cv_rect(1, y, 238, 1, hot ? PBC_WHITE : PBC_MID); cv_rect(1, y + 43, 238, 1, hot ? PBC_WHITE : PBC_MID);
        cv_rect(1, y, 1, 44, hot ? PBC_WHITE : PBC_MID); cv_rect(238, y, 1, 44, hot ? PBC_WHITE : PBC_MID);
        pb_sprite(t, 6, y + 6 - (hot ? (int32_t)(ui.frame >> 3 & 1u) : 0), 2, 0);
        pb_cpy(line, i ? pb_genre_name(pu.a_src[i]) : "NOW", sizeof line);
        pb_txt(46, y + 3, line, i ? PBC_INK : PBC_GOLD);
        pb_txt(46, y + 22, t == TRK_DRUM ? "KIT" : PB_ROLE[pu.a_role[i] % PB_ROLES], PBC_MID);
        pb_txt(134, y + 22, t == TRK_DRUM ? "HITS" : "FIT", PBC_MID);
        cv_rect(170, y + 27, 40, 6, PBC_BG);
        cv_rect(170, y + 27, (int32_t)((pu.a_fit[i] > 40u ? pu.a_fit[i] - 40u : 0u) * 2u / 3u), 6, pu.a_fit[i] > 85u ? PBC_MINT : PBC_GOLD);
        pb_num(214, y + 22, t == TRK_DRUM ? pu.a_fit[i] - 40 : pu.a_fit[i], PBC_INK);
        if (hot && i)
            pb_txt(150, y + 3, "EDIT: HIRE", (ui.frame >> 4 & 1u) ? PBC_WHITE : PBC_HOT);
    }
    if (pu.a_pick) fmt_int(v[0], pu.a_pick); else pb_cpy(v[0], "NOW", 8);
    pb_cpy(v[1], "BATCH", 8);
    pb_foot(L, v);
}
UI_SMALL static void au_knob(uint32_t k, int32_t s)
{
    if (pb_need_sketch())
        return;
    if (k == 0u) {
        int32_t p = clamp(pu.a_pick + (s > 0 ? 1 : -1), 0, 3);
        if (p != pu.a_pick)
            au_hear((uint32_t)p);
    } else if (k == 1u) {
        au_leave();
        pu.a_salt++;
        au_build();
        ui_message("THREE MORE");
    }
}

/* ------------------------------------------------------------------ GRID --- */
UI_SMALL static int gr_drum(void) { return song.sel == TRK_DRUM; }
UI_SMALL static void gr_focus(void)          /* the window on the member's notes */
{
    const track_t *T = TSEL;
    uint32_t i;
    pu.g_row = gr_drum() ? 0 : 35;
    if (!gr_drum())
        for (i = 0; i < NSTEP; i++)
            if (T->step[i].time == ST_NOTE && T->step[i].n) {
                pu.g_row = (int16_t)pb_note_deg(&pb.s, T->step[i].note[0]);
                break;
            }
    pu.g_base = (int16_t)(pu.g_row - 5);
    pu.g_lane = 0;
}
UI_SMALL static void gr_put(uint32_t note, int toggle)      /* a note (or a drum sound) on the cursor's step */
{
    track_t *T = TSEL;
    uint32_t s = pu.g_step % NSTEP, k, j, len = (uint32_t)clamp(T->p[P_SLEN], 1, NSTEP);
    undo_mark(T, (undo_sess += 4u) | 3u);
    fm1_irq_off();
    if (gr_drum()) {
        dstep_t *d = &T->dstep[s];
        if (note < DRUM_LANES)
            d->on[note / 8u] ^= (uint8_t)(1u << (note % 8u));
    } else {
        step_t *st = &T->step[s];
        int had = 0;
        if (st->time == ST_NOTE)
            for (k = 0; k < st->n; k++)
                if (st->note[k] == note)
                    had = 1;
        if (had && toggle) {
            for (k = j = 0; k < st->n; k++)
                if (st->note[k] != note)
                    st->note[j++] = st->note[k];
            st->n = (uint8_t)j;
            if (!j) {
                memset(st, 0, sizeof *st);
                st->time = ST_REST;
                for (k = s + 1u; k < len && T->step[k].time == ST_TIE; k++)
                    T->step[k].time = ST_REST;
            }
        } else if (!had) {
            if (st->time == ST_NOTE && st->n && st->n < 4u && toggle)
                st->note[st->n++] = (uint8_t)note;
            else {
                memset(st, 0, sizeof *st);
                st->note[0] = (uint8_t)note;
                st->n = 1;
                st->time = ST_NOTE;
                st->vel = 100;
                for (k = s + 1u; k < len && k < s + pu.g_len && !(T->step[k].time == ST_NOTE && T->step[k].n); k++) {
                    memset(&T->step[k], 0, sizeof(step_t));
                    T->step[k].time = ST_TIE;
                }
            }
        }
    }
    T->seq_active = 1;
    fm1_irq_on();
}
UI_SMALL static void gr_draw(void)
{
    static const char *const LS[4] = {"STEP", "NOTE", "LENGTH", "BAR"}, *const LD[4] = {"STEP", "SOUND", 0, "BAR"};
    const track_t *T = TSEL;
    uint32_t page = pu.g_step >> 4, c, r, i, rows = gr_drum() ? 8u : 11u;
    int32_t rh = gr_drum() ? 20 : 15, y0 = 20;
    char line[32], v[4][8];
    pb_cpy(line, "GRID: ", sizeof line);
    pb_cat(line, PB_MEMBER[song.sel % NTRK], sizeof line);
    pb_head(line);
    for (r = 0; r < rows; r++) {
        int32_t y = y0 + (int32_t)(rows - 1u - r) * rh;
        int cur = gr_drum() ? pu.g_lane + r == (uint32_t)pu.g_row : pu.g_base + (int32_t)r == pu.g_row;
        if (gr_drum())
            pb_cpy(line, LANE_SHORT[(pu.g_lane + r) % DRUM_LANES], 6);
        else {
            uint32_t n = pb_note(&pb.s, pu.g_base + (int32_t)r);
            pb_cpy(line, PB_NOTE[n % 12u], 4);
            pb_catn(line, (int32_t)(n / 12u) - 1, sizeof line);
        }
        pb_txt(1, y + (rh - 16) / 2, line, cur ? PBC_WHITE : PBC_MID);
        for (c = 0; c < 16u; c++)
            cv_rect(42 + (int32_t)c * 12, y, 11, rh - 1, (c >> 2) & 1u ? PBC_BG : PBC_DIM);
    }
    if (gr_drum()) {
        for (c = 0; c < 16u; c++) {
            const dstep_t *d = &T->dstep[page * 16u + c];
            uint32_t on = d->on[0] | (uint32_t)d->on[1] << 8;
            for (r = 0; r < rows; r++)
                if ((on >> (pu.g_lane + r)) & 1u)
                    cv_rect(42 + (int32_t)c * 12, y0 + (int32_t)(rows - 1u - r) * rh, 11, rh - 1, PB_BODY(TRK_DRUM));
        }
    } else {
        uint8_t held[4];
        uint32_t hn = 0;
        for (i = 0; i < page * 16u + 16u && i < NSTEP; i++) {      /* (from the start: a tie carries the notes before it) */
            const step_t *st = &T->step[i];
            int head = st->time == ST_NOTE && st->n;
            if (head) {
                hn = st->n > 4u ? 4u : st->n;
                memcpy(held, st->note, 4);
            } else if (st->time != ST_TIE)
                hn = 0;
            if (i < page * 16u)
                continue;
            for (r = 0; r < hn; r++) {
                int32_t row = pb_note_deg(&pb.s, held[r]) - pu.g_base;
                if (row < 0 || row >= (int32_t)rows)
                    continue;
                cv_rect(42 + (int32_t)(i & 15u) * 12, y0 + ((int32_t)rows - 1 - row) * rh, head ? 11 : 12, rh - 1, PB_BODY(song.sel));
                if (head)
                    cv_rect(42 + (int32_t)(i & 15u) * 12, y0 + ((int32_t)rows - 1 - row) * rh, 2, rh - 1, PBC_WHITE);
            }
        }
    }
    if (song.playing && (T->seq_idx >> 4) == page)
        cv_rect(42 + (int32_t)(T->seq_idx & 15u) * 12, 17, 11, 2, PBC_WHITE);
    {
        int32_t cr = gr_drum() ? pu.g_row - (int32_t)pu.g_lane : pu.g_row - pu.g_base, cx = 41 + (int32_t)(pu.g_step & 15u) * 12;
        int32_t cy = y0 + ((int32_t)rows - 1 - cr) * rh - 1;
        uint16_t cc = (ui.frame >> 3 & 1u) ? PBC_WHITE : PBC_HOT;
        cv_rect(cx, cy, 13, 1, cc); cv_rect(cx, cy + rh, 13, 1, cc); cv_rect(cx, cy, 1, rh, cc); cv_rect(cx + 12, cy, 1, rh + 1, cc);
    }
    pb_txt(3, 188, "SEQ: PUT / REMOVE", PBC_MID);
    fmt_int(v[0], (pu.g_step & 15u) + 1);
    if (gr_drum())
        pb_cpy(v[1], LANE_SHORT[(uint32_t)pu.g_row % DRUM_LANES], 8);
    else {
        uint32_t n = pb_note(&pb.s, pu.g_row);
        pb_cpy(v[1], PB_NOTE[n % 12u], 4);
        pb_catn(v[1], (int32_t)(n / 12u) - 1, 8);
    }
    fmt_int(v[2], pu.g_len);
    fmt_int(v[3], page + 1);
    pb_foot(gr_drum() ? LD : LS, v);
}
UI_SMALL static void gr_knob(uint32_t k, int32_t s)
{
    if (k == 0u)
        pu.g_step = (uint8_t)(((int32_t)pu.g_step + s + NSTEP * 4) % NSTEP);
    else if (k == 1u) {
        if (gr_drum()) {
            pu.g_row = (int16_t)clamp(pu.g_row + s, 0, DRUM_LANES - 1);
            if (pu.g_row < pu.g_lane) pu.g_lane = (uint8_t)pu.g_row;
            if (pu.g_row > pu.g_lane + 7) pu.g_lane = (uint8_t)(pu.g_row - 7);
        } else {
            pu.g_row = (int16_t)clamp(pu.g_row + s, 14, 70);
            if (pu.g_row < pu.g_base) pu.g_base = pu.g_row;
            if (pu.g_row > pu.g_base + 10) pu.g_base = (int16_t)(pu.g_row - 10);
        }
    } else if (k == 2u && !gr_drum())
        pu.g_len = (uint8_t)clamp(pu.g_len + s, 1, 16);
    else if (k == 3u)
        pu.g_step = (uint8_t)(((int32_t)pu.g_step + (s > 0 ? 16 : -16) + NSTEP * 4) % NSTEP);
}

/* ------------------------------------------------------------------ SOUND --- */
typedef struct { const char *l; int16_t *v; const param_desc_t *d; } pb_slot_t;
UI_SMALL static void so_slot(pb_slot_t *o, const char *l, uint32_t global, uint32_t id)
{
    o->l = l;
    if (global) { o->v = &song.g[id]; o->d = &GP[id]; }
    else { o->v = &TSEL->p[id]; o->d = track_desc(TSEL, id); }
}
UI_SMALL static void so_slots(pb_slot_t o[4])
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t i;
    if (gr_drum()) {
        if (pu.s_page == 0u) { so_slot(&o[0], "KIT", 0, P_E0); so_slot(&o[1], "LEVEL", 1, G_DRLVL); so_slot(&o[2], "FILTER", 0, P_TFLT); so_slot(&o[3], "PAN", 0, P_PAN); }
        else if (pu.s_page == 1u) { so_slot(&o[0], "REVERB", 1, G_DRREV); so_slot(&o[1], "ECHO", 1, G_DRDLY); so_slot(&o[2], "DUCK", 1, G_DUCK); so_slot(&o[3], "DUST", 1, G_DUST); }
        else { so_slot(&o[0], "ROOM", 1, G_RSIZE); so_slot(&o[1], "E.TIME", 1, G_DTIME); so_slot(&o[2], "E.FDBK", 1, G_DFDBK); so_slot(&o[3], "E.MIX", 1, G_DMIX); }
        return;
    }
    if (pu.s_page == 0u)
        for (i = 0; i < 4u; i++) {
            so_slot(&o[i], 0, 0, e->macro[i]);
            o[i].l = o[i].d ? o[i].d->label : "";
        }
    else if (pu.s_page == 1u) { so_slot(&o[0], "ATTACK", 0, P_ATK); so_slot(&o[1], "RELEAS", 0, P_REL); so_slot(&o[2], "FILTER", 0, P_TFLT); so_slot(&o[3], "LEVEL", 0, P_LEVEL); }
    else { so_slot(&o[0], "REVERB", 0, P_REV); so_slot(&o[1], "ECHO", 0, P_DLY); so_slot(&o[2], "CHORUS", 0, P_CHOR); so_slot(&o[3], "PAN", 0, P_PAN); }
}
UI_SMALL static void so_draw(void)
{
    static const char *const PG[3] = {"TONE", "SHAPE", "SPACE"};
    pb_slot_t o[4];
    const char *l[4];
    uint32_t i, sel = song.sel % NTRK;
    char line[32], v[4][8];
    pb_cpy(line, "SOUND: ", sizeof line);
    pb_cat(line, PB_MEMBER[sel], sizeof line);
    pb_head(line);
    so_slots(o);
    pb_sprite(sel, 8, 24 - (song.playing && !trk[sel].p[P_MUTE] && (sel == TRK_DRUM ? clk_pos < BEAT_U / 4u : trk[sel].seq_n != 0) ? 3 : 0), 4, 0);
    pb_txt(84, 22, pb_sound_name(sel), PBC_GOLD);
    pb_txt(84, 40, sel == TRK_DRUM ? "DRUM KIT" : ENGINES[trk[sel].eng_req % NENGINES]->name, PBC_MID);
    pb_txt(84, 58, "PRESETS: CHANGE IT", PBC_MID);
    for (i = 0; i < 3u; i++) {
        cv_rect(84 + (int32_t)i * 50, 78, 46, 14, i == pu.s_page ? PBC_INK : PBC_DIM);
        pb_txtc(107 + (int32_t)i * 50, 77, PG[i], i == pu.s_page ? PBC_BG : PBC_MID);
    }
    for (i = 0; i < 4u; i++) {
        int32_t y = 100 + (int32_t)i * 24, range, w;
        l[i] = o[i].l;
        if (!o[i].d) { v[i][0] = 0; continue; }
        pb_val(v[i], o[i].v, o[i].d);
        range = o[i].d->max - o[i].d->min;
        w = range > 0 ? (*o[i].v - o[i].d->min) * 120 / range : 0;
        pb_txt(4, y, o[i].l, PBC_INK);
        cv_rect(64, y + 3, 120, 10, PBC_DIM);
        cv_rect(64, y + 3, w, 10, i & 1u ? PBC_GOLD : PBC_HOT);
        pb_txt(190, y, v[i], PBC_WHITE);
    }
    pb_txt(4, 190, "ENV: NEXT PAGE", PBC_MID);
    pb_foot(l, v);
}
UI_SMALL static void so_knob(uint32_t k, int32_t s)
{
    pb_slot_t o[4];
    so_slots(o);
    if (o[k].d)
        pb_turn(o[k].v, o[k].d, s);
}

/* ------------------------------------------------------------------ JAM --- */
UI_SMALL static void jm_change(void)         /* one awake member gets a new idea */
{
    uint32_t i, t = pb_hash(fm1_ms, pu.j_bar) % NTRK;
    if (!pb.have)
        return;
    for (i = 0; i < NTRK && trk[t].p[P_MUTE]; i++)
        t = (t + 1u) % NTRK;
    if (trk[t].p[P_MUTE])
        return;
    pu.j_trk = (uint8_t)t;
    pu.j_prev = pb.idea[t];
    pb.idea[t] = (uint8_t)((pb.idea[t] + 1u) % 99u);
    pb_write(t);
    pb_fits();
    pu.j_wait = 1;
}
UI_SMALL static void jm_tick(void)           /* every frame, on every screen: the jam goes on */
{
    uint32_t bar = clk_beat / 4u;
    if (!song.playing || bar == pu.j_bar)
        return;
    pu.j_bar = bar;
    if (pu.party) {                          /* (the code PARTY2: a new skin every bar) */
        pb.skin = (uint8_t)((pb.skin + 1u) % PB_SKINS);
        pb_skin_apply();
    }
    if (pu.j_tour && bar && bar % pu.j_tour == 0u && !pu.b_on && pu.r_on != 1u) {
        uint32_t h = pb_hash(fm1_ms, bar);   /* a walk of a few steps from where the band stands, then a new sketch there */
        pb.mx = (int16_t)clamp(pb.mx + (int32_t)(h % 97u) - 48, 4, 220);
        pb.my = (int16_t)clamp(pb.my + (int32_t)((h >> 8) % 73u) - 36, 20, 156);
        pb_stay = 1;
        pb_go();
        pb_stay = 0;
        pu.j_wait = 0;
        return;
    }
    if (pu.j_rate && bar && bar % pu.j_rate == 0u)
        jm_change();
}
UI_SMALL static void jm_judge(int keep)
{
    if (!pu.j_wait) {
        ui_message("NOTHING TO JUDGE");
        return;
    }
    if (!keep) {
        pb.idea[pu.j_trk] = pu.j_prev;
        pb_write(pu.j_trk);
        pb_fits();
    }
    pu.j_wait = 0;
    ui_say(PB_MEMBER[pu.j_trk], keep ? ": KEPT" : ": KILLED");
}
UI_SMALL static void jm_draw(void)
{
    static const char *const L[4] = {"EVERY", "ENERGY", "MORPH", "TOUR"};
    char v[4][8], line[32];
    uint32_t t;
    pb_head("JAM");
    pb_band_row(30, 2);
    pb_txtc(120, 76, pu.j_rate ? "THE BAND IS JAMMING" : "THE BAND WAITS", pu.j_rate ? PBC_MINT : PBC_MID);
    if (pu.j_rate) {
        pb_cpy(line, "A NEW PART EVERY ", sizeof line);
        pb_catn(line, pu.j_rate, sizeof line);
        pb_cat(line, " BARS", sizeof line);
        pb_txtc(120, 94, line, PBC_INK);
    } else
        pb_txtc(120, 94, "TURN KNOB 1 TO START", PBC_INK);
    cv_rect(0, 116, 240, 1, PBC_MID);
    if (pu.j_wait) {
        pb_cpy(line, "NEW: ", sizeof line);
        pb_cat(line, PB_MEMBER[pu.j_trk], sizeof line);
        pb_cat(line, " IDEA ", sizeof line);
        pb_catn(line, pb.idea[pu.j_trk] + 1, sizeof line);
        pb_txtc(120, 122, line, PBC_WHITE);
        pb_txtc(120, 142, "OCT- KILL IT   OCT+ KEEP IT", (ui.frame >> 4 & 1u) ? PBC_HOT : PBC_GOLD);
    } else
        pb_txtc(120, 130, "NO CHANGE WAITING", PBC_MID);
    for (t = 0; t < 20u; t++)
        cv_rect(3 + (int32_t)t * 12, 166, 10, 8, t < pb.energy ? PBC_HOT : PBC_DIM);
    if (pu.j_tour) {
        pb_cpy(line, "ON TOUR: A NEW PLACE IN ", sizeof line);
        pb_catn(line, (int32_t)(pu.j_tour - (clk_beat / 4u) % pu.j_tour), sizeof line);
        pb_txt(3, 184, line, PBC_GOLD);
    } else
        pb_txt(3, 184, "LFO: CHANGE A PART NOW", PBC_MID);
    if (pu.j_rate) fmt_int(v[0], pu.j_rate); else pb_cpy(v[0], "OFF", 8);
    fmt_int(v[1], pb.energy * 5);
    fmt_int(v[2], pb.morph * 10);
    if (pu.j_tour) fmt_int(v[3], pu.j_tour); else pb_cpy(v[3], "OFF", 8);
    pb_foot(L, v);
}
UI_SMALL static void jm_knob(uint32_t k, int32_t s)
{
    static const uint8_t R[4] = {0, 8, 4, 2};
    if (k == 0u) {
        uint32_t at = pu.j_rate == 8u ? 1u : pu.j_rate == 4u ? 2u : pu.j_rate == 2u ? 3u : 0u;
        pu.j_rate = R[clamp((int32_t)at + (s > 0 ? 1 : -1), 0, 3)];
    } else if (k == 1u)
        st_knob(1, s);
    else if (k == 2u) {                      /* every part leans towards its next idea */
        int32_t m = clamp(pb.morph + (s > 0 ? 1 : -1), 0, 10);
        if (m != pb.morph && !pb_need_sketch()) {
            pb.morph = (uint8_t)m;
            pb_rewrite_all();
        }
    } else {                                 /* the tour: every so many bars the band moves on along the map */
        static const uint8_t TR[3] = {0, 32, 16};
        uint32_t at = pu.j_tour == 32u ? 1u : pu.j_tour == 16u ? 2u : 0u;
        pu.j_tour = TR[clamp((int32_t)at + (s > 0 ? 1 : -1), 0, 2)];
    }
}

/* ------------------------------------------------------------------ PERFORM --- */
static const char *const PF_SHORT[PUNCH_NFX] = {
    "L4", "L8", "L16", "L32", "STU", "REV", "STP", "HLF", "LOW", "HI", "TEL", "CRU", "ALI", "GAT", "ECH", "WOB",
    "K.LO", "K.MI", "K.HI", "RWND", "OCT+", "FLNG", "PAN", "FRZ", "L8T", "MONO", "FADE"};
UI_SMALL static void pf_enter(void) { ly_lock = LY_FX; }
/* the build: from the next bar, two bars of a snare roll with the bass out and the filter rising; then the drop:
 * the drums back, the filter open, everyone awake */
UI_SMALL static void bd_restore(void)
{
    fm1_irq_off();
    memcpy(TDRUM->dstep, pb_keep, sizeof pb_keep);
    fm1_irq_on();
    song.g[G_FILT] = pu.b_filt;
    trk[0].p[P_MUTE] = pu.b_mute;
    pu.b_on = 0;
}
UI_SMALL static void bd_arm(void)
{
    if (pu.b_on) { ui_message("THE BUILD IS ON ITS WAY"); return; }
    if (!song.playing) { ui_message("PRESS PLAY FIRST"); return; }
    pu.b_on = 1;
    pu.b_beat0 = clk_beat;
    ui_message("BUILD: FROM THE NEXT BAR");
}
UI_SMALL static void bd_tick(void)           /* every frame, on every screen */
{
    uint32_t i, s0, t;
    if (!pu.b_on)
        return;
    if (!song.playing) {
        if (pu.b_on == 2u) bd_restore();
        pu.b_on = 0;
        return;
    }
    if (pu.b_on == 1u) {
        if (clk_beat / 4u == pu.b_beat0 / 4u)
            return;                          /* (the bar it was asked in still plays) */
        pu.b_beat0 = clk_beat & ~3u;
        s0 = (TDRUM->seq_idx + 1u) & 48u;     /* the bar of the pattern that just began */
        pu.b_filt = song.g[G_FILT];
        pu.b_mute = (uint8_t)trk[0].p[P_MUTE];
        fm1_irq_off();
        memcpy(pb_keep, TDRUM->dstep, sizeof pb_keep);
        for (i = 0; i < 32u; i++) {
            dstep_t *d = &TDRUM->dstep[(s0 + i) & 63u];
            memset(d, 0, sizeof *d);
            if (i < 16u) {
                if (!(i & 3u)) d->on[0] |= 1u;        /* kick on the beats, snare on the eighths */
                if (!(i & 1u)) d->on[0] |= 4u;
            } else {
                d->on[0] |= 4u;                       /* then every sixteenth, toms into the drop */
                if (i >= 28u) d->on[1] |= 2u;
            }
        }
        fm1_irq_on();
        trk[0].p[P_MUTE] = 1;
        pu.b_on = 2;
        return;
    }
    i = clk_beat - pu.b_beat0;
    if (i >= 8u) {
        bd_restore();
        for (t = 0; t < NTRK; t++)
            trk[t].p[P_MUTE] = 0;
        ui_message("DROP!");
    } else
        song.g[G_FILT] = (int16_t)clamp(pu.b_filt + (int32_t)i * 6, -64, 63);
}
UI_SMALL static void pf_leave(void)
{
    fm1_irq_off();
    ly_lock = LY_PLAY;
    punch.req = -1;
    punch.keybit = 0;
    fm1_irq_on();
    thr_req = 0;
    pu.thr = 0;
}
UI_SMALL static void pf_draw(void)
{
    static const char *const L[4] = {"FILTER", "DUST", "DUCK", "FADE"};
    char v[4][8];
    uint32_t i;
    int32_t cur = punch.req;
    pb_head("PERFORM");
    pb_txt(3, 18, pu.b_on == 2u ? "BUILDING: THE DROP IS COMING" : pu.b_on ? "THE BUILD STARTS NEXT BAR" : "HOLD A KEY. FX: BUILD + DROP", pu.b_on ? PBC_GOLD : PBC_MID);
    for (i = 0; i < 16u; i++) {              /* the white keys, two rows of eight */
        int32_t x = 1 + (int32_t)(i % 8u) * 30, y = 38 + (int32_t)(i / 8u) * 24;
        int hot = cur == (int32_t)i;
        cv_rect(x, y, 29, 22, hot ? PBC_HOT : PBC_DIM);
        pb_txtc(x + 15, y + 3, PF_SHORT[i], hot ? PBC_BG : PBC_INK);
    }
    pb_txt(3, 88, "BLACK KEYS", PBC_MID);
    for (i = 16; i < PUNCH_NFX; i++) {
        int32_t k = (int32_t)i - 16, x = 1 + (k % 6) * 40, y = 106 + (k / 6) * 24;
        int hot = cur == (int32_t)i;
        cv_rect(x, y, 39, 22, hot ? PBC_GOLD : PBC_MID);
        pb_txtc(x + 20, y + 3, PF_SHORT[i], hot ? PBC_BG : PBC_WHITE);
    }
    pb_band_row(160, 1);
    pb_txt(3, 182, "OCT- REVERB", (pu.thr & 1u) ? PBC_WHITE : PBC_MID);
    pb_txt(131, 182, "OCT+ ECHO", (pu.thr & 2u) ? PBC_WHITE : PBC_MID);
    pb_val(v[0], &song.g[G_FILT], &GP[G_FILT]);
    pb_val(v[1], &song.g[G_DUST], &GP[G_DUST]);
    pb_val(v[2], &song.g[G_DUCK], &GP[G_DUCK]);
    fmt_int(v[3], song.g[G_FXFADE]);
    pb_foot(L, v);
}
UI_SMALL static void pf_knob(uint32_t k, int32_t s)
{
    static const uint8_t ID[4] = {G_FILT, G_DUST, G_DUCK, G_FXFADE};
    pb_turn(&song.g[ID[k]], &GP[ID[k]], s);
}

/* ------------------------------------------------------------------ KEY --- */
UI_SMALL static void ky_chord(char *out, uint32_t c)
{
    const uint8_t *sc = PB_SCALE[pb.s.scale % PB_SCALES];
    uint32_t r = sc[c % 7u], third = (sc[(c + 2u) % 7u] + 12u - r) % 12u, fifth = (sc[(c + 4u) % 7u] + 12u - r) % 12u;
    pb_cpy(out, PB_NOTE[(pb.s.root + r) % 12u], 4);
    pb_cat(out, fifth == 6u ? "DIM" : third == 3u ? "M" : "", 8);
}
UI_SMALL static void ky_apply(void)          /* the key changed: the tracks' key, and the parts written in it */
{
    uint32_t t;
    pb_key();
    if (!pb.have)
        return;
    for (t = 0; t < NPART; t++)
        pb_write(t);
    pb_fits();
}
UI_SMALL static void ky_draw(void)
{
    static const char *const L[4] = {"ROOT", "SCALE", "SWING", "CHORDS"};
    static const uint8_t WH[7] = {0, 2, 4, 5, 7, 9, 11}, BL[5][2] = {{1, 0}, {3, 1}, {6, 3}, {8, 4}, {10, 5}};
    const uint8_t *sc = PB_SCALE[pb.s.scale % PB_SCALES];
    uint32_t i, k, bar = song.playing ? (clk_beat / 4u) % 4u : 9u;
    char v[4][8], line[32];
    uint16_t in[12];
    pb_head("KEY");
    for (i = 0; i < 12u; i++) {
        in[i] = 0;
        for (k = 0; k < 7u; k++)
            if ((pb.s.root + sc[k]) % 12u == i)
                in[i] = 1;
    }
    pb_cpy(line, PB_NOTE[pb.s.root % 12u], 4);
    pb_cat(line, " ", sizeof line);
    pb_cat(line, PB_SCN[pb.s.scale % PB_SCALES], sizeof line);
    cv_text(8, 18, &FONT_L, line, PBC_WHITE);
    for (i = 0; i < 7u; i++) {
        int32_t x = 15 + (int32_t)i * 30;
        cv_rect(x, 54, 29, 52, in[WH[i]] ? PBC_INK : PBC_MID);
        if (in[WH[i]])
            cv_rect(x + 10, 92, 9, 9, WH[i] == pb.s.root ? PBC_HOT : PBC_MINT);
    }
    for (i = 0; i < 5u; i++) {
        int32_t x = 15 + BL[i][1] * 30 + 20;
        cv_rect(x, 54, 19, 30, PBC_BG);
        cv_rect(x + 1, 54, 17, 29, PBC_DIM);
        if (in[BL[i][0]])
            cv_rect(x + 5, 70, 9, 9, BL[i][0] == pb.s.root ? PBC_HOT : PBC_MINT);
    }
    pb_txt(8, 110, "THE KEYS SNAP TO LIT NOTES", PBC_MID);
    for (i = 0; i < 4u; i++) {
        int32_t x = 4 + (int32_t)i * 59;
        int hot = bar == i;
        cv_rect(x, 130, 55, 40, hot ? PBC_GOLD : PBC_DIM);
        ky_chord(line, pb.s.prog[i]);
        pb_txtc(x + 28, 142, line, hot ? PBC_BG : PBC_INK);
    }
    pb_txt(8, 180, "SEL: THE NEXT CHORD SET", PBC_MID);
    pb_cpy(v[0], PB_NOTE[pb.s.root % 12u], 8);
    pb_cpy(v[1], PB_SCN[pb.s.scale % PB_SCALES], 8);
    fmt_int(v[2], song.g[G_SWING]);
    fmt_int(v[3], pb.s.prog_set + 1);
    pb_foot(L, v);
}
UI_SMALL static void ky_next_chords(void)
{
    uint32_t t;
    if (pb_need_sketch())
        return;
    pb_next_chords(&pb.s);
    for (t = 0; t < NPART; t++)
        pb_write(t);
    pb_fits();
    ui_message("NEW CHORDS");
}
UI_SMALL static void ky_knob(uint32_t k, int32_t s)
{
    if (k == 0u) { pb.s.root = (uint8_t)((pb.s.root + (s > 0 ? 1u : 11u)) % 12u); ky_apply(); }
    else if (k == 1u) { pb.s.scale = (uint8_t)((pb.s.scale + (s > 0 ? 1u : PB_SCALES - 1u)) % PB_SCALES); ky_apply(); }
    else if (k == 2u) song.g[G_SWING] = (int16_t)clamp(song.g[G_SWING] + s * 2, 0, 100);
    else ky_next_chords();
}

/* ------------------------------------------------------------------ VISUALS --- */
/* ---- the run: a game hidden in the VISUALS. The member runs along the drum part that plays: a kick is a block to
 * jump, a snare a beam to duck. The high keys (or OCT+) jump, the low keys (or OCT-) duck. A miss puts a member of
 * the band to sleep; twelve clean in a row wake one. The keys play no notes meanwhile (the MIX layer, locked). */
UI_SMALL static uint32_t rn_pos(void) { return clk_beat * 1024u + clk_pos / (BEAT_U / 1024u); }   /* 1/256 of a step */
UI_SMALL static uint32_t rn_kind(uint32_t step)                 /* 0 nothing, 1 jump, 2 duck */
{
    const dstep_t *d = &TDRUM->dstep[(step + pu.r_off) & 63u];
    if (step & 1u)
        return 0;                            /* (the eighths only: room to land) */
    return (d->on[0] & 1u) ? 1u : (d->on[0] & 0x0Cu) ? 2u : 0u;
}
UI_SMALL static void rn_start(void)
{
    uint32_t t;
    if (pb_need_sketch())
        return;
    for (t = 0; t < NTRK; t++)
        trk[t].p[P_MUTE] = 0;
    pu.r_lives = 3; pu.r_score = 0; pu.r_combo = 0; pu.r_jv = pu.r_dv = 0; pu.r_flash = 0;
    pu.r_last = rn_pos();
    pu.r_on = 1;
    if (!song.playing)
        transport_req = 1;
}
UI_SMALL static void rn_press(int jump)
{
    if (pu.r_on != 1u) {
        rn_start();
        return;
    }
    if (jump) { pu.r_jump = rn_pos(); pu.r_jv = 1; }
    else { pu.r_duck = rn_pos(); pu.r_dv = 1; }
}
UI_SMALL static void rn_tick(void)
{
    static const uint8_t SLEEP[3] = {2, 1, 0};
    uint32_t p, s, guard = 0, t;
    if (pu.r_flash)
        pu.r_flash--;
    if (pu.r_on != 1u)
        return;
    if (!song.playing && !transport_req) {
        pu.r_on = 0;
        return;
    }
    p = rn_pos();
    if (p < pu.r_last || p - pu.r_last > 16u * 256u)
        pu.r_last = p;                       /* (the transport started again, or a long frame: from here) */
    pu.r_off = (TDRUM->seq_idx - (p >> 8)) & 63u;
    for (s = ((pu.r_last - 48u) >> 8) + 1u; s * 256u + 48u <= p && guard < 8u; s++, guard++) {
        uint32_t at = s * 256u + 48u, k;     /* a step is judged a moment after it passed: a late press still counts */
        if (at <= pu.r_last || (k = rn_kind(s)) == 0u)
            continue;
        if (k == 1u ? (pu.r_jv && at - pu.r_jump < 470u) : (pu.r_dv && at - pu.r_duck < 470u)) {
            pu.r_score = (uint16_t)(pu.r_score + 10u + (pu.r_combo > 20u ? 20u : pu.r_combo));
            pu.r_combo++;
            if (pu.r_combo % 12u == 0u)
                for (t = 0; t < 3u; t++)
                    if (trk[SLEEP[2u - t]].p[P_MUTE]) { trk[SLEEP[2u - t]].p[P_MUTE] = 0; if (pu.r_lives < 3u) pu.r_lives++; break; }
        } else {
            pu.r_combo = 0;
            pu.r_flash = 6;
            for (t = 0; t < 3u; t++)
                if (!trk[SLEEP[t]].p[P_MUTE]) { trk[SLEEP[t]].p[P_MUTE] = 1; break; }
            if (pu.r_lives)
                pu.r_lives--;
            if (!pu.r_lives) {
                pu.r_on = 2;
                if (pu.r_score > pu.r_best)
                    pu.r_best = pu.r_score;
                break;
            }
        }
    }
    pu.r_last = p;
}
UI_SMALL static void rn_draw(void)
{
    uint32_t p = rn_pos(), sel = song.sel % NTRK, i, s;
    int32_t h = 0, duck;
    char line[32];
    pb_head("RUN!");
    for (i = 0; i < 3u; i++)
        cv_rect(6 + (int32_t)i * 16, 22, 12, 10, i < pu.r_lives ? PBC_HOT : PBC_DIM);
    pb_txt(70, 19, "SCORE", PBC_MID); pb_num(118, 19, pu.r_score, PBC_WHITE);
    pb_txt(170, 19, "BEST", PBC_MID); pb_num(206, 19, pu.r_best, PBC_GOLD);
    pb_band_row(50, 2);
    if (pu.r_combo > 1u) {
        pb_cpy(line, "COMBO ", sizeof line);
        pb_catn(line, pu.r_combo, sizeof line);
        pb_txtc(120, 86, line, PBC_MINT);
    }
    pb_txtc(120, 102, pu.r_on == 1u ? "LOW KEYS DUCK, HIGH KEYS JUMP" : pu.r_on == 2u ? "GAME OVER. ANY KEY: AGAIN" : "ANY KEY, OR ARP: START", pu.r_on == 1u ? PBC_MID : PBC_WHITE);
    /* the field: the lower half of the screen */
    if (pu.r_flash)
        cv_rect(0, 120, 240, 120, PBC_DIM);
    cv_rect(0, 228, 240, 12, PBC_MID);
    for (i = 0; i < 11u; i++)                /* the ground moves with the beat */
        cv_rect((int32_t)i * 26 - (int32_t)((p % 256u) * 26u / 256u) + 4, 232, 12, 2, PBC_DIM);
    if (pu.r_on == 1u || song.playing)
        for (s = (p >> 8) - (p >= 512u ? 2u : 0u); s < (p >> 8) + 9u; s++) {
            int32_t x = 56 + (int32_t)(s * 256u - p) * 26 / 256;
            uint32_t k = pu.r_on == 1u ? rn_kind(s) : 0u;
            if (k == 1u) {
                cv_rect(x - 5, 214, 10, 14, PBC_GOLD);
                cv_rect(x - 3, 216, 6, 3, PBC_WHITE);
            } else if (k == 2u) {
                cv_rect(x - 4, 122, 8, 84, PBC_HOT);
                cv_rect(x - 6, 200, 12, 6, PBC_WHITE);
            }
        }
    duck = pu.r_on == 1u && pu.r_dv && p - pu.r_duck < 470u;
    if (pu.r_on == 1u && pu.r_jv && p - pu.r_jump < 470u) {
        int32_t t = (int32_t)(p - pu.r_jump);
        h = 46 * 4 * t * (470 - t) / (470 * 470);
    }
    if (duck)
        pb_sprite(sel, 48, 212, 1, 0);
    else
        pb_sprite(sel, 40, 196 - h, 2, pu.r_on == 2u);
}
UI_SMALL static void vs_draw(void)
{
    static const char *const L[4] = {"MODE", 0, 0, 0}, *const M[4] = {"SCOPE", "METERS", "STARS", "RUN"};
    char v[4][8];
    uint32_t i, w = scope_w;
    int kick = song.playing && clk_pos < BEAT_U / 6u;
    if (pu.v_mode == 3u) {
        rn_draw();
        return;
    }
    pb_head("VISUALS");
    if (pu.v_mode == 0u) {
        int32_t py = 110;
        for (i = 0; i < 9u; i++) cv_rect(0, 30 + (int32_t)i * 20, 240, 1, PBC_DIM);
        for (i = 0; i < 240u; i++) {
            int32_t y = clamp(110 - scope_buf[(w + i * 2u) & (SCOPE_N - 1u)] / 380, 18, 203);
            cv_line((int32_t)i - 1, py, (int32_t)i, y, PBC_MINT);
            py = y;
        }
    } else if (pu.v_mode == 1u) {
        for (i = 0; i < NTRK; i++) {
            static const uint8_t ORDER[NTRK] = {TRK_DRUM, 0, 1, 2};
            uint32_t k = ORDER[i], h = pb_isqrt(pu.meter[k]) * 130u / 181u;
            int32_t x = (int32_t)i * 60 + 12, j;
            if (h > 130u) h = 130u;
            for (j = 0; j < (int32_t)h; j += 6)
                cv_rect(x, 150 - j, 36, 4, j > 104 ? PBC_HOT : j > 60 ? PBC_GOLD : PBC_MINT);
            pb_sprite(k, x + 2, 158, 2, trk[k].p[P_MUTE] != 0);
        }
    } else {
        for (i = 0; i < 40u; i++) {
            int16_t *s = pu.star[i];
            int32_t x, y;
            s[2] = (int16_t)(s[2] - (kick ? 14 : 4));
            if (s[2] <= 8) {
                uint32_t h = pb_hash(fm1_ms + i, ui.frame);
                s[0] = (int16_t)((int32_t)(h & 255u) - 128); s[1] = (int16_t)((int32_t)(pb_hash(h, i) & 255u) - 128); s[2] = 256;
            }
            x = 120 + s[0] * 120 / s[2]; y = 110 + s[1] * 100 / s[2];
            if (y > 18 && y < 200)
                cv_rect(x, y, s[2] < 80 ? 3 : s[2] < 160 ? 2 : 1, s[2] < 80 ? 3 : s[2] < 160 ? 2 : 1, s[2] < 80 ? PBC_WHITE : s[2] < 160 ? PBC_INK : PBC_MID);
        }
        pb_band_row(176, 1);
    }
    pb_cpy(v[0], M[pu.v_mode % 4u], 8);
    pb_foot(L, v);
}
UI_SMALL static void vs_tick(void)           /* the meters: the tracks' peaks, read once a frame, falling */
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        int32_t pk;
        fm1_irq_off();
        if (t == TRK_DRUM) { pk = drums.peak; drums.peak = 0; }
        else { pk = trk[t].peak; trk[t].peak = 0; }
        fm1_irq_on();
        if (pk > 32767) pk = 32767;
        pu.meter[t] = (uint16_t)(pk > pu.meter[t] ? pk : pu.meter[t] - pu.meter[t] / 6u);
    }
}

/* ------------------------------------------------------------------ CART --- */
enum { CI_LIGHTS, CI_KEYS, CI_CLICK, CI_SYNC, CI_MIDI, CI_RATE, CI_LOWCUT, CI_CORE, CI_SKIN, CI_CAL, CI_NEW, CI_ABOUT, CI_N };
static const char *const CI_NAME[CI_N] = {"PANEL LIGHTS", "KEY LIGHTS", "CLICK", "CLOCK FROM", "MIDI OUT", "USB AUDIO", "SPEAKER LOW CUT",
                                          "SECOND CORE", "SKIN", "CALIBRATE", "NEW SKETCHBOOK", "ABOUT"};
UI_SMALL static void ct_value(uint32_t it, char *o)
{
    static const char *const LV[4] = {"OFF", "LOW", "MID", "HIGH"}, *const KY[4] = {"OFF", "C", "WHITE", "ALL"};
    static const char *const CK[3] = {"OFF", "REC", "ON"}, *const SY[3] = {"OWN", "USB", "JACK"}, *const MO[2] = {"KEYS", "PARTS"};
    static const char *const RT[3] = {"AUTO", "44.1", "48"};
    switch (it) {
    case CI_LIGHTS: pb_cpy(o, LV[lights_lvl & 3u], 8); break;
    case CI_KEYS: pb_cpy(o, KY[lights_keys & 3u], 8); break;
    case CI_CLICK: pb_cpy(o, CK[(uint32_t)song.g[G_CLOCK] % 3u], 8); break;
    case CI_SYNC: pb_cpy(o, SY[(uint32_t)song.g[G_SYNC] % 3u], 8); break;
    case CI_MIDI: pb_cpy(o, MO[(uint32_t)song.g[G_MIDI] & 1u], 8); break;
    case CI_RATE: pb_cpy(o, RT[usb_rate % 3u], 8); break;
    case CI_LOWCUT: pb_cpy(o, settings.lowcut ? "ON" : "OFF", 8); break;
    case CI_CORE: pb_cpy(o, cpu1_off ? "OFF" : "ON", 8); break;
    case CI_SKIN: pb_cpy(o, PB_SKIN_NAME[pb.skin % PB_SKINS], 9); break;
    default: pb_cpy(o, "SAVE", 8); break;
    }
}
UI_SMALL static void ct_set(uint32_t it, int32_t s)
{
    switch (it) {
    case CI_LIGHTS: lights_lvl = (uint8_t)clamp(lights_lvl + s, 0, 3); break;
    case CI_KEYS: lights_keys = (uint8_t)clamp(lights_keys + s, 0, 3); if (lights_keys && !lights_lvl) lights_lvl = 1; break;
    case CI_CLICK: song.g[G_CLOCK] = (int16_t)clamp(song.g[G_CLOCK] + s, 0, 2); break;
    case CI_SYNC: song.g[G_SYNC] = (int16_t)clamp(song.g[G_SYNC] + s, 0, 2); break;
    case CI_MIDI: song.g[G_MIDI] = (int16_t)clamp(song.g[G_MIDI] + s, 0, 1); break;
    case CI_RATE: usb_rate = (uint8_t)clamp(usb_rate + s, 0, 2); ui_message("AT THE NEXT POWER-ON"); break;
    case CI_LOWCUT: settings.lowcut = s > 0; fx_lowcut = (uint8_t)(settings.lowcut != 0); break;
    case CI_CORE: cpu1_off = s < 0; cpu1_units = cpu1_off ? 0 : -1; break;
    case CI_SKIN: pb.skin = (uint8_t)(((int32_t)pb.skin + s + (int32_t)PB_SKINS) % (int32_t)PB_SKINS); pb_skin_apply(); return;
    default: return;
    }
    settings_later = 1;
}
UI_SMALL static void ct_draw(void)
{
    static const char *const PG[3] = {"CODE", "SLOTS", "SETUP"};
    static const char *const LC[4] = {"PAGE", "SPOT", "LETTER", 0}, *const LS[4] = {"PAGE", "SLOT", "DO", 0}, *const LU[4] = {"PAGE", "ITEM", "SET", 0};
    char v[4][8], one[2] = {0, 0}, pw[7], line[36];
    uint32_t i, ready = arrangement_ready();
    pb_head("CARTRIDGE");
    for (i = 0; i < 3u; i++) {
        cv_rect(2 + (int32_t)i * 79, 18, 77, 16, i == pu.c_page ? PBC_INK : PBC_DIM);
        pb_txtc(40 + (int32_t)i * 79, 18, PG[i], i == pu.c_page ? PBC_BG : PBC_MID);
    }
    pb_cpy(v[0], PG[pu.c_page % 3u], 8);
    v[3][0] = 0;
    if (pu.c_page == 0u) {
        pb_txt(8, 38, "THIS SKETCH", PBC_MID);
        if (pb.have == 2u)
            pb_txt(8, 58, "THE VOID HAS NO CODE", PBC_INK);
        else if (pb.have) {
            pb_encode(&pb.s, pw);
            for (i = 0; i < 6u; i++) {
                one[0] = pw[i];
                cv_text(36 + (int32_t)i * 30, 54, &FONT_L, one, PBC_WHITE);
            }
        } else
            pb_txt(8, 58, "NONE YET: TRAVEL FIRST", PBC_INK);
        pb_txt(8, 92, "ENTER A CODE", PBC_MID);
        for (i = 0; i < 6u; i++) {
            one[0] = pb.entry[i];
            cv_rect(30 + (int32_t)i * 30, 110, 26, 36, i == pb.cur ? PBC_GOLD : PBC_DIM);
            cv_text(35 + (int32_t)i * 30, 112, &FONT_L, one, i == pb.cur ? PBC_BG : PBC_INK);
        }
        pb_txt(8, 152, "SAVE: TRAVEL TO THIS CODE", (ui.frame >> 4 & 1u) ? PBC_WHITE : PBC_GOLD);
        pb_txt(4, 170, "A CODE REBUILDS THE SKETCH AS", PBC_MID);
        pb_txt(4, 186, "IT WAS MADE. EDITS: THE SLOTS", PBC_MID);
        fmt_int(v[1], pb.cur + 1);
        one[0] = pb.entry[pb.cur % 6u];
        pb_cpy(v[2], one, 8);
        pb_foot(LC, v);
    } else if (pu.c_page == 1u) {
        for (i = 0; i < 4u; i++) {
            int32_t y = 38 + (int32_t)i * 34;
            int hot = i == pu.c_slot;
            cv_rect(4, y, 232, 30, hot ? PBC_DIM : PBC_BG);
            cv_rect(4, y, 232, 1, hot ? PBC_WHITE : PBC_MID); cv_rect(4, y + 29, 232, 1, hot ? PBC_WHITE : PBC_MID);
            one[0] = (char)('A' + i);
            cv_text(10, y - 1, &FONT_L, one, hot ? PBC_WHITE : PBC_MID);
            pb_txt(40, y + 7, (ready >> i) & 1u ? (live_sec == (int32_t)i ? "PLAYING / LAST USED" : "HOLDS A SKETCH") : "EMPTY", (ready >> i) & 1u ? PBC_INK : PBC_MID);
        }
        pb_cpy(line, "SAVE: ", sizeof line);
        one[0] = (char)('A' + pu.c_slot);
        pb_cat(line, pu.c_act == 2u ? "CHAIN THE STORED SLOTS" : pu.c_act ? "PLAY SLOT " : "STORE IN SLOT ", sizeof line);
        if (pu.c_act != 2u)
            pb_cat(line, one, sizeof line);
        pb_txt(8, 178, line, (ui.frame >> 4 & 1u) ? PBC_WHITE : PBC_GOLD);
        pb_cpy(v[1], one, 8);
        pb_cpy(v[2], pu.c_act == 2u ? "CHAIN" : pu.c_act ? "PLAY" : "STORE", 8);
        pb_foot(LS, v);
    } else if (pu.c_item == CI_ABOUT) {
        pb_txt(4, 38, FELUCCA_VERSION, PBC_WHITE);
        pb_txt(4, 54, "GITHUB.COM/ELPABL0/POCKETBAND", PBC_INK);
        pb_txt(4, 74, "ENGINES, SEQUENCER, DRIVERS:", PBC_MID);
        pb_txt(4, 90, "SLOOP 2.8.5 BY ISOD89, FROM", PBC_INK);
        pb_txt(4, 106, "FELUCCA BY LEO KUROSHITA,", PBC_INK);
        pb_txt(4, 122, "H\xDCGELTON INSTRUMENTS", PBC_INK);
        pb_txt(4, 142, "GPL-3.0, NO WARRANTY", PBC_GOLD);
        pb_txt(4, 160, "FONT: TERMINUS (OFL)", PBC_MID);
        pb_txt(4, 176, "SAMPLES: VERSILIAN, SONIC PI", PBC_MID);
        pb_txt(4, 190, "(CC0)   SAVE: TAKE A BOW", PBC_MID);
        pb_cpy(v[1], "ABOUT", 8); v[2][0] = 0;
        pb_foot(LU, v);
    } else {
        uint32_t top = pu.c_item > 4u ? pu.c_item - 4u : 0u;
        for (i = 0; i < 7u && top + i < CI_N; i++) {
            int32_t y = 38 + (int32_t)i * 22;
            int hot = top + i == pu.c_item;
            if (hot)
                cv_rect(2, y, 236, 20, PBC_DIM);
            pb_txt(8, y + 2, CI_NAME[top + i], hot ? PBC_WHITE : PBC_INK);
            if (top + i < CI_CAL) {
                ct_value(top + i, line);
                cv_text(232 - text_w(&FONT_S, line), y + 2, &FONT_S, line, hot ? PBC_GOLD : PBC_MID);
            } else
                cv_text(232 - text_w(&FONT_S, "SAVE"), y + 2, &FONT_S, "SAVE", hot ? PBC_GOLD : PBC_MID);
        }
        pb_cpy(v[1], "", 8);
        fmt_int(v[1], pu.c_item + 1);
        ct_value(pu.c_item, v[2]);
        pb_foot(LU, v);
    }
}
UI_SMALL static void ct_knob(uint32_t k, int32_t s)
{
    if (k == 0u) { pu.c_page = (uint8_t)((pu.c_page + (s > 0 ? 1u : 2u)) % 3u); return; }
    if (pu.c_page == 0u) {
        if (k == 1u) pb.cur = (uint8_t)((pb.cur + (s > 0 ? 1u : 5u)) % 6u);
        else if (k == 2u) {
            uint32_t at = 0;
            while (at < 32u && PB_ALPHA[at] != pb.entry[pb.cur]) at++;
            pb.entry[pb.cur] = PB_ALPHA[(at % 32u + (s > 0 ? 1u : 31u)) % 32u];
        }
    } else if (pu.c_page == 1u) {
        if (k == 1u) pu.c_slot = (uint8_t)((pu.c_slot + (s > 0 ? 1u : 3u)) % 4u);
        else if (k == 2u) pu.c_act = (uint8_t)clamp(pu.c_act + (s > 0 ? 1 : -1), 0, 2);
    } else {
        if (k == 1u) pu.c_item = (uint8_t)clamp(pu.c_item + (s > 0 ? 1 : -1), 0, CI_N - 1);
        else if (k == 2u) ct_set(pu.c_item, s > 0 ? 1 : -1);
    }
}
UI_SMALL static void ct_action(void)
{
    uint32_t ga, gb, bl, seed, s = pu.c_slot;
    if (pu.c_page == 0u) {
        /* four words that are not places */
        if (!memcmp(pb.entry, "FASTER", 6)) {
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] * 5 / 4, GP[G_BPM].min, GP[G_BPM].max);
            ui_message("FASTER!");
        } else if (!memcmp(pb.entry, "SLEEPY", 6)) {
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] * 3 / 4, GP[G_BPM].min, GP[G_BPM].max);
            if (pb.have) { pb.energy = 4; pb_rewrite_all(); }
            ui_message("SHHH...");
        } else if (!memcmp(pb.entry, "PARTY2", 6)) {
            uint32_t t;
            pu.party = (uint8_t)!pu.party;
            if (pu.party) {
                for (t = 0; t < NTRK; t++)
                    trk[t].p[P_MUTE] = 0;
                if (pb.have) { pb.energy = 20; pb_rewrite_all(); }
            }
            ui_message(pu.party ? "PARTY!" : "THE PARTY IS OVER");
        } else if (!memcmp(pb.entry, "SECRET", 6))
            pb_void();
        else if (pb_decode(pb.entry, &ga, &gb, &bl, &seed))
            pb_travel(ga, gb, bl, seed);
        else
            ui_message("NO SUCH PLACE");
    } else if (pu.c_page == 1u) {
        if (!pu.c_act) {
            section_store(s);
            ui_message("STORED");
        } else if (pu.c_act == 2u) {         /* the stored slots one after the other, each for its own length, round */
            uint8_t list[4];
            uint32_t n = 0, i;
            for (i = 0; i < 4u; i++)
                if ((arrangement_ready() >> i) & 1u)
                    list[n++] = (uint8_t)i;
            if (n < 2u) ui_message("STORE TWO SLOTS FIRST");
            else if (!song.playing) ui_message("PRESS PLAY FIRST");
            else {
                fm1_irq_off();
                for (i = 0; i < n; i++)
                    chain_sec[i] = list[i];
                chain_i = 0;
                chain_bars = (uint8_t)section_bars(list[0]);
                live_req = (int8_t)list[0];
                chain_n = (uint8_t)n;
                fm1_irq_on();
                ui_message("THE SLOTS PLAY IN TURN");
            }
        } else if (!((arrangement_ready() >> s) & 1u))
            ui_message("THAT SLOT IS EMPTY");
        else if (song.playing) {
            fm1_irq_off(); chain_n = 0; live_req = (int8_t)s; fm1_irq_on();
            ui_message("PLAYS FROM THE NEXT BAR");
        } else {
            section_load(s);
            ui_message("LOADED");
        }
    } else if (pu.c_item == CI_CAL) {
        if (song.playing) ui_message("STOP FIRST");
        else { panel_setup(); settings_save(); }
    } else if (pu.c_item == CI_ABOUT) {
        uint32_t t;                          /* the band takes a bow: everyone awake, the first sketch's home town */
        for (t = 0; t < NTRK; t++)
            trk[t].p[P_MUTE] = 0;
        ui_message("THANK YOU FOR PLAYING");
    } else if (pu.c_item == CI_NEW) {
        project_new();
        pb.have = 0;
        ui_message("EMPTY: TRAVEL SOMEWHERE");
    }
}

/* ------------------------------------------------------------------ the frame, the buttons, the LEDs --- */
UI_SMALL static void vs_mode(uint32_t m)      /* the run takes the keys (no notes); the others give them back */
{
    pu.v_mode = (uint8_t)(m % 4u);
    pu.r_on = 0;
    ly_lock = pb.scr == PS_VIS && pu.v_mode == 3u ? LY_MIX : LY_PLAY;
}
UI_SMALL static void go_home(void)
{
    static uint8_t booted;
    pb.scr = PS_STAGE;
    ui.force = 1;
    if (!booted) {
        booted = 1;
        pb_store_boot();                     /* the sketch as it was when the FM-1 was switched off */
    }
}
UI_SMALL static void pb_goto(uint32_t s)
{
    if (pb.scr == PS_AUDITION) au_leave();
    if (pb.scr == PS_PERFORM) pf_leave();
    if (pb.scr == PS_VIS) { ly_lock = LY_PLAY; pu.r_on = 0; }
    pb.scr = (uint8_t)(s % PS_N);
    if (s == PS_VIS) vs_mode(pu.v_mode);
    if (s == PS_AUDITION && pb.have) au_build();
    if (s == PS_GRID) gr_focus();
    if (s == PS_PERFORM) pf_enter();
    ui.force = 1;
}
UI_SMALL static void pb_action(void)         /* the screen's own button again */
{
    switch (pb.scr) {
    case PS_STAGE: pb_wake_toggle(); break;
    case PS_MAP: pb_go(); break;
    case PS_AUDITION:
        if (pb_need_sketch()) break;
        if (!pu.a_pick) { ui_message("TURN KNOB 1 FIRST"); break; }
        pu.a_pick = 0;                       /* hired: this is the member now */
        au_build();
        ui_message("HIRED");
        break;
    case PS_GRID: gr_put(gr_drum() ? (uint32_t)pu.g_row : pb_note(&pb.s, pu.g_row), 1); break;
    case PS_SOUND: pu.s_page = (uint8_t)((pu.s_page + 1u) % 3u); break;
    case PS_JAM: pu.j_bar = clk_beat / 4u; jm_change(); break;
    case PS_PERFORM: bd_arm(); break;
    case PS_KEY: ky_next_chords(); break;
    case PS_VIS:
        if (pu.v_mode == 3u && pu.r_on != 1u) rn_start();
        else vs_mode((pu.v_mode + 1u) % 4u);
        break;
    default: ct_action(); break;
    }
}
/* TAP: a key's tap is played and recorded as a note the brain picks: the bass its chord's root (higher keys the
 * fifth, the octave), the keys the whole chord, the lead a chord or scale tone that rises with the key; the drums
 * kick, snare, hat by thirds of the keyboard. The sequencer's own recording puts it on its step. */
UI_SMALL static void tp_off(uint32_t i)
{
    fm1_irq_off();
    input_off(&trk[pu.off[i].trk % NPART], pu.off[i].note);
    fm1_irq_on();
    pu.off[i].on = 0;
}
UI_SMALL static void tp_tick(void)
{
    uint32_t i;
    for (i = 0; i < 6u; i++)
        if (pu.off[i].on && fm1_ms - pu.off[i].t0 >= 110u)
            tp_off(i);
}
UI_SMALL static void tp_key(uint32_t k)
{
    static const int8_t ST[6] = {0, 2, 4, 7, 9, 11};
    track_t *T = TSEL;
    uint32_t sel = song.sel % NTRK, n = 0, i, j, role;
    int32_t c = pb.s.prog[(T->seq_idx >> 4) & 3u];
    uint8_t nt[4];
    if (sel == TRK_DRUM) {
        fm1_irq_off();
        drum_input(k < 9u ? 0u : k < 18u ? 2u : 4u, LV_NORM, 0, 1);
        fm1_irq_on();
        return;
    }
    role = pb.s.role[sel];
    if (role == PB_BASS) {
        uint32_t base = pb_fold(pb_note(&pb.s, 21 + c), 28u, 40u);
        nt[n++] = (uint8_t)(base + (k < 9u ? 0u : k < 18u ? pb_note(&pb.s, 21 + c + 4) - pb_note(&pb.s, 21 + c) : 12u));
    } else if (role == PB_CHORD || role == PB_PAD)
        n = pb_tones(&pb.s, c, 53u, 65u, nt);
    else
        nt[n++] = (uint8_t)pb_fold(pb_note(&pb.s, 35 + c + ST[k * 6u / 27u]), 60u, 87u);
    for (i = 0; i < n; i++) {
        for (j = 0; j < 6u && pu.off[j].on; j++) {}
        if (j == 6u) { j = 0; tp_off(0); }   /* (more than six sounding: the first ends early) */
        fm1_irq_off();
        input_on(T, nt[i], 100);
        fm1_irq_on();
        pu.off[j].on = 1; pu.off[j].trk = (uint8_t)sel; pu.off[j].note = nt[i]; pu.off[j].t0 = fm1_ms;
    }
}
UI_SMALL static void pb_rec(void)
{
    track_t *T = TSEL;
    if (pu.t_on) {
        song.rec = 0; rec_wait = 0; pu.t_on = 0;
        ui_message("REC OFF");
        return;
    }
    if (song.rec || rec_wait) {              /* REC again: TAP. The part is emptied; any key taps its rhythm in, and the
                                              * notes are picked for you from the chord of the bar (tp_key) */
        undo_mark(T, (undo_sess += 4u) | 3u);
        fm1_irq_off();
        steps_clear(T);
        T->seq_active = 1;
        panic_req |= (uint8_t)(1u << (song.sel % NTRK));
        fm1_irq_on();
        T->p[P_MUTE] = 0;
        pu.t_on = 1;
        ui_message("TAP: ANY KEY, PART EMPTIED");
        return;
    }
    arrangement_enabled = 0;
    if (song.playing)
        rec_begin();
    else {
        rec_wait = 1;                        /* (PLAY starts it; so does the first note) */
        transport_req = 1;
    }
    ui_say("REC: PLAY THE ", PB_MEMBER[song.sel % NTRK]);
}
UI_SMALL static void ui_input(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), id, k;
    int32_t s;
    enc_hold = 0;
    if (pressed || notes)
        ui_input_ms = fm1_ms;
    midi_chan_fix();
    fm6_poll();
    lk_r = lk_w;                             /* (no layer's keys to hear of) */
    step_in_r = step_in_w;
    rec_go = 0; ft_bars = 0; mono_hint = 0; er_flash = 0;
    jm_tick();
    bd_tick();
    tp_tick();
    if (pu.t_on && !song.rec && !rec_wait)
        pu.t_on = 0;                         /* (STOP ended the recording: the tap mode with it) */
    for (id = 0; id < 14u; id++) {
        uint32_t b;
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        if (b == B_PLAY)
            transport_req = song.playing ? 2 : 1;
        else if (b == B_REC)
            pb_rec();
        else if (b == B_OCTDN || b == B_OCTUP) {
            if (pb.scr == PS_JAM)
                jm_judge(b == B_OCTUP);
            else if (pb.scr == PS_VIS && pu.v_mode == 3u)
                rn_press(b == B_OCTUP);
            else if (pb.scr != PS_PERFORM && song.sel != TRK_DRUM)
                song.octave = (int8_t)clamp(song.octave + (b == B_OCTUP ? 1 : -1), -3, 3);
        } else
            for (k = 0; k < PS_N; k++)
                if (PS_BTN[k] == b) {
                    if (pb.scr == k) pb_action(); else pb_goto(k);
                }
    }
    if (pb.scr == PS_PERFORM) {              /* the throws, while their buttons are held */
        pu.thr = (uint8_t)(((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u) | (((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u) << 1));
        thr_req = pu.thr;
    }
    if (pu.t_on && notes) {
        for (k = 0; k < 27u; k++)
            if ((notes >> k) & 1u)
                tp_key(k);
    } else if (pb.scr == PS_VIS && pu.v_mode == 3u && notes)
        rn_press((notes >> 13) != 0u);         /* F3..F#4 duck, G4..G5 jump */
    if (pb.scr == PS_GRID && notes && !song.playing && !gr_drum()) {   /* stopped: a key writes its note, the cursor moves on */
        for (k = 0; k < 27u; k++)
            if ((notes >> k) & 1u) {
                uint32_t n = kb_map(TSEL, k);
                if (n < 128u) {
                    gr_put(n, 0);
                    pu.g_step = (uint8_t)((pu.g_step + 1u) % NSTEP);
                }
            }
    }
    if ((s = panel_enc(EN_SELECT)) != 0)
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + s, GP[G_BPM].min, GP[G_BPM].max);
    if ((s = panel_enc(EN_ALGO)) != 0) {
        static const uint8_t NEXT[NTRK] = {1, 2, 3, 0}, PREV[NTRK] = {3, 0, 1, 2};   /* drums, bass, keys, lead, round */
        if (pb.scr == PS_AUDITION) au_leave();
        pb_select(s > 0 ? NEXT[song.sel % NTRK] : PREV[song.sel % NTRK]);
        if (pb.scr == PS_AUDITION && pb.have) au_build();
        if (pb.scr == PS_GRID) gr_focus();
    }
    if ((s = panel_enc(EN_PRESET)) != 0)
        pb_sound_step(song.sel % NTRK, s);
    for (k = 0; k < 4u; k++) {
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        switch (pb.scr) {
        case PS_STAGE: st_knob(k, s); break;
        case PS_MAP: mp_knob(k, s); break;
        case PS_AUDITION: au_knob(k, s); break;
        case PS_GRID: gr_knob(k, s); break;
        case PS_SOUND: so_knob(k, s); break;
        case PS_JAM: jm_knob(k, s); break;
        case PS_PERFORM: pf_knob(k, s); break;
        case PS_KEY: ky_knob(k, s); break;
        case PS_VIS: if (k == 0u) vs_mode((pu.v_mode + (s > 0 ? 1u : 3u)) % 4u); break;
        default: ct_knob(k, s); break;
        }
    }
    /* what the keys are: taps (no notes of their own), PERFORM's effects, the run's jump and duck, or notes */
    ly_lock = (uint8_t)(pu.t_on ? LY_MIX : pb.scr == PS_PERFORM ? LY_FX : pb.scr == PS_VIS && pu.v_mode == 3u ? LY_MIX : LY_PLAY);
}
UI_SMALL static void ui_draw(void)
{
    uint32_t pass;
    int game = pb.scr == PS_VIS && pu.v_mode == 3u, all = 0;
    ui.frame++;
    vs_tick();
    rn_tick();
    pb_store_tick();
    if (!pb.tiles_ok)
        pb_map_build();
    if (!pu.drawn || ui.force) {
        pu.drawn = 1;
        all = 1;
        lcd_fill(0, 0, 240, 240, PBC_BG);
    } else if (!game && (ui.frame & 1u)) {
        return;
    }
    ui.force = 0;
    for (pass = 0; pass < 2u; pass++) {
        if (game && !pass && !all && (ui.frame & 7u))
            continue;                        /* the run: the field every frame, the score above it now and then */
        cv_begin(240, 120, PBC_BG);
        cv_oy = -(int32_t)(pass * 120u);
        switch (pb.scr) {
        case PS_STAGE: st_draw(); break;
        case PS_MAP: mp_draw(); break;
        case PS_AUDITION: if (pb.have) au_draw(); else st_draw(); break;
        case PS_GRID: gr_draw(); break;
        case PS_SOUND: so_draw(); break;
        case PS_JAM: jm_draw(); break;
        case PS_PERFORM: pf_draw(); break;
        case PS_KEY: ky_draw(); break;
        case PS_VIS: vs_draw(); break;
        default: ct_draw(); break;
        }
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
    if (ui.msg_t)
        ui.msg_t--;
}
UI_SMALL static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL], dl[FM1_NCOL], bl[FM1_NCOL];
    uint32_t k, c;
    const track_t *T = TSEL;
    memset(nl, 0, sizeof nl); memset(dl, 0, sizeof dl); memset(bl, 0, sizeof bl);
    for (k = 0; k < PS_N; k++)
        led_put(pb.scr == k ? nl : bl, panel.btn[PS_BTN[k]], 1);
    led_put(nl, panel.btn[B_PLAY], song.playing && clk_pos < BEAT_U / 4u);
    led_put(nl, panel.btn[B_REC], song.rec || (rec_wait && (fm1_ms >> 7 & 1u)));
    led_put(nl, panel.btn[B_OCTDN], pb.scr == PS_PERFORM ? pu.thr & 1u : song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], pb.scr == PS_PERFORM ? (pu.thr >> 1) & 1u : song.octave > 0);
    for (k = 0; k < 27u; k++) {
        int held = (fm1_in.notes >> k) & 1u;
        if (pb.scr == PS_PERFORM)
            led_put(nl, 14u + k, held || (punch.req >= 0 && punch_key_of_fx((uint32_t)punch.req) == k));
        else {
            uint32_t n = song.sel == TRK_DRUM ? 255u : kb_pitch(T, k, song.octave);
            led_put(nl, 14u + k, held);
            if (n < 128u && n % 12u == pb.s.root % 12u)
                led_put(dl, 14u + k, 1);     /* the root glows: where home is */
            if (lights_keys)
                led_put(bl, 14u + k, 1);
        }
    }
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c] & (uint8_t)~nl[c];
        fm1_led_bg[c] = bl[c] & (uint8_t)~nl[c];
    }
    fm1_led_bg_ns = LIGHTS_NS[lights_lvl & 3u];
}
/* the boot screen */
UI_SMALL static void sloop_splash(void)
{
    uint32_t pass, i;
    lcd_fill(0, 0, 240, 240, PBC_BG);
    for (pass = 0; pass < 2u; pass++) {
        cv_begin(240, 120, PBC_BG);
        cv_oy = -(int32_t)(pass * 120u);
        cv_text(72, 48, &FONT_L, "POCKET", PBC_INK);
        cv_text(88, 82, &FONT_L, "BAND", PBC_HOT);
        for (i = 0; i < NTRK; i++)
            pb_sprite((i + 3u) % NTRK, 28 + (int32_t)i * 50, 132, 2, 0);
        pb_txtc(120, 184, FELUCCA_VERSION, PBC_MID);
        pb_txtc(120, 204, "ENGINES: SLOOP / FELUCCA", PBC_MID);
        pb_txtc(120, 220, "GPL-3.0, NO WARRANTY", PBC_MID);
        cv_oy = 0;
        cv_blit(0, pass * 120u);
    }
    lcd_sync();
}
