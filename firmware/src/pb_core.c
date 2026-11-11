/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (the functions taken from SLOOP's ui.c)
 * Pocket Band core: what the engines, the storage and the sequencer need from "the interface" that is not
 * drawing or knobs. Loading a sound, the factory sound list by kind, undo, a new project, the device's
 * button calibration. Taken from SLOOP 2.8.5's ui.c / ui_input.c / ui_layers.c with the screens removed. */
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "POCKET BAND 0.4"
#endif
static void project_save(uint32_t slot);
static void arrangement_save(void);
static void project_load(uint32_t slot);
static void project_erase(uint32_t slot);
static int project_used(uint32_t slot);
static int up_used(uint32_t k);
static int up_arp_of(uint32_t k, int16_t *a);
static int up_load(uint32_t k);
static int up_load_to(track_t *t, uint32_t k);
static uint32_t up_count(void);
static uint32_t up_nth(uint32_t n);
static uint32_t up_rank(uint32_t slot);
static void section_store(uint32_t s);
static void section_load(uint32_t s);
static uint32_t arrangement_ready(void);
static void settings_save(void);

UI_SMALL static uint32_t user_of(const track_t *t)    /* user preset slot its sound came from, UP_SLOTS = none */
{
    return t->user && up_used(t->user - 1u) ? t->user - 1u : UP_SLOTS;
}
static uint32_t up_gen;                      /* bumped on every user bank change (redraws) */
static uint8_t sync_reload;                  /* engine / preset / project / user preset loaded: editor RELOAD push */

/* what the rest of the firmware reads and writes of the interface's state */
static struct {
    uint8_t home, page, menu;    /* (main.c's debug record, the autosave's "not in a menu") */
    volatile uint8_t force;      /* redraw everything (also set from the audio interrupt: a section applied) */
    uint8_t msg_t;               /* frames the message stays */
    uint32_t step_sess;          /* the undo session of a gesture */
    uint32_t frame;
    char msg[28];
} ui;

/* transient message in the top bar: a + b */
UI_SMALL static void ui_say(const char *a, const char *b)
{
    uint32_t n;
    str_cpy(ui.msg, a, sizeof ui.msg);
    n = str_len(ui.msg);
    str_cpy(ui.msg + n, b, sizeof ui.msg - n);
    ui.msg_t = 40;
}

UI_SMALL static void ui_message(const char *s) { ui_say(s, ""); }
UI_SMALL static void midi_chan_fix(void)
{
    static const uint8_t ID[4] = {G_CH1, G_CH2, G_CH3, G_DRCH};
    static int16_t was[4] = {-1, -1, -1, -1};
    uint32_t k, j;
    for (k = 0; k < 4u; k++) {
        int32_t v = song.g[ID[k]], dir, owner = -1;
        if (was[k] < 0 || v == was[k] || !v) {
            was[k] = (int16_t)v;
            continue;
        }
        dir = v > was[k] ? 1 : -1;
        for (;;) {
            int32_t o = -1;
            for (j = 0; j < 4u; j++)
                if (j != k && song.g[ID[j]] == v)
                    o = (int32_t)j;
            if (o < 0)
                break;
            if (owner < 0)
                owner = o;
            v += dir;
            if (v < 1) {                                /* (down past 1: OFF, never taken) */
                v = 0;
                break;
            }
            if (v > 16) {                               /* (none free up there: as it was) */
                v = was[k];
                break;
            }
        }
        if (owner >= 0) {
            char b[8];
            str_cpy(b, "CH ", sizeof b);
            fmt_int(b + 3, song.g[ID[k]]);
            ui_say(b, owner == 3 ? " = DRUMS" : owner == 0 ? " = TRACK 1" : owner == 1 ? " = TRACK 2" : " = TRACK 3");
            song.g[ID[k]] = (int16_t)v;
        }
        was[k] = (int16_t)v;
    }
}
UI_SMALL static int undo_swap(int redo)
{
    track_t *t;
    int16_t len;
    if (!undo.valid || (uint32_t)!!redo != undo.undone)
        return 0;
    t = &trk[undo.trk % NTRK];
    fm1_irq_off();
    {
        uint32_t i;
        for (i = 0; i < NSTEP; i++) {
            step_t x = t->step[i];
            int8_t m = t->micro[i];
            t->step[i] = undo.st[i];
            undo.st[i] = x;
            t->micro[i] = undo.micro[i];                 /* (2.7) and its nudge */
            undo.micro[i] = m;
        }
    }
    len = t->p[P_SLEN];
    t->p[P_SLEN] = undo.len;
    undo.len = len;
    undo.undone = (uint8_t)!redo;
    fm1_irq_on();
    sync_reload = 1;
    ui.force = 1;
    return 1;
}
static const uint8_t TRK_DEF[NPART][2] = {{0, 0}, {1, 0}, {4, 5}};   /* ANALOG 808 BOOM, DIGITAL RHODES, SAMPLE LOFI FLUTE */
UI_SMALL static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? TRK_DEF[i][0] : 0u; }

