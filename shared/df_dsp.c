/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DigiFilter shared DSP core: firmware-free SVF/comb math + per-voice state.
 *
 * Digitakt 1.1b verbatim (copy-paste, no logic changes). Cached SVF
 * coefficients when still (one digifilter_svf call for the whole block), a
 * linear ramp run in one digifilter_svf4 call when sweeping, precomputed a1
 * (no software division per block), and a damping-free comb fast path.
 * Sound identical, cost lower. No kit/engine/firmware here.
 */
#include "df_dsp.h"
#include "filter_tables.h"
#include "lc_tables.h"
#ifndef SVF_A1_GEN
#include "svf_a1_tables.h"          /* a1 per (resonance, cutoff), from tools/gen_svf_a1.py */
#endif

extern void digifilter_svf(int32 *buf, int frames, const int32 *coef, int32 *state);
extern void digifilter_svf4(int32 *buf, const int32 *coef4, int32 *state);
extern void digifilter_svf_mix(int32 *buf, int frames, const int32 *coef, int32 *state);
extern void digifilter_svf4_mix(int32 *buf, const int32 *coef4, int32 *state);

static int32 dsp_st[8][2];
static int32 comb_buf[8][256];
static int comb_pos[8];
static int comb_init[8];
static int32 comb_dly[8];
static int32 comb_g[8];
static int32 comb_lp[8];
static int svf_fi[8], svf_qi[8], svf_m[8];
static int svf_have[8];
static struct df_coef svf_c[8];

/* low-CPU modes */
static int32 lc_op_st[8][4];        /* 1-pole/4-pole TPT states */
static int lc_comb_init[8];
static int lc_comb_pos[8];

#define COMB_N 256
#define COMB_MASK (COMB_N - 1)
#define COMB_DSHIFT 6
#define COMB_GSHIFT 7

/* floor(2^16 / d) for d = 1..8; ceil() is exact for COMB_DELAY <= 255, so the
 * comb's delay divisor needs no software division. */
static const uint32 COMB_RECIP[9] = {
    0, 65536, 32768, 21846, 16384, 13108, 10923, 9363, 8192
};

/* floor(a * b / 2^sh) for 16 <= sh <= 31, when the result fits 32 bits. */
int32 df_mulsh(int32 a, int32 b, int sh)
{
    uint32 ua = a < 0 ? -(uint32)a : (uint32)a, ub = b < 0 ? -(uint32)b : (uint32)b;
    uint32 al = ua & 0xffff, ah = ua >> 16, bl = ub & 0xffff, bh = ub >> 16;
    uint32 ll = al * bl, lh = al * bh, hl = ah * bl, hh = ah * bh;
    uint32 mid = (ll >> 16) + (lh & 0xffff) + (hl & 0xffff);
    uint32 lo = (ll & 0xffff) | (mid << 16);
    uint32 hi = hh + (lh >> 16) + (hl >> 16) + (mid >> 16);
    if ((a < 0) != (b < 0)) {
        lo = ~lo + 1;
        hi = ~hi + (lo == 0);
    }
    return (int32)((hi << (32 - sh)) | (lo >> sh));
}

/* floor(2^55 / d), d > 2^24. Only tools/gen_svf_a1.py compiles this: the
 * shipped build reads the precomputed SVF_A1 table instead. */
#ifdef SVF_A1_GEN
int32 df_div55(uint32 d)
{
    uint32 rem = 1u << 23, q = 0;
    int i;
    for (i = 0; i < 32; i++) {
        uint32 carry = rem >> 31;
        rem <<= 1;
        q <<= 1;
        if (carry || rem >= d) {
            rem -= d;
            q |= 1;
        }
    }
    return (int32)q;
}
#endif

int df_clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* SVF coefficients and band-pass mix (c0=-1 cancels v0, c1=k, c2=0).
 * Q is capped (top of K27 overshoots); m==1 is BP2 and halves Q. */
void df_mode_coef(int m, int fi, int qi, struct df_coef *c)
{
    int qk = qi * 13 / 15;
    int ek = (m == DF_SVF_BP2) ? (qk >> 1) : qk;  /* K27 row the mode uses */
    int32 g = G27[fi], k = K27[ek];
    if (m == DF_SVF_LP) {
        c->c0 = -ONE27;
        c->c1 = 0;
        c->c2 = ONE27;
    } else if (m == DF_SVF_HP) {
        c->c0 = 0;
        c->c1 = -k;
        c->c2 = -ONE27;
    } else if (m == DF_SVF_NOTCH) {
        c->c0 = 0;
        c->c1 = -k;
        c->c2 = 0;
    } else {                                  /* BP / BP2 */
        c->c0 = -ONE27;
        c->c1 = k;
        c->c2 = 0;
    }
#ifdef SVF_A1_GEN
    c->a1 = df_div55((uint32)((1 << 24) + df_mulsh(g, g + k, 30)));
#else
    /* a1 = floor(2^55 / ((1 << 24) + mulsh(g, g + k, 30))): precomputed for every
     * (ek, fi) by tools/gen_svf_a1.py, so a block needs no software division. */
    c->a1 = SVF_A1[ek][fi];
#endif
    c->a2 = df_mulsh(g, c->a1, 27);
    c->a3 = df_mulsh(g, c->a2, 27);
}

