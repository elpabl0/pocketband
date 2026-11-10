/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the music brain: determinism, ranges, scale membership, passwords. */
#include "pb_brain.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)
int main(int argc, char **argv)
{
    (void)argv;
    static const int LO[PB_ROLES] = {24, 50, 45, 50, 58}, HI[PB_ROLES] = {56, 68, 82, 92, 88};
    uint32_t g, e, i, k, r, show = argc > 1;
    for (g = 0; g < PB_GENRES; g++) for (e = 0; e < 3; e++) {
        pb_song_t s, s2; pb_ev_t a[PB_STEPS], b[PB_STEPS], part[3][PB_STEPS]; pb_dstep_t d[PB_STEPS], d2[PB_STEPS];
        char pw[7]; uint32_t ga, gb, bl, sd, energy = e == 0 ? 60 : e == 1 ? 153 : 255, dn = 0, cnt[3] = {0, 0, 0};
        memset(&s, 0, sizeof s); memset(&s2, 0, sizeof s2);
        pb_make_song(&s, g, (g + 3) % PB_GENRES, g % 8, 1234 + g * 77);
        pb_encode(&s, pw);
        CHECK(pb_decode(pw, &ga, &gb, &bl, &sd), "decode %s", pw);
        pb_make_song(&s2, ga, gb, bl, sd);
        CHECK(!memcmp(&s, &s2, sizeof s), "password round trip %s", pw);
        CHECK(s.bpm >= 60 && s.bpm <= 180 && s.swing <= 33 && s.root < 12 && s.scale < PB_SCALES, "song ranges %s", pw);
        pb_gen_drums(&s, s.src[3], s.tseed[3], energy, d); pb_gen_drums(&s, s.src[3], s.tseed[3], energy, d2);
        CHECK(!memcmp(d, d2, sizeof d), "drums deterministic");
        for (i = 0; i < PB_STEPS; i++) if (d[i].on) dn++;
        for (r = 0; r < 3; r++) {
            pb_gen_part(&s, s.role[r], s.src[r], s.tseed[r], energy, a); pb_gen_part(&s, s.role[r], s.src[r], s.tseed[r], energy, b);
            CHECK(!memcmp(a, b, sizeof a), "part %u deterministic", r);
            memcpy(part[r], a, sizeof a);
            for (i = 0; i < PB_STEPS; i++) {
                if (a[i].n) cnt[r]++;
                CHECK(a[i].n <= 4 && a[i].gate >= 1 && a[i].gate <= 16, "step shape g%u r%u s%u", g, r, i);
                for (k = 0; k < a[i].n; k++) {
                    CHECK(a[i].note[k] >= LO[s.role[r]] && a[i].note[k] <= HI[s.role[r]], "range g%u role%u note %u", g, s.role[r], a[i].note[k]);
                    CHECK(pb_snap(&s, a[i].note[k]) == a[i].note[k], "off scale g%u role%u step%u note %u", g, s.role[r], i, a[i].note[k]);
                }
            }
            CHECK(cnt[r] > 0, "empty part g%u r%u e%u", g, r, energy);
        }
        if (e == 1) {
            uint32_t f = pb_fit(&s, s.role[0], part[0], d, part[1], part[2]);
            CHECK(f >= 40 && f <= 99, "fit");
            printf("%-12s %s %3u bpm sw%2u root%2u sc%u prog %u%u%u%u roles %u%u%u awake %x  hits d%2u b%2u k%2u l%2u fit %u\n", pb_genre_name(g), pw, s.bpm, s.swing,
                   s.root, s.scale, s.prog[0], s.prog[1], s.prog[2], s.prog[3], s.role[0], s.role[1], s.role[2], s.awake, dn, cnt[0], cnt[1], cnt[2], f);
            if (show && g < 2) {
                for (k = 0; k < 16; k++) { int any = 0; for (i = 0; i < 64; i++) any |= (d[i].on >> k) & 1; if (!any) continue;
                    printf("  lane%2u ", k); for (i = 0; i < 64; i++) putchar((d[i].on >> k) & 1 ? ((d[i].ghost >> k) & 1 ? 'o' : 'X') : (i % 4 ? '.' : ':')); putchar('\n'); }
                for (r = 0; r < 3; r++) { printf("  part%u  ", r); for (i = 0; i < 64; i++) putchar(part[r][i].n ? "0123456789ab"[part[r][i].note[0] % 12] : (i % 4 ? '.' : ':')); putchar('\n'); }
            }
        }
        if (e == 0) { pb_dstep_t dh[PB_STEPS]; uint32_t hn = 0; pb_gen_drums(&s, s.src[3], s.tseed[3], 255, dh);
            for (i = 0; i < PB_STEPS; i++) { if (dh[i].on) hn++; CHECK((d[i].on & ~dh[i].on) == 0 || i >= 60, "energy only adds hits g%u s%u", g, i); }
            CHECK(hn >= dn, "more energy, more hits"); }
    }
    printf(fails ? "%d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