UI_SMALL static int seq_is_empty(const track_t *t) { return track_empty(t); }

UI_SMALL static void track_defaults_steps(track_t *t) { steps_clear(t); }

/* what loading a sound (factory or user preset) leaves alone: the mix (LEVEL, PAN, MUTE:
 * the TRACKS faders, and the track FILTER), the pattern parameters (LEN, DIV, SWING, GATE) and the key the part plays
 * in (ROOT, SCALE, QNT, CHORD: the song's; SEL + key sets the root of every part). The SLICER is
 * part of the sound: a factory preset turns it OFF (its defaults), a user preset brings its own */
UI_SMALL static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_SLEN && i <= P_SGATE) ||
           (i >= P_ROOT && i <= P_QUANT) || i == P_CHORD || i == P_TFLT || i == P_STRUM || i == P_VLEAD;
}

/* preset pi of the engine the track asked for: the whole sound (not the pattern parameters) */
/* SLOOP 2.7: a sound loaded while the track's arpeggiator runs keeps it: its settings (MODE … ORDER, never written: the
 * audio ISR never sees them change) and the notes it holds (seq.c arp_keep_req: the panic of this track leaves them).
 * With the arp OFF, or the arp the old sound brought (2.7.1), a preset brings its own arp, as before */
UI_SMALL static int arp_param(uint32_t i) { return i >= P_AMODE && i <= P_AORDER; }
/* 2.7.1: is the arp that runs the sound's own, as the sound brought it (8BIT ARP, a user preset with an arp)? Then the
 * next sound replaces it with its own (most often OFF), as up to 2.6: only an arp you turned on or changed goes on.
 * (2.7 kept any running arp: after 8BIT ARP, every sound arpeggiated.) The sound loaded: t->engine (not eng_req,
 * already the new one when the ENGINE changes), t->preset, or user_of(t) */
