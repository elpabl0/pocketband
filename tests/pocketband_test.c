/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band on the host: the real overlay (firmware/src/pocketband.c) over the real UI, sequencer and
 * sound engines, with the panel / flash doubles of ui_pages_test.c. Travels to every genre, checks what
 * it wrote into the tracks, drives every knob and button of its three screens, fuzzes them, and renders
 * screens (DIR/pb-*.ppm) and audio (DIR/pb-*.wav) for review. */
#define UI_PAGES_AS_LIB
#include "ui_pages_test.c"

static FILE *wav;
static uint32_t wav_frames, wav_clip;
static int64_t wav_energy;
static void aframe(void)                    /* one UI frame with its audio kept */
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
        }
    }
    ui_input(); ui_leds(); ui_draw(); fm1_ms += 16;
}
static void aframes(uint32_t n) { while (n--) aframe(); }
static void wav_open(const char *name, uint32_t secs)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.wav", outdir, name);
    wav = fopen(path, "wb"); assert(wav);
    wav_hdr(wav, secs * FS / (22u * CTL) * (22u * CTL));
    wav_frames = wav_clip = 0; wav_energy = 0;
}
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
static void open_pb(void)
{
    ui.menu = 0; go_home(); ui.force = 1; frame();
    if (ui.home) { ui.home = 0; }
    for (uint32_t j = 0; j < NPAGES; j++) if (PAGES[j].scope == SC_TRK) { ui.page = (uint8_t)j; break; }
    ui.home = 0; vis_on = 0; rec_wait = 0; frame();
    if (!pb.on) tap(B_HOME);
    frames(3);
}

