/* SPDX-License-Identifier: GPL-3.0-only */
/* Pocket Band: the sketch remembered. The tracks themselves are kept by the working project (project.c's
 * autosave); this keeps what made them: the sketch's numbers, each member's idea, the energy, the morph, the
 * place on the map, the skin, the best run. One small object (storage.c OBJ_POCKET), written as the autosave
 * is: only when stopped, quiet and left alone a moment, and only when something changed. */
#define PB_REC_MAGIC 0x34304250u            /* "PB04" */
typedef struct {
    uint32_t magic;
    pb_song_t s;
    uint8_t idea[NTRK];
    uint8_t have, energy, morph, skin, town, j_rate, tour, rsv;
    int16_t mx, my;
    uint16_t best;
    char entry[8];
} pb_rec_t;
static uint32_t pb_store_sum, pb_store_ms, pb_store_seen;
UI_SMALL static void pb_rec_fill(pb_rec_t *r)
{
    memset(r, 0, sizeof *r);
    r->magic = PB_REC_MAGIC;
    r->s = pb.s;
    memcpy(r->idea, pb.idea, NTRK);
    r->have = pb.have; r->energy = pb.energy; r->morph = pb.morph; r->skin = pb.skin; r->town = pb.town;
    r->j_rate = pu.j_rate; r->tour = pu.j_tour; r->mx = pb.mx; r->my = pb.my; r->best = pu.r_best;
    memcpy(r->entry, pb.entry, 7);
}
UI_SMALL static void pb_store_boot(void)     /* once, after the working project came back */
{
    pb_rec_t r;
    uint32_t t, notes = 0;
#if FELUCCA_FLASH
    if (!flash_ok || st_load(OBJ_POCKET, &r, sizeof r) != (int)sizeof r)
        return;
#else
    return;
#endif
    if (r.magic != PB_REC_MAGIC || r.s.ga >= PB_GENRES || r.s.gb >= PB_GENRES || r.s.scale >= PB_SCALES || r.s.root > 11u ||
        r.energy < 1u || r.energy > 20u || r.morph > 10u || r.town >= PB_GENRES || r.have > 2u)
        return;                              /* (not ours, or damaged: start fresh) */
    for (t = 0; t < NTRK; t++) {
        notes += trk[t].seq_active;
        if (r.idea[t] > 98u || r.s.src[t] >= PB_GENRES || (t < NPART && r.s.role[t] >= PB_ROLES))
            return;
    }
    pb.s = r.s;
    memcpy(pb.idea, r.idea, NTRK);
    pb.energy = r.energy; pb.morph = r.morph; pb.skin = (uint8_t)(r.skin % PB_SKINS); pb.town = r.town;
    pb.mx = (int16_t)clamp(r.mx, 4, 236); pb.my = (int16_t)clamp(r.my, 20, 172);
    pu.j_rate = (uint8_t)(r.j_rate == 8u || r.j_rate == 4u || r.j_rate == 2u ? r.j_rate : 0u);
    pu.r_best = r.best;
    pu.j_tour = (uint8_t)(r.tour == 16u || r.tour == 32u ? r.tour : 0u);
    memcpy(pb.entry, r.entry, 6); pb.entry[6] = 0;
    pb_skin_apply();
    pb.have = (uint8_t)(notes ? r.have : 0u); /* (the sketch without its tracks is no sketch: travel again) */
    if (pb.have) {                           /* the parts as they were generated, for FIT: the tracks are not touched */
        pb_dry = 1;
        pb_rewrite_all();
        pb_dry = 0;
    }
    pb_rec_fill(&r);
    pb_store_sum = st_crc32(&r, sizeof r);
}
UI_SMALL static void pb_store_tick(void)     /* every frame */
{
#if FELUCCA_FLASH
    pb_rec_t r;
    uint32_t now = fm1_ms, sum;
    if (!flash_ok || song.playing || transport_req || rec_wait || now - ui_input_ms < 3000u || now - pb_store_seen < 1000u ||
        (pb_store_ms && now - pb_store_ms < 15000u))
        return;
    pb_store_seen = now;
    pb_rec_fill(&r);
    sum = st_crc32(&r, sizeof r);
    if (sum == pb_store_sum || !audio_quiet())
        return;
    if (st_save(OBJ_POCKET, &r, sizeof r) == 0)
        pb_store_sum = sum;
    pb_store_ms = now ? now : 1u;
#endif
}