UI_SMALL static int arp_is_sounds(const track_t *t)
{
    int16_t a[8];
    uint32_t i, k = user_of(t);
    if (k < UP_SLOTS) {
        if (up_arp_of(k, a))
            return 0;
    } else {
        const engine_t *e = ENGINES[t->engine % NENGINES];
        const preset_t *pr;
        if (t->user || !e->npresets)
            return 0;
        pr = &e->presets[t->preset % e->npresets];
        for (i = 0; i < 8u; i++)
            a[i] = (int16_t)(i < 4u && pr->arp[i] ? pr->arp[i] - 1 : TP[P_AMODE + i].def);
    }
    for (i = 0; i < 8u; i++)
        if (t->p[P_AMODE + i] != a[i])
            return 0;
    return 1;
}
UI_SMALL static void apply_preset_to(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i;
    int keep_arp = t->p[P_AMODE] != 0 && !arp_is_sounds(t), kbd_hold = !t->p[P_AMODE] && t->p[P_AHOLD];   /* (2.7: KBD HOLD stays on) */
    if (is_drum(t))
        return;
    if (keep_arp)
        arp_keep_req |= (uint8_t)(1u << trk_index(t));   /* (before the panic below) */
    sound_change(t);                                  /* (2.6: what sounds fades out with the old sound, no click) */
    panic_req |= (uint8_t)(1u << trk_index(t));       /* MONO/POLY may change: release what sounds */
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    if (!e->npresets)
        return;
    pi %= e->npresets;
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)                        /* the rest of the sound to its defaults: a preset */
        if (!param_kept(i) && !(keep_arp && arp_param(i)) && !(kbd_hold && i == P_AHOLD))
            t->p[i] = TP[i].def;                     /* sounds the same after any edit (not the pattern, not the mix) */
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)e->presets[pi].e[i];
    t->p[P_ATK] = e->presets[pi].env[0];
    t->p[P_DEC] = e->presets[pi].env[1];
    t->p[P_SUS] = e->presets[pi].env[2];
    t->p[P_REL] = e->presets[pi].env[3];
    t->p[P_ED_FLT] = e->presets[pi].fenv;
    t->p[P_ED_FX] = preset_trim(t->eng_req % NENGINES, pi);  /* level-matched (tools/level_presets.py) */
    t->p[P_VOICE] = e->presets[pi].mono ? V_LEGATO : V_POLY;   /* mono presets keep the legato feel */
    {   /* the rest of the patch: sends, arpeggiator (never a pattern: LIVE) */
        static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
        const preset_t *pr = &e->presets[pi];
        for (i = 0; i < 4u; i++) {
            t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
            if (!keep_arp)
                t->p[P_AMODE + i] = (int16_t)(pr->arp[i] ? pr->arp[i] - 1 : TP[P_AMODE + i].def);
        }
        preset_extras(t->p, pr);                     /* glide, pitch / LFO modulation, voice mode */
    }
    fm6_track_loaded(t);                             /* FM6: the preset's patch (its PTCH) */
}

/* the engine's defaults and its first preset. With the audio IRQ off: the ISR sees the old engine with
 * its values or the new one with its own (voice.c engine_block), never one with the other's */
UI_SMALL static void set_engine_of(track_t *t, uint32_t ei)
{
    const engine_t *e = ENGINES[ei % NENGINES];
    uint32_t i;
    if (is_drum(t))
        return;
    fm1_irq_off();
    locks_restore(t);                                 /* (a lock in force: its base first, then the new sound) */
    sound_change(t);                                  /* (2.6: the sound as it is, before anything new is written) */
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    apply_preset_to(t, 0);
    fm1_irq_on();
}

UI_SMALL static void apply_preset(uint32_t pi) { apply_preset_to(TSEL, pi); }
UI_SMALL static void set_engine(uint32_t ei) { set_engine_of(TSEL, ei); }

UI_SMALL static void track_defaults(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    track_defaults_steps(t);
}
/* the factory presets as one list by kind (basses, keys, organs, pads, leads, plucks and bells, stabs,
 * the rest), then the used user presets: the PRESETS knob and the PRESETS page browse it. By name: an
 * engine's preset table may change order; tests/ui_pages_test.c checks every preset is here once */