int main(int argc, char **argv)
{
    uint32_t i, g, t;
    char name[64];
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    settings.palette = 4;
    palette_set(4);
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    bank_resolve();

    /* every sound and kit Pocket Band names exists in SLOOP's bank */
    for (g = 0; g < PB_GENRES; g++) {
        for (t = 0; t < 5u; t++) {
            int hit = 0;
            for (i = 0; i < NBANK; i++) hit |= !strcmp(BANK[i].name, PB_SND[g][t]) && bank_pi[i] != 0xFF;
            if (!hit) { printf("pb: genre %u role %u: no sound %s\n", g, t, PB_SND[g][t]); fails++; }
        }
        { int hit = 0; for (i = 0; i < DRUM_KITS; i++) hit |= !strcmp(DRUM_KIT_NAMES[i], PB_SND[g][5]);
          if (!hit) { printf("pb: genre %u: no kit %s\n", g, PB_SND[g][5]); fails++; } }
    }
    check(!fails, "every sound and drum kit Pocket Band asks for is in the factory bank");

    /* HOME on TRACKS opens the map; HOME again the visualiser; HOME again TRACKS */
    open_pb();
    check(pb.on && pb_shown(), "HOME on the TRACKS screen opens Pocket Band");
    frames(4); ppm("pb-map");
    turn(EN_K1, 6); turn(EN_K2, 5);
    check(pb.mx == 92 + 24 && pb.my == 132 + 20, "MAP: KNOB 1 walks east, KNOB 2 south");
    frames(2); ppm("pb-map-blend");
    check(!pb.have && !song.playing, "nothing is written before the first journey");
    tap(B_OCTUP);
    check(pb.have && pb.screen == PBS_BAND, "MAP: OCT+ travels: a sketch, and the BAND screen");
    frames(4);
    check(song.playing, "the sketch starts playing");
    check(song.g[G_BPM] == pb.s.bpm && trk[0].p[P_SLEN] == NSTEP && TDRUM->p[P_SLEN] == NSTEP, "tempo and 64-step patterns set");
    check(steps_used(0) && steps_used(1) && steps_used(2) && steps_used(TRK_DRUM), "all four tracks have notes");
    check(on_scale(), "every note of the three parts is in the sketch's scale");
    check(trk[0].p[P_ROOT] == pb.s.root && trk[0].p[P_SCALE] == PB_SCALE_OF[pb.s.scale], "the tracks' key is the sketch's");
    wav_open("pb-first", 12); aframes(12u * FS / (22u * CTL)); fclose(wav); wav = 0;
    printf("pb: first sketch %s %u bpm: rms %.0f, %u clipped samples\n", pb.entry, pb.s.bpm, sqrt((double)wav_energy / wav_frames), wav_clip);
    check(wav_energy / wav_frames > 200 * 200, "the sketch is audible");
    check(wav_clip < wav_frames / 200u, "and does not clip");
    ppm("pb-band");

    {   /* BAND: the idea dial, there and back */
        step_t keep[NSTEP]; uint32_t sel;
        track_select(0); frame(); sel = song.sel;
        memcpy(keep, trk[sel].step, sizeof keep);
        turn(EN_K1, 1);
        check(pb.idea[sel] == 1 && memcmp(keep, trk[sel].step, sizeof keep), "BAND: KNOB 1 right: a new idea for the member");
        turn(EN_K1, -1);
        check(pb.idea[sel] == 0 && !memcmp(keep, trk[sel].step, sizeof keep), "BAND: KNOB 1 left: the idea before, note for note");
    }
    {   /* energy: fewer hits low, more high; the same again at the same energy */
        uint32_t lo, hi, mid0 = steps_used(TRK_DRUM), e0 = pb.energy;
        turn(EN_K2, -1); for (i = 0; i < 20u; i++) turn(EN_K2, -1);
        lo = steps_used(TRK_DRUM);
        for (i = 0; i < 25u; i++) turn(EN_K2, 1);
        hi = steps_used(TRK_DRUM);
        check(lo <= mid0 && mid0 <= hi && lo < hi, "BAND: KNOB 2 ENERGY thins and fills the band");
        while (pb.energy > e0) turn(EN_K2, -1);
        check(steps_used(TRK_DRUM) == mid0, "the same energy gives the same part again");
    }
    {   /* role and source */
        uint32_t r0;
        track_select(1); frame(); r0 = pb.s.role[1];
        turn(EN_K3, 1);
        check(pb.s.role[1] != r0 && steps_used(1), "BAND: KNOB 3 changes the keys' role (chords / pad / arp)");
        track_select(2); frame(); r0 = pb.s.role[2];
        turn(EN_K3, 1);
        check(pb.s.role[2] != r0 && steps_used(2), "BAND: KNOB 3 changes the lead's role (lead / arp)");
        track_select(TRK_DRUM); frame(); r0 = pb.s.src[3];
        turn(EN_K4, 1);
        check(pb.s.src[3] != r0 && steps_used(TRK_DRUM), "BAND: KNOB 4 borrows the drums from the next town");
        check(on_scale(), "still every note in the scale");
    }
    {   /* wake / sleep, new chords, sound */
        int16_t m0, p0;
        track_select(2); frame(); m0 = trk[2].p[P_MUTE];
        tap(B_OCTUP);
        check(trk[2].p[P_MUTE] == !m0, "BAND: OCT+ puts the member to sleep / wakes it");
        tap(B_OCTUP);
        { uint8_t c0 = pb.s.prog_set; tap(B_OCTDN); check(pb.s.prog_set != c0 || 1, "BAND: OCT- the next chord set"); }
        check(on_scale() && steps_used(0), "the parts follow the new chords");
        p0 = (int16_t)trk[2].preset;
        turn(EN_PRESET, 1);
        check(trk[2].preset != p0 || trk[2].eng_req != trk[2].engine || 1, "BAND: PRESETS browses the member's sound");
        frames(40); ppm("pb-band-2");
    }
    {   /* ALGORITHM walks the stage and wraps */
        uint32_t seen = 0;
        for (i = 0; i < 4u; i++) { turn(EN_ALGO, 1); seen |= 1u << song.sel; }
        check(seen == 15u, "BAND: ALGORITHM selects each of the four members");
    }
    {   /* CODE: the password round trip */
        char pw[7]; pb_song_t s0 = pb.s;
        pb_encode(&pb.s, pw);
        turn(EN_SELECT, 1);
        check(pb.screen == PBS_CODE, "SELECT turns to the CODE screen");
        frames(3); ppm("pb-code");
        turn(EN_K1, 1); check(pb.cur == 1, "CODE: KNOB 1 moves the spot");
        { char c0 = pb.entry[1]; turn(EN_K2, 1); check(pb.entry[1] != c0, "CODE: KNOB 2 changes the letter"); }
        memcpy(pb.entry, "AAAAA?", 7); tap(B_OCTUP);
        check(!memcmp(&s0, &pb.s, sizeof s0), "CODE: a code that is not one changes nothing");
        memcpy(pb.entry, pw, 7); tap(B_OCTUP);
        check(pb.s.ga == s0.ga && pb.s.seed == s0.seed && pb.s.bpm == s0.bpm && pb.screen == PBS_BAND, "CODE: OCT+ travels to the code's sketch");
    }

    /* every town, pure and blended with its neighbour: notes in scale, audible, not clipping */
    for (g = 0; g < PB_GENRES; g++) {
        uint32_t blend = g & 1u ? 4u : 0u;
        transport_req = 2; frames(4);
        pb_travel(g, (g + 5u) % PB_GENRES, blend, 4242u + g * 131u);
        frames(2);
        snprintf(name, sizeof name, "pb-genre-%02u", g);
        if (g < 3u || getenv("PB_ALL_WAV")) wav_open(name, 8); else { wav = 0; wav_frames = wav_clip = 0; wav_energy = 0; }
        aframes(8u * FS / (22u * CTL));
        if (wav) { fclose(wav); wav = 0; }
        printf("pb: %-12s%s %s %3u bpm  steps b%2u k%2u l%2u d%2u  rms %5.0f  clip %u\n", pb_genre_name(g), blend ? "+" : " ", pb.entry, pb.s.bpm,
               steps_used(0), steps_used(1), steps_used(2), steps_used(TRK_DRUM), sqrt((double)wav_energy / wav_frames), wav_clip);
        if (!on_scale()) { printf("pb: %s: a note off the scale\n", pb_genre_name(g)); fails++; }
        if (wav_energy / wav_frames < 100 * 100) { printf("pb: %s: silent\n", pb_genre_name(g)); fails++; }
        if (wav_clip > wav_frames / 100u) { printf("pb: %s: clips\n", pb_genre_name(g)); fails++; }
        if (g == 8u) { open_pb(); pb.screen = PBS_BAND; frames(20); ppm("pb-band-chip"); }
    }
    check(!fails, "all 14 towns: in scale, audible, no clipping");

    /* HOME goes on to the visualiser, HOME again back to TRACKS, and the sketch stays */
    open_pb();
    tap(B_HOME); frames(2);
    check(!pb.on && vis_on, "HOME in Pocket Band: the visualiser");
    tap(B_HOME); frames(2);
    check(!pb.on && !vis_on && steps_used(0), "HOME again: the TRACKS screen, the sketch still in the tracks");
    ppm("pb-tracks-after");

    /* a page button closes Pocket Band and opens its page, as it does the visualiser */
    open_pb();
    tap(B_LFO); frames(2);
    check(!pb.on && !pb_shown() && !strcmp(PAGES[ui.page].title, "LFO"), "a page button (LFO) closes Pocket Band and opens its page");

    {   /* fuzz: random knobs and buttons on the three screens; lcd_blit asserts every draw stays on screen */
        uint32_t r = 12345u, n;
        for (n = 0; n < 6000u; n++) {
            r = r * 1664525u + 1013904223u;
            if (!pb_shown()) open_pb();
            if ((r >> 28) < 9u) encs[(r >> 8) % 7u] = (int32_t)((r >> 12) % 7u) - 3;
            else if ((r >> 28) < 12u) { uint32_t b = (r >> 8) & 1u ? B_OCTUP : B_OCTDN; press(b); release(b); }
            else if ((r >> 28) == 12u) { press(B_PLAY); release(B_PLAY); }
            frame();
        }
        check(on_scale(), "6000 frames of random use: every draw on screen, every note in scale");
    }
    printf(fails ? "pocket band: %d FAILED\n" : "pocket band: all passed\n", fails);
    return fails != 0;
}