/* Damping-free comb: same math, but without the per-sample damp branch or
 * comb_lp update. Used when the damping knob is at zero, which is common. */
static inline __attribute__((always_inline)) void df_comb_nodamp(int32 *buf, int frames,
                                                  int32 tdq, int32 gt, int32 *b, int *pos,
                                                  int32 *dly, int32 *g)
{
    int p = *pos, i;
    int32 d = *dly, gg = *g;
    for (i = 0; i < frames; i++) {
        int32 ds, ip, fp, i0, i1, x, val, y, fa, da, ga, va;
        d += (tdq - d) >> COMB_DSHIFT;
        gg += (gt - gg) >> COMB_GSHIFT;
        ds = d;
        ip = ds >> 16;
        fp = ds & 0xffff;
        i0 = (p - ip) & COMB_MASK;
        i1 = (i0 - 1) & COMB_MASK;
        x = buf[i];
        fa = fp >> 1;
        da = (b[i1] - b[i0]) >> 13;
        val = b[i0] + ((fa * da) >> 2);
        ga = gg >> 12;
        va = val >> 11;
        y = x + ((ga * va) >> 4);
        if (y > (1 << 27))
            y = 1 << 27;
        else if (y < -(1 << 27))
            y = -(1 << 27);
        b[p] = y;
        p = (p + 1) & COMB_MASK;
        buf[i] = y;
    }
    *pos = p;
    *dly = d;
    *g = gg;
}

/* Feedback comb, fractional and smoothed: y = x + g*y[n-D].
 * D = COMB_DELAY[fi] / (div * hdiv), clamped 3..255; g from fb 0..127.
 * Delay and feedback glide per sample; output saturates at +-2^27. */
void df_comb_run(int32 *buf, int frames, int fi, int fb, int v, int div,
                 int hdiv, int damp)
{
    int32 target = (int32)(((uint32)COMB_DELAY[fi] * COMB_RECIP[div * hdiv]) >> 16);
    int32 tdq, gt, *b = comb_buf[v];
    int p, i;
    if (target < 3)
        target = 3;
    if (target > COMB_MASK)
        target = COMB_MASK;
    tdq = target << 16;
    gt = (int32)fb * ((ONE27 / 32) * 31 / 127);
    if (!comb_init[v]) {
        comb_init[v] = 1;
        comb_dly[v] = tdq;
        comb_g[v] = gt;
        comb_lp[v] = 0;
        comb_pos[v] = 0;
        for (i = 0; i < COMB_N; i++)
            b[i] = 0;
    }
    if (!damp) {
        df_comb_nodamp(buf, frames, tdq, gt, b, &comb_pos[v], &comb_dly[v], &comb_g[v]);
        return;
    }
    p = comb_pos[v];
    for (i = 0; i < frames; i++) {
        int32 ds, ip, fp, i0, i1, x, val, y, fa, da, ga, va;
        comb_dly[v] += (tdq - comb_dly[v]) >> COMB_DSHIFT;
        comb_g[v] += (gt - comb_g[v]) >> COMB_GSHIFT;
        ds = comb_dly[v];
        ip = ds >> 16;
        fp = ds & 0xffff;
        i0 = (p - ip) & COMB_MASK;
        i1 = (i0 - 1) & COMB_MASK;
        x = buf[i];
        fa = fp >> 1;
        da = (b[i1] - b[i0]) >> 13;
        val = b[i0] + ((fa * da) >> 2);
        if (damp) {
            comb_lp[v] += (val - comb_lp[v]) >> 4;
            val += df_mulsh(comb_lp[v] - val, damp, 15);
        }
        ga = comb_g[v] >> 12;
        va = val >> 11;
        y = x + ((ga * va) >> 4);
        if (y > (1 << 27))
            y = 1 << 27;
        else if (y < -(1 << 27))
            y = -(1 << 27);
        b[p] = y;
        p = (p + 1) & COMB_MASK;
        buf[i] = y;
    }
    comb_pos[v] = p;
}