enum { BK_BASS, BK_KEYS, BK_ORGAN, BK_PAD, BK_LEAD, BK_PLUCK, BK_STAB, BK_FX };
static const char *const BANK_KIND[] = {"BASS", "KEYS", "ORGN", "PAD", "LEAD", "PLCK", "STAB", "FX"};
static const struct { uint8_t kind, e; const char *name; } BANK[] = {
    {BK_BASS, 0, "808 BOOM"}, {BK_BASS, 0, "808 DIRTY"}, {BK_BASS, 0, "808 SLIDE"}, {BK_BASS, 0, "SUB BASS"},
    {BK_BASS, 0, "PLUGG BASS"}, {BK_BASS, 0, "REESE"}, {BK_BASS, 0, "WOBBLE"}, {BK_BASS, 0, "ACID 303"},
    {BK_BASS, 1, "FM BASS"}, {BK_BASS, 2, "CZ BASS"}, {BK_BASS, 6, "FAT BASS"}, {BK_BASS, 0, "FUNK BASS"},
    {BK_BASS, 5, "WOW BASS"}, {BK_BASS, 3, "GB BASS"}, {BK_BASS, 4, "UP BASS"}, {BK_BASS, 4, "DEEP BASS"},
    {BK_BASS, ENGI_FM6, "ROUND BASS"}, {BK_BASS, 1, "BASS GTR"}, {BK_BASS, 0, "MONO BASS"},
    {BK_BASS, ENGI_PHYS, "PLUCK BASS"}, {BK_BASS, 3, "NES BASS"}, {BK_BASS, 0, "SAW BASS"},
    {BK_KEYS, 1, "RHODES"}, {BK_KEYS, 1, "DX RHODES"}, {BK_KEYS, 1, "WURLI"}, {BK_KEYS, 1, "M1 PIANO"},
    {BK_KEYS, 1, "AFRO KEYS"}, {BK_KEYS, 4, "GRAND PNO"}, {BK_KEYS, 4, "DUSTY PNO"}, {BK_KEYS, 4, "LOFI KEYS"},
    {BK_KEYS, 2, "SOFT KEYS"}, {BK_KEYS, 1, "CLAV"}, {BK_KEYS, ENGI_FM6, "TINE EP"}, {BK_KEYS, 1, "FM GRAND"},
    {BK_ORGAN, 7, "SOUL ORGAN"}, {BK_ORGAN, 7, "GOSPEL"}, {BK_ORGAN, 7, "JAZZ ORGAN"},
    {BK_ORGAN, 7, "DIRTY B3"}, {BK_ORGAN, 7, "HOUSE ORGN"}, {BK_ORGAN, ENGI_FM6, "DRAWBARS"},
    {BK_ORGAN, 6, "ACCORDION"}, {BK_ORGAN, 7, "ROCK ORGAN"}, {BK_ORGAN, 7, "CHAPEL"},
    {BK_ORGAN, 7, "REGGAE ORG"}, {BK_PAD, 0, "WARM PAD"}, {BK_PAD, 6, "SAW PAD"}, {BK_PAD, 1, "GLASS PAD"},
    {BK_PAD, 0, "DARK STR"}, {BK_PAD, 2, "CZ STRING"}, {BK_PAD, 0, "ATMOS PAD"}, {BK_PAD, 8, "LOFI CLOUD"},
    {BK_PAD, 8, "VIBE HAZE"}, {BK_PAD, 5, "CHOIR AAH"}, {BK_PAD, 5, "SOUL OOH"}, {BK_PAD, ENGI_FM6, "SOFT PAD"},
    {BK_PAD, 6, "ENSEMBLE"}, {BK_PAD, 8, "STR CLOUD"}, {BK_PAD, 8, "SHIMMER"}, {BK_PAD, 8, "DRONE"},
    {BK_PAD, ENGI_PHYS, "BOWED MTL"}, {BK_PAD, ENGI_NOISE, "WIND"}, {BK_PAD, ENGI_PHYS, "CELLO BOW"},
    {BK_PAD, ENGI_PHYS, "GLASS BOWL"}, {BK_PAD, ENGI_NOISE, "OCEAN"}, {BK_PAD, 2, "CZ PAD"},
    {BK_PAD, 5, "VOX PAD"}, {BK_PAD, 8, "PNO FREEZE"}, {BK_PAD, 8, "HORN CLOUD"}, {BK_PAD, 0, "SINE PAD"},
    {BK_PAD, 6, "PULSE PAD"}, {BK_LEAD, 0, "SUPERSAW"}, {BK_LEAD, 0, "G-FUNK LD"}, {BK_LEAD, 6, "SYNC LEAD"},
    {BK_LEAD, 6, "HOOVER"}, {BK_LEAD, 5, "TALKBOX"}, {BK_LEAD, 3, "GAME LEAD"}, {BK_LEAD, 4, "LOFI FLUTE"},
    {BK_LEAD, 8, "FLUTE DUST"}, {BK_LEAD, 0, "FAT LEAD"}, {BK_LEAD, 0, "PAN FLUTE"}, {BK_LEAD, 5, "HARMONICA"},
    {BK_LEAD, ENGI_NOISE, "NZ ARCADE"}, {BK_LEAD, 3, "WAVE LEAD"}, {BK_LEAD, 2, "CZ LEAD"},
    {BK_LEAD, 5, "ROBOT"}, {BK_LEAD, 0, "PWM LEAD"}, {BK_LEAD, 6, "SQR LEAD"}, {BK_PLUCK, 0, "TRAP PLUCK"},
    {BK_PLUCK, 2, "RESO PLUCK"}, {BK_PLUCK, 1, "PLUGG BELL"}, {BK_PLUCK, 1, "TRAP BELL"},
    {BK_PLUCK, 1, "MUSIC BOX"}, {BK_PLUCK, 1, "KALIMBA"}, {BK_PLUCK, 1, "MARIMBA"}, {BK_PLUCK, 4, "VIBES"},
    {BK_PLUCK, 3, "8BIT ARP"}, {BK_PLUCK, ENGI_FM6, "GLASS BELL"}, {BK_PLUCK, ENGI_FM6, "WOOD BARS"},
    {BK_PLUCK, ENGI_FM6, "NYLON PICK"}, {BK_PLUCK, 1, "HARP"}, {BK_PLUCK, 1, "STEEL DRUM"},
    {BK_PLUCK, 1, "GLOCKEN"}, {BK_PLUCK, 1, "CELESTA"}, {BK_PLUCK, 1, "XYLOPHONE"},
    {BK_PLUCK, ENGI_PHYS, "STR PLUCK"}, {BK_PLUCK, ENGI_PHYS, "SITAR"}, {BK_PLUCK, ENGI_PHYS, "PHYS HARP"},
    {BK_PLUCK, ENGI_PHYS, "BELL TREE"}, {BK_PLUCK, ENGI_PHYS, "MODAL BAR"}, {BK_PLUCK, ENGI_PHYS, "THUMB PNO"},
    {BK_PLUCK, ENGI_PHYS, "NYLON GTR"}, {BK_PLUCK, ENGI_PHYS, "STEEL GTR"}, {BK_PLUCK, ENGI_PHYS, "MUTED GTR"},
    {BK_PLUCK, ENGI_PHYS, "KOTO"}, {BK_PLUCK, ENGI_PHYS, "BANJO"}, {BK_PLUCK, ENGI_PHYS, "TANPURA"},
    {BK_PLUCK, ENGI_PHYS, "CHIMES"}, {BK_PLUCK, ENGI_PHYS, "BIG BELL"}, {BK_PLUCK, ENGI_PHYS, "VIBRA BAR"},
    {BK_PLUCK, ENGI_PHYS, "STEEL PAN"}, {BK_PLUCK, ENGI_PHYS, "WOOD BLOCK"}, {BK_PLUCK, 3, "1BIT BEEP"},
    {BK_PLUCK, 2, "CZ RESO"}, {BK_PLUCK, 1, "TUBE BELL"}, {BK_PLUCK, 1, "FM PLUCK"}, {BK_STAB, 6, "MIN STAB"},
    {BK_STAB, 6, "MIN7 STAB"}, {BK_STAB, 6, "RAVE STAB"}, {BK_STAB, 6, "DUB CHORD"}, {BK_STAB, 0, "SYN BRASS"},
    {BK_STAB, 2, "CZ BRASS"}, {BK_STAB, 4, "HORN STAB"}, {BK_STAB, 4, "STRING STB"},
    {BK_STAB, ENGI_FM6, "BRASS SECT"}, {BK_STAB, 6, "80S POLY"}, {BK_STAB, 3, "CHIP CHORD"},
    {BK_STAB, 6, "POWER STAB"}, {BK_FX, 4, "SCRATCH"}, {BK_FX, 4, "GM KIT"}, {BK_FX, ENGI_PHYS, "HAND DRUM"},
    {BK_FX, ENGI_PHYS, "MEMB TOMS"}, {BK_FX, ENGI_NOISE, "RAIN"}, {BK_FX, ENGI_NOISE, "NZ METAL"},
    {BK_FX, ENGI_PHYS, "TABLA"}, {BK_FX, ENGI_PHYS, "CONGA"}, {BK_FX, ENGI_PHYS, "TIMPANI"},
    {BK_FX, ENGI_NOISE, "VINYL"}, {BK_FX, ENGI_NOISE, "HISS"}, {BK_FX, ENGI_NOISE, "RISER"},
    {BK_FX, ENGI_NOISE, "NZ SNARE"}, {BK_FX, ENGI_NOISE, "BITCRUSH"}, {BK_FX, ENGI_NOISE, "RADIO"},
    {BK_FX, 3, "CHIP NOISE"},
#if FELUCCA_SLICE
    {BK_FX, ENGI_SLICE, "BREAK 16"}, {BK_FX, ENGI_SLICE, "CHOP 8"}, {BK_FX, ENGI_SLICE, "REVERSE"},
    {BK_FX, ENGI_SLICE, "USR SLICE"},
#endif
};
#define NBANK (sizeof BANK / sizeof BANK[0])
static uint8_t bank_pi[NBANK];                       /* the preset index of each entry in its engine */
static uint8_t bank_ready;
UI_SMALL static void bank_resolve(void)
{
    uint32_t i, k;
    for (i = 0; i < NBANK; i++) {
        const engine_t *e = ENGINES[BANK[i].e % NENGINES];
        bank_pi[i] = 0xFF;
        for (k = 0; k < e->npresets; k++)
            if (str_eq(e->presets[k].name, BANK[i].name))
                bank_pi[i] = (uint8_t)k;
    }
    bank_ready = 1;
}

