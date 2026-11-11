/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band, the whole firmware interface (firmware/src/pb_ui.c) on the host: the real screens over the real
 * sequencer and sound engines, with panel / flash doubles. Drives every screen, knob and button, checks what
 * lands in the tracks, renders audio for every genre, fuzzes the panel, and writes screens (DIR/pb-*.ppm). */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
static uint16_t screen[240*240];
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ uint32_t i,j; assert(x+w<=240 && y+h<=240); for(j=0;j<h;j++) for(i=0;i<w;i++) screen[(y+j)*240+x+i]=p[j*w+i]; }
#include "../firmware/src/gfx.c"
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c)
{ uint32_t i,j; assert(x+w<=240&&y+h<=240); for(j=0;j<h;j++)for(i=0;i<w;i++)screen[(y+j)*240+x+i]=swap16(c); }
static int32_t encs[7];
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u * 24u; }
#define FM1_TICKS_PER_US 24u
static int32_t enc_late[7];                         /* a detent the timer ISR counts just after a read (as on the FM-1, where the
                                                     * scan runs while a UI pass does): it lands in the same pass */
static int32_t fm1_enc_take(uint32_t e) { int32_t s = encs[e]; encs[e] = enc_late[e]; enc_late[e] = 0; return s; }
static uint8_t fm1_led[16], fm1_led_dim[16], fm1_led_bg[16];
static volatile uint16_t fm1_led_bg_ns;
#define FM1_NCOL 16u
static const int8_t FM1_KEYMAP[5][16];
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
static uint32_t edges_btn, notes_seen;
static uint32_t fm1_input_edges(int x) { uint32_t e = edges_btn; (void)x; edges_btn = 0; return e; }
static uint32_t fm1_input_note_edges(void) { uint32_t e = fm1_in.notes & ~notes_seen; notes_seen = fm1_in.notes; return e; }
static uint32_t wdt_tick_ms;                       /* (stress_test: 1, the blocking PANEL setup's clock runs out) */
static void fm1_wdt_feed(void) { fm1_ms += wdt_tick_ms; }
static int32_t fm1_adc_read(int c) { (void)c; return -1; }
static struct { uint32_t magic, stage, page, home, ui_frames; } felucca_dbg;
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N], scope_bufr[SCOPE_N];
static uint32_t scope_w;
#include "../firmware/src/panel.c"
#include "../firmware/src/pb_ui.c"
/* storage doubles: four RAM slots */
static step_t slot_steps[4][NTRK][NSTEP];
static uint32_t slot_used, saves;
static void project_save(uint32_t i) { (void)i; }
static void project_load(uint32_t i) { (void)i; }
static void project_erase(uint32_t i) { (void)i; }
static int project_used(uint32_t i) { return (slot_used >> i) & 1u; }
static void arrangement_save(void) {}
static uint32_t arrangement_ready(void) { return slot_used; }
static void arrangement_apply(uint32_t s) { uint32_t k; for (k = 0; k < NTRK; k++) memcpy(trk[k].step, slot_steps[s][k], sizeof trk[k].step); }
static uint32_t section_bars(uint32_t s) { (void)s; return 4; }
static void song_backup(void) {}
static void song_restore(void) {}
static void section_store(uint32_t s) { uint32_t k; for (k = 0; k < NTRK; k++) memcpy(slot_steps[s][k], trk[k].step, sizeof trk[k].step); slot_used |= 1u << s; live_sec = (int8_t)s; }
static void section_load(uint32_t s) { arrangement_apply(s); live_sec = (int8_t)s; }
static int up_used(uint32_t k) { (void)k; return 0; }
static int up_load(uint32_t k) { (void)k; return 1; }
static int up_load_to(track_t *t, uint32_t k) { (void)t; (void)k; return 1; }
static int up_arp_of(uint32_t k, int16_t *a) { (void)k; (void)a; return 1; }
static uint32_t up_count(void) { return 0; }
static uint32_t up_nth(uint32_t n) { return n; }
static uint32_t up_rank(uint32_t s) { return s; }
static void settings_save(void) { saves++; }
static uint8_t nor[0x100000];               /* the 1 MiB flash, in RAM */
static uint32_t nor_erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { if (off + n > sizeof nor) return -1; memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { if (off + 4096u > sizeof nor || (off & 4095u)) return -1; memset(nor + off, 0xFF, 4096); nor_erases++; return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { uint32_t i; if (off + n > sizeof nor) return -1; for (i = 0; i < n; i++) nor[off + i] &= ((const uint8_t *)src)[i]; return 0; }
#define ST_XIP_PTR(off) (nor + (off))
#include "../firmware/src/storage.c"
#define FELUCCA_FLASH 1
static uint8_t flash_ok = 1;
static int audio_quiet(void) { return 1; }
#include "../firmware/src/pb_store.c"

static const char *outdir;
static int fails;
static void check(int ok, const char *what) { printf("pb: %-78s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }
static void ppm(const char *name)
{
    char path[512]; unsigned i; FILE *f;
    snprintf(path, sizeof path, "%s/%s.ppm", outdir, name);
    f = fopen(path, "wb"); assert(f); fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240 * 240; i++) { uint16_t p = swap16(screen[i]); uint8_t rgb[3] = {(p >> 11) * 255 / 31, ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31}; fwrite(rgb, 1, 3, f); }
    fclose(f);
}
static FILE *wav;
static uint32_t wav_frames, wav_clip;
static int64_t wav_energy;
static void frame(void)                     /* one UI frame (~16 ms): the audio between, then input, LEDs, draw */
{
    static int32_t o[CTL * 2];
    uint32_t q, i;
    for (q = 0; q < 22u; q++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            if (o[2 * i] >= 32700 || o[2 * i] <= -32700) wav_clip++;
            wav_energy += (int64_t)o[2 * i] * o[2 * i];
            if (wav) wav_put(wav, o[2 * i], o[2 * i + 1]);
            wav_frames++;
            if (i & 1u) { scope_buf[scope_w & (SCOPE_N - 1u)] = (int16_t)clamp(o[2 * i], -32768, 32767); scope_w++; }
        }
    }
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void frames(uint32_t n) { while (n--) frame(); }
static void meter0(void) { wav_frames = wav_clip = 0; wav_energy = 0; }
static uint32_t BT(uint32_t b) { return 1u << panel.btn[b]; }
static void press(uint32_t b) { edges_btn |= BT(b); fm1_in.buttons |= BT(b); frame(); }
static void release(uint32_t b) { fm1_in.buttons &= ~BT(b); frame(); }
static void tap(uint32_t b) { press(b); release(b); }
static void turn(uint32_t e, int32_t d) { encs[panel.enc[e]] = d; frame(); }
static uint32_t steps_used(uint32_t t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NSTEP; i++)
        n += t == TRK_DRUM ? (trk[t].dstep[i].on[0] | trk[t].dstep[i].on[1]) != 0 : (trk[t].step[i].time == ST_NOTE && trk[t].step[i].n);
    return n;
}
static int on_scale(void)
{
    uint32_t t, i, k;
    for (t = 0; t < NPART; t++)
        for (i = 0; i < NSTEP; i++)
            for (k = 0; k < trk[t].step[i].n; k++)
                if (trk[t].step[i].time == ST_NOTE && pb_snap(&pb.s, trk[t].step[i].note[k]) != trk[t].step[i].note[k])
                    return 0;
    return 1;
}
static void sel(uint32_t t) { pb_select(t); frame(); }

int main(int argc, char **argv)
{
    uint32_t i, g, t;
    char name[64];
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    settings.palette = 4;
    palette_set(4);
    memset(nor, 0xFF, sizeof nor);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    layers_init(); go_home(); ui.force = 1;
    bank_resolve();
    for (g = 0; g < PB_GENRES; g++) {
        for (t = 0; t < 5u; t++) { int hit = 0; for (i = 0; i < NBANK; i++) hit |= !strcmp(BANK[i].name, PB_SND[g][t]) && bank_pi[i] != 0xFF; if (!hit) { printf("pb: no sound %s\n", PB_SND[g][t]); fails++; } }
        { int hit = 0; for (i = 0; i < DRUM_KITS; i++) hit |= !strcmp(DRUM_KIT_NAMES[i], PB_SND[g][5]); if (!hit) { printf("pb: no kit %s\n", PB_SND[g][5]); fails++; } }
    }
    check(!fails, "every sound and drum kit the genres name is in the factory bank");
    sloop_splash(); ppm("pb-boot");

    /* boot: the stage, no sketch; the keys play, no layer */
    frames(3); ppm("pb-stage-empty");
    check(pb.scr == PS_STAGE && !pb.have, "boot: the STAGE, no sketch yet");
    check(layer_now() == LY_PLAY, "no button puts the keys in a SLOOP layer");
    press(B_EDIT); check(layer_now() == LY_PLAY, "EDIT held: still the keys play (no erase layer)"); release(B_EDIT);
    tap(B_HOME);
    turn(EN_K1, 1); check(!pb.have && ui.msg_t, "STAGE without a sketch: the knobs say where to go");

    /* MAP: walk, travel */
    tap(B_GLO); check(pb.scr == PS_MAP, "GLO opens the MAP");
    frames(3); ppm("pb-map");
    { int16_t x = pb.mx, y = pb.my; turn(EN_K1, 6); turn(EN_K2, 5); check(pb.mx == x + 24 && pb.my == y + 20, "MAP: KNOB 1 east, KNOB 2 south"); }
    turn(EN_K3, 1); frames(2); ppm("pb-map-town");
    tap(B_GLO);
    check(pb.have && pb.scr == PS_STAGE, "GLO again travels: a sketch, and the STAGE");
    frames(4);
    check(song.playing, "the sketch starts playing");
    check(song.g[G_BPM] == pb.s.bpm && trk[0].p[P_SLEN] == NSTEP, "tempo and 64-step patterns set");
    check(steps_used(0) && steps_used(1) && steps_used(2) && steps_used(TRK_DRUM), "all four tracks have notes");
    check(on_scale(), "every note of the three parts is in the sketch's scale");
    meter0(); frames(400);
    check(wav_energy / wav_frames > 200 * 200 && wav_clip < wav_frames / 200u, "the sketch is audible and does not clip");
    ppm("pb-stage");

    /* STAGE knobs */
    {   step_t keep[NSTEP];
        sel(0); memcpy(keep, trk[0].step, sizeof keep);
        turn(EN_K1, 1); check(pb.idea[0] == 1 && memcmp(keep, trk[0].step, sizeof keep), "STAGE: KNOB 1 right, a new idea");
        turn(EN_K1, -1); check(pb.idea[0] == 0 && !memcmp(keep, trk[0].step, sizeof keep), "STAGE: KNOB 1 left, the idea before, note for note");
    }
    {   uint32_t lo, hi, mid = steps_used(TRK_DRUM), e0 = pb.energy;
        for (i = 0; i < 21u; i++) turn(EN_K2, -1);
        lo = steps_used(TRK_DRUM);
        for (i = 0; i < 25u; i++) turn(EN_K2, 1);
        hi = steps_used(TRK_DRUM);
        check(lo <= mid && mid <= hi && lo < hi, "STAGE: KNOB 2 ENERGY thins and fills the band");
        while (pb.energy > e0) turn(EN_K2, -1);
        check(steps_used(TRK_DRUM) == mid, "the same energy gives the same part again");
    }
    { uint32_t r0; sel(1); r0 = pb.s.role[1]; turn(EN_K3, 1); check(pb.s.role[1] != r0 && steps_used(1), "STAGE: KNOB 3 the keys' role"); }
    { uint32_t r0; sel(TRK_DRUM); r0 = pb.s.src[3]; turn(EN_K4, 1); check(pb.s.src[3] != r0 && steps_used(TRK_DRUM), "STAGE: KNOB 4 borrows the drums from the next town"); }
    { int16_t m0; sel(2); m0 = trk[2].p[P_MUTE]; tap(B_HOME); check(trk[2].p[P_MUTE] == !m0, "STAGE: HOME again, the member sleeps / wakes"); tap(B_HOME); }
    { uint32_t seen = 0; for (i = 0; i < 4u; i++) { turn(EN_ALGO, 1); seen |= 1u << song.sel; } check(seen == 15u, "ALGORITHM selects each of the four members"); }
    { uint8_t p0, e0; sel(2); p0 = trk[2].preset; e0 = trk[2].eng_req; turn(EN_PRESET, 1); check(trk[2].preset != p0 || trk[2].eng_req != e0, "PRESETS: the member's next sound"); }
    { int16_t b0 = song.g[G_BPM]; turn(EN_SELECT, 3); check(song.g[G_BPM] == b0 + 3, "SELECT: the tempo"); turn(EN_SELECT, -3); }
    { sel(0); tap(B_OCTUP); check(song.octave == 1, "OCT+: the keys an octave up"); tap(B_OCTDN); check(song.octave == 0, "OCT-: back"); }

    /* AUDITION */
    {   step_t keep[NSTEP]; uint8_t src0 = pb.s.src[0], idea0 = pb.idea[0];
        sel(0); memcpy(keep, trk[0].step, sizeof keep);
        tap(B_EDIT); check(pb.scr == PS_AUDITION && pu.a_fit[1] >= 40, "EDIT opens the AUDITION: three others, scored");
        turn(EN_K1, 2); check(pu.a_pick == 1, "AUDITION: one detent, one player");   /* (a turn is one step) */
        turn(EN_K1, 1); frames(30); ppm("pb-audition");
        check(pu.a_pick == 2 && memcmp(keep, trk[0].step, sizeof keep), "AUDITION: KNOB 1 puts another player in the mix");
        tap(B_HOME);
        check(pb.s.src[0] == src0 && pb.idea[0] == idea0 && !memcmp(keep, trk[0].step, sizeof keep), "leaving without hiring: the member as it was");
        tap(B_EDIT); turn(EN_K1, 1); tap(B_EDIT);
        check(pu.a_pick == 0 && memcmp(keep, trk[0].step, sizeof keep), "EDIT again hires the player");
        turn(EN_K2, 1); check(pu.a_salt == 1, "AUDITION: KNOB 2, three more");
        tap(B_HOME);
    }

    /* GRID */
    {   uint32_t n0, s0;
        sel(0); tap(B_SEQ); check(pb.scr == PS_GRID, "SEQ opens the GRID");
        frames(3); ppm("pb-grid");
        turn(EN_K1, 3); s0 = pu.g_step; n0 = steps_used(0);
        if (trk[0].step[s0].time == ST_NOTE && trk[0].step[s0].n) { turn(EN_K1, 1); s0 = pu.g_step; }
        turn(EN_K2, 9);                      /* (a row no generated note is on) */
        tap(B_SEQ); check(steps_used(0) >= n0 && trk[0].step[s0].time == ST_NOTE && trk[0].step[s0].n, "GRID: SEQ again puts a note on the cursor");
        { uint8_t nn = trk[0].step[s0].n; tap(B_SEQ); check(trk[0].step[s0].n == nn - 1, "GRID: and again removes it"); }
        check(undo_swap(0), "the edit can be undone");
        turn(EN_K4, 1); check((pu.g_step >> 4) == ((s0 >> 4) + 1u) % 4u, "GRID: KNOB 4, the next bar");
        sel(TRK_DRUM); frames(2); ppm("pb-grid-drums");
        { uint32_t st = pu.g_step; uint8_t b0 = TDRUM->dstep[st].on[0]; tap(B_SEQ); check(TDRUM->dstep[st].on[0] != b0, "GRID on the drums: SEQ toggles the sound on the step"); tap(B_SEQ); }
        sel(0);
    }

    /* SOUND */
    {   int16_t v0;
        tap(B_ENV); check(pb.scr == PS_SOUND && pu.s_page == 0, "ENV opens SOUND");
        frames(3); ppm("pb-sound");
        tap(B_ENV); check(pu.s_page == 1, "ENV again: the next page");
        v0 = trk[0].p[P_TFLT]; turn(EN_K3, -4); check(trk[0].p[P_TFLT] < v0, "SOUND / SHAPE: KNOB 3 closes the member's filter");
        trk[0].p[P_TFLT] = v0;
        tap(B_ENV); v0 = trk[0].p[P_REV]; turn(EN_K1, 5); check(trk[0].p[P_REV] > v0, "SOUND / SPACE: KNOB 1 the reverb");
        sel(TRK_DRUM); tap(B_ENV); v0 = song.g[G_DRLVL]; turn(EN_K2, -5); check(song.g[G_DRLVL] < v0, "SOUND on the drums: KNOB 2 their level");
        song.g[G_DRLVL] = v0; frames(2); ppm("pb-sound-drums"); sel(0);
    }

    /* JAM */
    {   uint8_t ideas[4];
        tap(B_LFO); check(pb.scr == PS_JAM, "LFO opens the JAM");
        memcpy(ideas, pb.idea, 4);
        tap(B_LFO); check(pu.j_wait && pb.idea[pu.j_trk] != ideas[pu.j_trk], "LFO again: one member changes its part now");
        frames(3); ppm("pb-jam");
        tap(B_OCTDN); check(!pu.j_wait && !memcmp(ideas, pb.idea, 4), "JAM: OCT- kills the change");
        tap(B_LFO); tap(B_OCTUP); check(!pu.j_wait && memcmp(ideas, pb.idea, 4), "JAM: OCT+ keeps it");
        turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); check(pu.j_rate == 2, "JAM: KNOB 1 sets how often");
        memcpy(ideas, pb.idea, 4); pu.j_wait = 0;
        frames(600);
        check(memcmp(ideas, pb.idea, 4) != 0, "the band changes parts by itself while it plays");
        turn(EN_K1, -1); turn(EN_K1, -1); turn(EN_K1, -1); check(pu.j_rate == 0, "JAM off");
    }

    /* PERFORM */
    {   tap(B_FX); check(pb.scr == PS_PERFORM && layer_now() == LY_FX, "FX opens PERFORM: the keys are the effects");
        fm1_in.notes |= 1u << 0; frames(4);
        check(punch.req == punch_fx_of_key(0), "PERFORM: a key held holds its effect");
        ppm("pb-perform");
        fm1_in.notes = 0; frames(4);
        check(punch.req < 0, "let go: the effect ends");
        press(B_OCTUP); check(thr_req == 2, "PERFORM: OCT+ held, the echo throw"); release(B_OCTUP); check(thr_req == 0, "let go: no throw");
        { int16_t f0 = song.g[G_FILT]; turn(EN_K1, -5); check(song.g[G_FILT] < f0, "PERFORM: KNOB 1 the filter"); song.g[G_FILT] = f0; }
        fm1_in.notes |= 1u << 3; frames(2);
        tap(B_HOME); fm1_in.notes = 0; frames(2);
        check(layer_now() == LY_PLAY && punch.req < 0 && thr_req == 0, "leaving PERFORM: the keys play again, no effect left on");
    }

    /* KEY */
    {   uint8_t r0 = pb.s.root, c0 = pb.s.prog_set;
        tap(B_SCL); check(pb.scr == PS_KEY, "SEL opens KEY");
        turn(EN_K1, 1); check(pb.s.root == (r0 + 1) % 12 && trk[0].p[P_ROOT] == pb.s.root && on_scale(), "KEY: KNOB 1, the root; the parts move into it");
        turn(EN_K2, 1); check(on_scale() && trk[1].p[P_SCALE] == PB_SCALE_OF[pb.s.scale], "KEY: KNOB 2, the scale; still every note in it");
        tap(B_SCL); check(on_scale() && (pb.s.prog_set != c0 || PB_G[pb.s.prog_src].nprog == 1), "SEL again: the next chords");
        frames(3); ppm("pb-key");
    }

    /* VISUALS */
    tap(B_ARP); check(pb.scr == PS_VIS, "ARP opens VISUALS"); frames(20); ppm("pb-vis-scope");
    tap(B_ARP); frames(20); ppm("pb-vis-meters"); check(pu.v_mode == 1 && (pu.meter[0] || pu.meter[3]), "VISUALS: the meters move");
    tap(B_ARP); frames(20); ppm("pb-vis-stars");

    /* REC: what the keys play lands in the member's part */
    {   uint32_t n0; sel(2);
        for (i = 0; i < NSTEP; i++) { memset(&trk[2].step[i], 0, sizeof(step_t)); trk[2].step[i].time = ST_REST; }
        n0 = steps_used(2);
        tap(B_HOME); tap(B_REC); check(song.rec == 1u << 2, "REC while playing: the selected member records");
        for (i = 0; i < 6u; i++) { fm1_in.notes = 1u << (4 + i); frames(9); fm1_in.notes = 0; frames(6); }
        check(song.rec && steps_used(2) > n0, "the notes played are in its part");
        {   uint32_t k, bad = 0;             /* (0.4) the keys really are in the key: every key, a note of the scale */
            for (k = 0; k < 27u; k++) { uint32_t n = kb_pitch(&trk[2], k, 0); bad += n > 127u || pb_snap(&pb.s, n) != n; }
            check(!bad && trk[0].p[P_QUANT] == 1, "every key plays a note of the sketch's scale (SNAP)");
        }
        /* REC again: TAP. The part is emptied; any key taps a rhythm, the notes are picked from the bar's chord */
        tap(B_REC);
        check(pu.t_on && song.rec && !steps_used(2) && layer_now() == LY_MIX, "REC again: TAP mode, the part emptied, the keys play no notes of their own");
        for (i = 0; i < 8u; i++) { fm1_in.notes = 1u << (3u * i); frames(7); fm1_in.notes = 0; frames(7); }
        frames(20);
        check(steps_used(2) >= 4 && on_scale(), "TAP: the taps are in the part, every note in the scale");
        {   uint32_t st, k, off = 0;
            for (st = 0; st < NSTEP; st++) for (k = 0; k < trk[2].step[st].n; k++)
                if (trk[2].step[st].time == ST_NOTE) { int32_t d = pb_note_deg(&pb.s, trk[2].step[st].note[k]) - pb.s.prog[st >> 4]; d = ((d % 7) + 7) % 7; off += !(d == 0 || d == 2 || d == 4); }
            check(!off, "TAP on the lead: every note a tone of its bar's chord");
        }
        for (i = 0; i < 6u; i++) check(!pu.off[i].on || 1, "");
        frames(12); { uint32_t held = 0; for (i = 0; i < 6u; i++) held += pu.off[i].on; check(!held && !trk[2].nheld, "no tapped note left sounding"); }
        sel(TRK_DRUM); { uint32_t d0 = steps_used(TRK_DRUM); fm1_in.notes = 1u << 2; frames(5); fm1_in.notes = 0; frames(9); fm1_in.notes = 1u << 12; frames(5); fm1_in.notes = 0; frames(9);
          check(steps_used(TRK_DRUM) >= d0, "TAP on the drums: low keys the kick, the middle the snare"); }
        tap(B_REC); check(!pu.t_on && !song.rec && layer_now() == LY_PLAY, "REC a third time: off, the keys play again");
        tap(B_REC); tap(B_REC); press(B_PLAY); release(B_PLAY); frames(6);
        check(!pu.t_on && !song.rec && layer_now() == LY_PLAY, "STOP while tapping: tap mode ends with the recording");
        press(B_PLAY); release(B_PLAY); frames(6); sel(2);
    }

    /* CART */
    {   char pw[7]; pb_song_t s0;
        tap(B_SAVE); check(pb.scr == PS_CART && pu.c_page == 0, "SAVE opens the CART");
        frames(3); ppm("pb-cart-code");
        s0 = pb.s; pb_encode(&pb.s, pw);
        memcpy(pb.entry, "AAAAA?", 7); tap(B_SAVE); check(!memcmp(&s0, &pb.s, sizeof s0), "a code that is not one changes nothing");
        tap(B_SAVE);
        memcpy(pb.entry, pw, 7); tap(B_SAVE); check(pb.s.seed == s0.seed && pb.s.ga == s0.ga && pb.scr == PS_STAGE, "SAVE again travels to the code");
        tap(B_SAVE); turn(EN_K1, 1); check(pu.c_page == 1, "CART: KNOB 1, the SLOTS");
        turn(EN_K2, 1); tap(B_SAVE); check(slot_used == 2u, "SLOTS: stored in B");
        frames(3); ppm("pb-cart-slots");
        turn(EN_K3, 1); tap(B_SAVE); frames(300); check(live_sec == 1, "SLOTS: PLAY B, from the next bar");
        turn(EN_K2, 1); tap(B_SAVE); check(ui.msg_t, "an empty slot says so");
        turn(EN_K1, 1); check(pu.c_page == 2, "CART: SETUP");
        turn(EN_K3, 1); check(lights_lvl == 1 && settings_later, "SETUP: PANEL LIGHTS");
        turn(EN_K2, 1); turn(EN_K2, 1); turn(EN_K3, 1); check(song.g[G_CLOCK] == 1, "SETUP: CLICK");
        frames(2); ppm("pb-cart-setup");
        for (i = 0; i < 12u; i++) turn(EN_K2, 1);
        check(pu.c_item == CI_ABOUT, "SETUP: the last item is ABOUT"); frames(2); ppm("pb-cart-about");
        turn(EN_K2, -1); check(pu.c_item == CI_NEW, "SETUP: NEW SKETCHBOOK");
        tap(B_SAVE); frames(4); check(!pb.have && !steps_used(0) && !steps_used(TRK_DRUM), "NEW: the tracks are empty, no sketch");
        tap(B_HOME);
    }

    /* 0.3: morph, the build and the drop, the chain, skins, the run, the sketch remembered */
    pb_travel(0, 2, 3, 777u); frames(4); tap(B_HOME);
    {   step_t keep[NSTEP]; memcpy(keep, trk[0].step, sizeof keep);
        tap(B_LFO); for (i = 0; i < 6u; i++) turn(EN_K3, 1);
        check(pb.morph == 6 && memcmp(keep, trk[0].step, sizeof keep) && on_scale(), "JAM: KNOB 3 MORPH leans the parts to their next idea, in key");
        for (i = 0; i < 6u; i++) turn(EN_K3, -1);
        check(pb.morph == 0 && !memcmp(keep, trk[0].step, sizeof keep), "MORPH back to 0: the parts as they were");
    }
    {   dstep_t keep[NSTEP]; int16_t f0 = song.g[G_FILT]; uint32_t rolled = 0, hi = 0;
        memcpy(keep, TDRUM->dstep, sizeof keep);
        trk[2].p[P_MUTE] = 1;
        tap(B_FX); tap(B_FX);
        check(pu.b_on == 1, "PERFORM: FX again arms the build");
        for (i = 0; i < 700u && pu.b_on; i++) { frame(); if (pu.b_on == 2u) { rolled |= memcmp(keep, TDRUM->dstep, sizeof keep) != 0 && trk[0].p[P_MUTE]; if (song.g[G_FILT] > hi) hi = (uint32_t)song.g[G_FILT]; } }
        check(rolled && hi > 20u, "the build: a roll on the drums, the bass out, the filter rising");
        check(!pu.b_on && !memcmp(keep, TDRUM->dstep, sizeof keep) && song.g[G_FILT] == f0 && !trk[0].p[P_MUTE] && !trk[2].p[P_MUTE], "the drop: the drums back, the filter as it was, everyone awake");
        tap(B_FX); press(B_PLAY); release(B_PLAY); frames(6);
        check(!song.playing && !pu.b_on && !memcmp(keep, TDRUM->dstep, sizeof keep), "a build armed, then STOP: nothing left changed");
        press(B_PLAY); release(B_PLAY); frames(6); tap(B_HOME);
    }
    {   tap(B_SAVE); while (pu.c_page != 1u) turn(EN_K1, 1);
        slot_used = 0; pu.c_slot = 0; pu.c_act = 0; tap(B_SAVE); turn(EN_K2, 1); tap(B_SAVE);
        turn(EN_K3, 1); turn(EN_K3, 1); check(pu.c_act == 2, "SLOTS: the third action is CHAIN");
        tap(B_SAVE); check(chain_n == 2 && chain_sec[0] == 0 && chain_sec[1] == 1, "CHAIN: the stored slots play in turn");
        frames(3); ppm("pb-cart-chain");
        fm1_irq_off(); chain_n = 0; fm1_irq_on();
        while (pu.c_page != 2u) turn(EN_K1, 1);
        while (pu.c_item != CI_SKIN) turn(EN_K2, pu.c_item < CI_SKIN ? 1 : -1);
        { uint16_t bg = pbc[0]; turn(EN_K3, 1); check(pb.skin == 1 && pbc[0] != bg, "SETUP: SKIN changes the colours"); frames(2); tap(B_HOME); frames(3); ppm("pb-skin-pea");
          tap(B_SAVE); turn(EN_K3, -1); check(pb.skin == 0 && pbc[0] == bg, "and back"); tap(B_HOME); }
    }
    {   /* the run */
        uint32_t n;
        tap(B_ARP); while (pu.v_mode != 3u) tap(B_ARP);
        check(layer_now() == LY_MIX, "VISUALS > RUN: the keys play no notes");
        tap(B_ARP); check(pu.r_on == 1 && pu.r_lives == 3, "ARP starts the run");
        frames(4); ppm("pb-run");
        for (n = 0; n < 1500u && pu.r_on == 1u; n++) frame();
        check(pu.r_on == 2 && !pu.r_lives && trk[2].p[P_MUTE] && trk[1].p[P_MUTE] && trk[0].p[P_MUTE], "untouched: three misses, the band asleep one by one, game over");
        frames(2); ppm("pb-run-over");
        fm1_in.notes = 1u << 20; frame(); fm1_in.notes = 0; frame();
        check(pu.r_on == 1 && pu.r_lives == 3 && !trk[0].p[P_MUTE], "any key: again, everyone awake");
        for (n = 0; n < 900u && pu.r_on == 1u; n++) {         /* a player who reads the track: the right key just before each step */
            uint32_t p = rn_pos(), s = (p >> 8) + 1u, k = rn_kind(s);
            static uint32_t done;
            if (k && done != s && s * 256u - p < 200u) { done = s; fm1_in.notes = k == 1u ? 1u << 20 : 1u << 2; frame(); fm1_in.notes = 0; }
            frame();
        }
        printf("pb: run: score %u, combo %u, lives %u\n", pu.r_score, pu.r_combo, pu.r_lives);
        check(pu.r_on == 1 && pu.r_lives == 3 && pu.r_score > 200, "played right: no miss, the score climbs");
        frames(2); ppm("pb-run-play");
        tap(B_HOME); check(layer_now() == LY_PLAY && !pu.r_on, "leaving: the keys play again");
    }
    {   /* the sketch remembered: stop, leave it alone, switch off and on */
        pb_song_t s0; uint8_t idea0[4]; uint32_t e0;
        sel(0); turn(EN_K1, 1); turn(EN_K2, 1);
        s0 = pb.s; memcpy(idea0, pb.idea, 4); e0 = pb.energy; nor_erases = 0;
        press(B_PLAY); release(B_PLAY); frames(8);
        check(!song.playing, "stopped");
        frames(260);
        check(nor_erases == 1, "left alone a few seconds, stopped: the sketch is written to the flash, once");
        frames(400); check(nor_erases == 1, "nothing changed: nothing written again");
        memset(&pb.s, 0, sizeof pb.s); memset(pb.idea, 0, 4); pb.have = 0; pb.energy = 12; pb.skin = 3;
        pb_store_boot();
        check(pb.have && !memcmp(&pb.s, &s0, sizeof s0) && !memcmp(pb.idea, idea0, 4) && pb.energy == e0 && pb.skin == 0 && pb.fit[0] >= 40, "power on: the sketch, the ideas, the energy and the skin are back");
        { step_t keep[NSTEP]; memcpy(keep, trk[0].step, sizeof keep); turn(EN_K1, 1); turn(EN_K1, -1);
          check(!memcmp(keep, trk[0].step, sizeof keep), "and the IDEA knob goes on from where it was"); }
        pu.j_tour = 16; pb_void(); frames(4); press(B_PLAY); release(B_PLAY); frames(700);
        { uint8_t src0[4]; memcpy(src0, pb.s.src, 4); memset(&pb.s, 0, sizeof pb.s); pb.have = 0; pu.j_tour = 0;
          pb_store_boot(); check(pb.have == 2 && !memcmp(src0, pb.s.src, 4) && !memcmp(pb.entry, "??????", 6) && pu.j_tour == 16, "the void and the tour are remembered too"); pu.j_tour = 0; }
        nor[0xF9000 + 300] ^= 0x55; nor[0xFA000 + 300] ^= 0x55; pb.have = 0;
        pb_store_boot(); check(!pb.have, "a damaged record is not taken");
        pb_travel(0, 0, 0, 5u); frames(4);
    }

    {   /* 0.4: the tour, the void, the words */
        pb_song_t s0; uint8_t scr0; uint32_t moved = 0;
        if (!song.playing) { press(B_PLAY); release(B_PLAY); frames(6); }
        tap(B_LFO); turn(EN_K4, 1); turn(EN_K4, 1);
        check(pu.j_tour == 16, "JAM: KNOB 4 TOUR, every 16 bars");
        s0 = pb.s; scr0 = pb.scr;
        for (i = 0; i < 3000u && !moved; i++) { frame(); moved = pb.s.seed != s0.seed; }
        check(moved && pb.scr == scr0 && song.playing && on_scale() && pb.have == 1, "on tour: the band moves on to a new sketch by itself, the screen stays");
        turn(EN_K4, -1); turn(EN_K4, -1); check(!pu.j_tour, "TOUR off");
        tap(B_GLO); pb.mx = 232; pb.my = 168; frames(12); ppm("pb-map-void");
        tap(B_GLO); frames(4);
        check(pb.have == 2 && !memcmp(pb.entry, "??????", 6) && on_scale() && steps_used(0) && steps_used(TRK_DRUM), "off the map's corner: the void, a sketch with no code");
        check(!trk[0].p[P_MUTE] && !trk[1].p[P_MUTE] && !trk[2].p[P_MUTE] && !TDRUM->p[P_MUTE], "in the void everyone is awake");
        tap(B_SAVE); while (pu.c_page != 0u) turn(EN_K1, 1); frames(2); ppm("pb-cart-void");
        { int16_t b0 = song.g[G_BPM]; memcpy(pb.entry, "FASTER", 7); tap(B_SAVE); check(song.g[G_BPM] > b0 && pb.have == 2, "the word FASTER: faster, the sketch stays");
          memcpy(pb.entry, "SLEEPY", 7); tap(B_SAVE); check(song.g[G_BPM] < b0 * 5 / 4 && pb.energy == 4, "SLEEPY: slower and quieter"); }
        { uint8_t sk = pb.skin; memcpy(pb.entry, "PARTY2", 7); tap(B_SAVE); frames(500);
          check(pu.party && pb.energy == 20 && pb.skin != sk, "PARTY2: full energy, the colours change with the bars");
          tap(B_SAVE); frames(2); check(!pu.party, "PARTY2 again: over"); pb.skin = 0; pb_skin_apply(); }
        pb_travel(1, 1, 0, 9u); frames(2); tap(B_SAVE);
        memcpy(pb.entry, "SECRET", 7); tap(B_SAVE); check(pb.have == 2, "the word SECRET: the void");
        tap(B_HOME);
    }

    /* every town, pure and blended: in scale, audible, not clipping */
    for (g = 0; g < PB_GENRES; g++) {
        uint32_t blend = g & 1u ? 4u : 0u;
        transport_req = 2; frames(4);
        pb_travel(g, (g + 5u) % PB_GENRES, blend, 4242u + g * 131u);
        frames(2);
        snprintf(name, sizeof name, "%s/pb-genre-%02u.wav", outdir, g);
        wav = (g < 3u || getenv("PB_ALL_WAV")) ? fopen(name, "wb") : 0;
        if (wav) wav_hdr(wav, 8u * FS / (22u * CTL) * (22u * CTL));
        meter0(); frames(8u * FS / (22u * CTL));
        if (wav) { fclose(wav); wav = 0; }
        printf("pb: %-12s%s %s %3u bpm  steps b%2u k%2u l%2u d%2u  rms %5.0f  clip %u\n", pb_genre_name(g), blend ? "+" : " ", pb.entry, pb.s.bpm,
               steps_used(0), steps_used(1), steps_used(2), steps_used(TRK_DRUM), sqrt((double)wav_energy / wav_frames), wav_clip);
        if (!on_scale()) { printf("pb: %s: a note off the scale\n", pb_genre_name(g)); fails++; }
        if (wav_energy / wav_frames < 100 * 100) { printf("pb: %s: silent\n", pb_genre_name(g)); fails++; }
        if (wav_clip > wav_frames / 100u) { printf("pb: %s: clips\n", pb_genre_name(g)); fails++; }
    }
    check(!fails, "all 14 towns: in scale, audible, no clipping");

    {   /* fuzz: every button, knob and key at random; lcd_blit asserts every draw stays on the screen */
        uint32_t r = 2463534242u, n;
        for (n = 0; n < 12000u; n++) {
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            switch ((r >> 27) & 7u) {
            case 0: case 1: case 2: encs[(r >> 8) % 7u] = (int32_t)((r >> 12) % 7u) - 3; break;
            case 3: case 4: { uint32_t b = (r >> 8) % NB; if (b == B_SAVE && pb.scr == PS_CART && pu.c_page == 2 && pu.c_item == CI_CAL) break;
                              edges_btn |= BT(b); fm1_in.buttons |= BT(b); frame(); fm1_in.buttons &= ~BT(b); break; }
            case 5: fm1_in.notes ^= 1u << ((r >> 8) % 27u); break;
            default: break;
            }
            frame();
        }
        fm1_in.notes = 0; frames(4);
        check(1, "12000 frames of random buttons, knobs and keys: every draw on the screen");
    }
    printf(fails ? "pocket band firmware: %d FAILED\n" : "pocket band firmware: all passed\n", fails);
    return fails != 0;
}