/* SVF runner: cached coefficients when still, 4x ramp when sweeping. */
void df_svf_run(int m, int fi, int qi, int32 *buf, int v)
{
    struct df_coef c0, c1;
    int k, have = svf_have[v];
    if (have && svf_fi[v] == fi && svf_qi[v] == qi && svf_m[v] == m) {
        if (m >= DF_SVF_LP)
            digifilter_svf_mix(buf, 32, (const int32 *)&svf_c[v], dsp_st[v]);
        else
            digifilter_svf(buf, 32, (const int32 *)&svf_c[v], dsp_st[v]);
        return;
    }
    df_mode_coef(m, fi, qi, &c1);
    if (have)
        c0 = svf_c[v];
    else
        c0 = c1;
    svf_c[v] = c1;
    svf_fi[v] = fi;
    svf_qi[v] = qi;
    svf_m[v] = m;
    svf_have[v] = 1;
    if (!have) {
        if (m >= DF_SVF_LP)
            digifilter_svf_mix(buf, 32, (const int32 *)&svf_c[v], dsp_st[v]);
        else
            digifilter_svf(buf, 32, (const int32 *)&svf_c[v], dsp_st[v]);
        return;
    }
    {
        /* Build the four sub-block sets and run them in one call: the prologue/
         * epilogue and the EMAC save/restore happen once, not four times. */
        struct df_coef c4[4];
        for (k = 0; k < 4; k++) {
            int w = k + 1;                           /* 1..4, ends exactly at c1 */
            int32 *d = (int32 *)&c0, *e = (int32 *)&c1;
            int32 *o = (int32 *)&c4[k];
            int j;
            for (j = 0; j < 6; j++) {
                /* Lerp d..e by w/4 without overflowing on a large jump: split the
                 * delta so no intermediate exceeds 32 bits. */
                int32 delta = e[j] - d[j];
                o[j] = d[j] + (delta >> 2) * w + (((delta & 3) * w) >> 2);
            }
        }
        if (m >= DF_SVF_LP)
            digifilter_svf4_mix(buf, (const int32 *)c4, dsp_st[v]);
        else
            digifilter_svf4(buf, (const int32 *)c4, dsp_st[v]);
    }
}
/* ---- low-CPU modes ---------------------------------------------------- */

#ifdef __mcoldfire__
static inline __attribute__((always_inline)) int32 lc_fmul(int32 a, int32 b)
{
    int32 r;
    __asm__ volatile("mac.l %1,%2,%%acc0\n\tmovclr.l %%acc0,%0" : "=d"(r) : "r"(a), "r"(b));
    return r;
}
#else
static inline int32 lc_fmul(int32 a, int32 b)
{
    return (int32)(((long long)a * b) >> 31);
}
#endif

/* 1-pole TPT LP/HP: v = (x - s) G, y = v + s, s = y + v. */
static void lc_onepole_lp(int fi, int32 *buf, int v)
{
    int32 G = LC_OP_G[fi], s = lc_op_st[v][0];
    int i;
    for (i = 0; i < 32; i++) {
        int32 x = buf[i], t = lc_fmul(x - s, G), y = t + s;
        s = y + t;
        buf[i] = y;
    }
    lc_op_st[v][0] = s;
}

static void lc_onepole_hp(int fi, int32 *buf, int v)
{
    int32 G = LC_OP_G[fi], s = lc_op_st[v][0];
    int i;
    for (i = 0; i < 32; i++) {
        int32 x = buf[i], t = lc_fmul(x - s, G), y = t + s;
        s = y + t;
        buf[i] = x - y;
    }
    lc_op_st[v][0] = s;
}

/* Four TPT one-poles, same cutoff: 24 dB/oct, no resonance. */
static void lc_onepole4_lp(int fi, int32 *buf, int v)
{
    int32 G = LC_OP_G[fi];
    int32 s0 = lc_op_st[v][0], s1 = lc_op_st[v][1];
    int32 s2 = lc_op_st[v][2], s3 = lc_op_st[v][3];
    int i;
    for (i = 0; i < 32; i++) {
        int32 x = buf[i], t, y;
        t = lc_fmul(x - s0, G); y = t + s0; s0 = y + t; x = y;
        t = lc_fmul(x - s1, G); y = t + s1; s1 = y + t; x = y;
        t = lc_fmul(x - s2, G); y = t + s2; s2 = y + t; x = y;
        t = lc_fmul(x - s3, G); y = t + s3; s3 = y + t;
        buf[i] = y;
    }
    lc_op_st[v][0] = s0; lc_op_st[v][1] = s1;
    lc_op_st[v][2] = s2; lc_op_st[v][3] = s3;
}