/* favourites: SLOOP keeps 16 favourite sounds in the settings record. Pocket Band has no favourites list, but the
 * record's layout stays (project.c stores it), so a device that goes back to SLOOP finds its own again */
#define FAV_N 16u
static uint16_t fav[FAV_N];
static uint8_t fav_dirty;

#define FAVS_MAGIC 0x31564146u                      /* "FAV1" */
typedef struct {
    theme_t th;
    uint32_t fmagic;
    uint16_t fav[FAV_N];
    uint32_t fcrc;
} side_rec_t;
UI_SMALL static void side_fill(side_rec_t *r)                /* what the settings' side record holds now */
{
    memset(r, 0, sizeof *r);
    if (theme_valid)
        r->th = theme;
    r->fmagic = FAVS_MAGIC;
    memcpy(r->fav, fav, sizeof fav);
    r->fcrc = st_crc32(&r->fmagic, 4u + sizeof r->fav);
}
UI_SMALL static void side_take(side_rec_t *r)                /* a side record read back: what checks out (theme, favourites) */
{
    if (theme_check(&r->th)) {
        theme = r->th;
        theme_valid = 1;
    }
    if (r->fmagic == FAVS_MAGIC && r->fcrc == st_crc32(&r->fmagic, 4u + sizeof r->fav))
        memcpy(fav, r->fav, sizeof fav);
}

