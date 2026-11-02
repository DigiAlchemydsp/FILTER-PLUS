/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DigiFilter shared DSP core: firmware-free SVF/comb math + per-voice state.
 *
 * Digitakt 1.1b verbatim (copy-paste, no logic changes): mulsh/clamping, mode
 * coefficients (Q capped, BP2 halves, precomputed a1), feedback comb
 * (fractional, smoothed, saturated; damping-free fast path) and SVF runner
 * (cached coefficients, single call when still, one svf4 ramp when sweeping).
 * No kit/engine/firmware: callers pass m/fi/qi/fb/hdiv/damp/div + buf/voice.
 * Host tests cover it.
 */
#ifndef DF_DSP_H
#define DF_DSP_H

typedef int int32;
typedef unsigned int uint32;

struct df_coef {
    int32 a1, a2, a3, c0, c1, c2;
};

/* SVF output mixes (the same kernel; c2 is zero for BP/BP2). */
enum { DF_SVF_BP, DF_SVF_BP2, DF_SVF_LP, DF_SVF_HP, DF_SVF_NOTCH };

/* Low-CPU modes: 1-pole/4-pole clean, 2-pole SVF mixes, integer-delay combs. */
enum {
    DF_LC_LP1, DF_LC_HP1, DF_LC_LP4, DF_LC_HP4,
    DF_LC_LP2, DF_LC_HP2, DF_LC_NOTCH,
    DF_LC_COMB, DF_LC_TRASH,
    DF_LC_NMODES
};

int32 df_mulsh(int32 a, int32 b, int sh);
#ifdef SVF_A1_GEN
int32 df_div55(uint32 d);
#endif
int df_clampi(int v, int lo, int hi);
void df_mode_coef(int m, int fi, int qi, struct df_coef *c);
void df_comb_run(int32 *buf, int frames, int fi, int fb, int v, int div,
                 int hdiv, int damp);
void df_svf_run(int m, int fi, int qi, int32 *buf, int v);
void df_lc_run(int mode, int fi, int qi, int32 *buf, int v);
void df_reset(int v);

#endif