static void lc_onepole4_hp(int fi, int32 *buf, int v)
{
    int32 G = LC_OP_G[fi];
    int32 s0 = lc_op_st[v][0], s1 = lc_op_st[v][1];
    int32 s2 = lc_op_st[v][2], s3 = lc_op_st[v][3];
    int i;
    for (i = 0; i < 32; i++) {
        int32 x = buf[i], t, y;
        t = lc_fmul(x - s0, G); y = t + s0; s0 = y + t; x -= y;
        t = lc_fmul(x - s1, G); y = t + s1; s1 = y + t; x -= y;
        t = lc_fmul(x - s2, G); y = t + s2; s2 = y + t; x -= y;
        t = lc_fmul(x - s3, G); y = t + s3; s3 = y + t; x -= y;
        buf[i] = x;
    }
    lc_op_st[v][0] = s0; lc_op_st[v][1] = s1;
    lc_op_st[v][2] = s2; lc_op_st[v][3] = s3;
}
/* Lofi comb: integer delay, block-rate gain, no fractional read or damping. */
static void lc_comb(int32 *buf, int fi, int fb, int v, int div)
{
    int32 *b = comb_buf[v];
    int target = COMB_DELAY[fi] / div;
    int32 ga, p;
    int i;
    if (target < 3)
        target = 3;
    if (target > COMB_MASK)
        target = COMB_MASK;
    ga = ((int32)fb * ((ONE27 / 32) * 31 / 127)) >> 12;
    if (!lc_comb_init[v]) {
        lc_comb_init[v] = 1;
        lc_comb_pos[v] = 0;
        for (i = 0; i < COMB_N; i++)
            b[i] = 0;
    }
    p = lc_comb_pos[v];
    for (i = 0; i < 32; i++) {
        int32 d = b[(p - target) & COMB_MASK];
        int32 y = buf[i] + ((ga * (d >> 11)) >> 4);
        if (y > (1 << 27))
            y = 1 << 27;
        else if (y < -(1 << 27))
            y = -(1 << 27);
        b[p] = y;
        p = (p + 1) & COMB_MASK;
        buf[i] = y;
    }
    lc_comb_pos[v] = p;
}

void df_lc_run(int mode, int fi, int qi, int32 *buf, int v)
{
    if (fi < 0)
        fi = 0;
    else if (fi > 127)
        fi = 127;
    if (mode == DF_LC_LP2 || mode == DF_LC_HP2 || mode == DF_LC_NOTCH) {
        int m = mode == DF_LC_LP2 ? DF_SVF_LP :
                mode == DF_LC_HP2 ? DF_SVF_HP : DF_SVF_NOTCH;
        df_svf_run(m, fi, qi, buf, v);
        return;
    }
#ifdef __mcoldfire__
    {
        int32 macsr;
        __asm__ volatile("move.l %%macsr,%0\n\tmove.l #0x30,%%d0\n\tmove.l %%d0,%%macsr"
                         : "=d"(macsr) :: "d0");
        if (mode == DF_LC_LP1)
            lc_onepole_lp(fi, buf, v);
        else if (mode == DF_LC_HP1)
            lc_onepole_hp(fi, buf, v);
        else if (mode == DF_LC_LP4)
            lc_onepole4_lp(fi, buf, v);
        else if (mode == DF_LC_HP4)
            lc_onepole4_hp(fi, buf, v);
        else if (mode == DF_LC_COMB)
            lc_comb(buf, fi, qi, v, 1);
        else if (mode == DF_LC_TRASH)
            lc_comb(buf, fi, qi, v, 2);
        __asm__ volatile("move.l %0,%%macsr" :: "d"(macsr));
    }
#else
    if (mode == DF_LC_LP1)
        lc_onepole_lp(fi, buf, v);
    else if (mode == DF_LC_HP1)
        lc_onepole_hp(fi, buf, v);
    else if (mode == DF_LC_LP4)
        lc_onepole4_lp(fi, buf, v);
    else if (mode == DF_LC_HP4)
        lc_onepole4_hp(fi, buf, v);
    else if (mode == DF_LC_COMB)
        lc_comb(buf, fi, qi, v, 1);
    else if (mode == DF_LC_TRASH)
        lc_comb(buf, fi, qi, v, 2);
#endif
}
void df_reset(int v)
{
    if (v < 0 || v >= 8)
        return;
    comb_init[v] = 0;
    svf_have[v] = 0;
    dsp_st[v][0] = 0;
    dsp_st[v][1] = 0;
    lc_op_st[v][0] = lc_op_st[v][1] = lc_op_st[v][2] = lc_op_st[v][3] = 0;
    lc_comb_init[v] = 0;
}