/* ---- the LEDs: a scratch image a frame, copied in one go (ui_leds) ---- */
static uint8_t led_pos[41];                        /* (col << 3) | row bit, 0xFF = none */

UI_SMALL static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}

UI_SMALL static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}
/* ---- a new, empty project (the sounds and the song's settings at their defaults; the FM-1's own settings stay) ---- */
UI_SMALL static void project_new(void)
{
    uint32_t i;
    transport_req = 2;
    panic_req = (1u << NTRK) - 1u;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        undo_mark(t, (undo_sess += 4u) | 3u);
        fm1_irq_off();
        if (i < NPART)
            sound_change(t);                            /* (2.6: what sounds fades out with its own sound) */
        track_defaults(t);
        if (i < NPART) {
            set_engine_of(t, TRK_DEF[i][0]);
            apply_preset_to(t, TRK_DEF[i][1]);
        }
        fm1_irq_on();
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    memset(drum_tune, 0, sizeof drum_tune);             /* (2.7) every drum sound as the kit has it */
    memset(drum_dec, 0, sizeof drum_dec);
    drum_mute = 0;                                      /* (2.6: the live mutes) */
    for (i = 0; i < G_COUNT; i++)
        if (i != G_SLOT && i != G_DRCH && i != G_SYNC && i != G_MIDI && i != G_ROUTE && (i < G_CH1 || i > G_CHOTH) &&
            i != G_BEND && i != G_FXCH && i != G_DRMAP && i != G_RMCH && i != G_CLKOUT && i != G_FXFADE)                                                                     /* (the
                                                                 * FM-1's settings: GLO > MIDI 2.6) */
            song.g[i] = GP[i].def;
    song.solo = 0;
    song.octave = 0;
    sync_reload = 1;
    ui.force = 1;
}
/* ---- what the audio interrupt reads of the buttons ----
 * SLOOP's "hold a function button and the keys do something else" layers stay off: every ly_bit is 0, so the keys
 * always play. One is kept for the PERFORM screen: LY_FX on a bit no button has, reached only by locking it
 * (ly_lock = LY_FX): the 27 keys are then the punch-in effects, handled in the interrupt as in SLOOP. */
#define PB_FX_BIT (1u << 20)
UI_SMALL static void layers_init(void)
{
    memset(ly_bit, 0, sizeof ly_bit);
    ly_bit[LY_FX] = PB_FX_BIT;
    ly_bit[LY_MIX] = PB_FX_BIT << 1;                     /* (the run: the keys play no notes, locked the same way) */
    dyn_bit[0] = 1u << panel.btn[B_OCTDN];               /* the drum track: OCT- held a ghost hit, OCT+ a hard one */
    dyn_bit[1] = 1u << panel.btn[B_OCTUP];
    ft_btn_mask = 1u << panel.btn[B_REC];
    ft_drop_mask = 1u << panel.btn[B_PLAY];
    rec_tempo = 1;                                       /* recording follows the tempo: no free take, no count-in */
    rec_count = 0;
    led_pos_init();
}
/* ---- HARDWARE CALIBRATION: teach each button and knob (OCT- + OCT+ at power-on, or SETUP) ---- */
#define SETUP_IDLE_MS 30000u
UI_SMALL static void panel_setup(void)
{
    uint32_t i, used = 0, t0 = fm1_ms;
    const panel_t old = panel;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 10, 240, &FONT_S, "HARDWARE CALIBRATION", C_WHITE, 1);
    draw_text_box(0, 30, 240, &FONT_S, "TEACH EACH BUTTON AND KNOB", C_GRAY, 1);
    while (fm1_in.buttons) {                             /* wait for OCT-/OCT+ release */
        fm1_wdt_feed();
        if (fm1_ms - t0 > SETUP_IDLE_MS)
            goto timeout;
    }
    fm1_input_edges(0);
    for (i = 0; i < NB; i++) {
        uint32_t p = 0, id;
        draw_text_box(0, 80, 240, &FONT_S, "PRESS", C_GRAY, 1);
        draw_text_box(0, 100, 240, &FONT_L, B_NAME[i], C_WHITE, 1);
        t0 = fm1_ms;
        while (!(p & ~used)) {
            fm1_wdt_feed();
            p |= fm1_input_edges(0);
            if (fm1_ms - t0 > SETUP_IDLE_MS)
                goto timeout;
        }
        for (id = 0; id < 14u; id++)
            if (((p & ~used) >> id) & 1u)
                break;
        panel.btn[i] = (uint8_t)id;
        used |= 1u << id;
    }
    used = 0;
    for (i = 0; i < NE; i++) {
        uint32_t e;
        int32_t st = 0;
        draw_text_box(0, 80, 240, &FONT_S, "TURN RIGHT", C_GRAY, 1);
        draw_text_box(0, 100, 240, &FONT_L, E_NAME[i], C_WHITE, 1);
        for (e = 0; e < 7u; e++)
            fm1_enc_take(e);
        t0 = fm1_ms;
        for (;;) {
            fm1_wdt_feed();
            if (fm1_ms - t0 > SETUP_IDLE_MS)
                goto timeout;
            for (e = 0; e < 7u; e++)
                if (!((used >> e) & 1u) && (st = fm1_enc_take(e)) != 0)
                    break;
            if (e < 7u)
                break;
        }
        panel.enc[i] = (uint8_t)e;
        panel.dir[i] = (int8_t)(st > 0 ? 1 : -1);
        used |= 1u << e;
        fm1_delay_ms(300);
        fm1_enc_take(e);
    }
    panel.magic = PANEL_MAGIC;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    layers_init();
    ui.force = 1;
    return;
timeout:
    panel = old;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.force = 1;
    ui_message("SETUP CANCELLED");
}
