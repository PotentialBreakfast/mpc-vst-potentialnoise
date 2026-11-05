/* PotentialNoise: a calibrated pink noise reference for mixing (MPC OS VST2 instrument engine, mpc-vst-plugins kit).
 *
 * Pink noise (equal energy per octave, -3 dB/octave) from Paul Kellet's refined filter on near-gaussian white noise.
 * Each range is calibrated at startup by running ~6 s of noise through it and measuring the true RMS, so a level of
 * "-14 dB" means -14 dBFS RMS. READING shows the measured RMS and peak of what is actually sent out.
 *   FULL    : 20 Hz high-pass (removes the subsonic part of the filter's tail)
 *   LIMITED : 40 Hz high-pass and 16 kHz low-pass, like the band-limited pink noise files many engineers use
 * MONO sends the same noise to both channels; STEREO uses two uncorrelated generators.
 * Level and on/off changes ramp over ~20 ms, so there are no clicks. MIT licence. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"

#define SR 44100.0
static const float LEVELS[6] = {-24, -20, -18, -16, -14, -12};

typedef struct { float b0, b1, b2, a1, a2, z1, z2; } bq_t;
typedef struct { uint32_t seed; float p[7]; bq_t hp1, hp2, lp1, lp2; } gen_t;

static void bq_hp(bq_t *f, double fc, double q) {
    double w = 2 * M_PI * fc / SR, c = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
    f->b0 = (float)((1 + c) / 2 / a0); f->b1 = (float)(-(1 + c) / a0); f->b2 = f->b0;
    f->a1 = (float)(-2 * c / a0); f->a2 = (float)((1 - al) / a0); f->z1 = f->z2 = 0;
}
static void bq_lp(bq_t *f, double fc, double q) {
    double w = 2 * M_PI * fc / SR, c = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
    f->b0 = (float)((1 - c) / 2 / a0); f->b1 = (float)((1 - c) / a0); f->b2 = f->b0;
    f->a1 = (float)(-2 * c / a0); f->a2 = (float)((1 - al) / a0); f->z1 = f->z2 = 0;
}
static inline float bq(bq_t *f, float x) {   /* transposed direct form II */
    float y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}
static inline float urand(uint32_t *s) {     /* xorshift32 -> -1..1 */
    uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *s = x;
    return (float)((int32_t)x) * (1.0f / 2147483648.0f);
}
static void gen_init(gen_t *g, uint32_t seed, int range) {
    memset(g, 0, sizeof *g); g->seed = seed ? seed : 1;
    double q1 = 0.5411961, q2 = 1.3065630;   /* 4th-order Butterworth = two biquads */
    if (range == 0) { bq_hp(&g->hp1, 20, q1); bq_hp(&g->hp2, 20, q2); }
    else { bq_hp(&g->hp1, 40, q1); bq_hp(&g->hp2, 40, q2); bq_lp(&g->lp1, 16000, q1); bq_lp(&g->lp2, 16000, q2); }
}
static inline float gen_next(gen_t *g, int range) {
    float w = (urand(&g->seed) + urand(&g->seed) + urand(&g->seed) + urand(&g->seed)) * 0.5f;   /* near-gaussian */
    float *b = g->p;
    b[0] = 0.99886f * b[0] + w * 0.0555179f;
    b[1] = 0.99332f * b[1] + w * 0.0750759f;
    b[2] = 0.96900f * b[2] + w * 0.1538520f;
    b[3] = 0.86650f * b[3] + w * 0.3104856f;
    b[4] = 0.55000f * b[4] + w * 0.5329522f;
    b[5] = -0.7616f * b[5] - w * 0.0168980f;
    float y = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f;
    b[6] = w * 0.115926f;
    y = bq(&g->hp2, bq(&g->hp1, y));
    if (range) y = bq(&g->lp2, bq(&g->lp1, y));
    return y;
}

typedef struct {
    int on, level, width, range;
    float norm[2];          /* gain that makes each range exactly 0 dBFS RMS */
    gen_t g[2];
    float gain, target, ramp_to, ramp_step;   /* output gain, ramped linearly over ~20 ms */
    double acc; int accn;   /* RMS window */
    float win_peak, peak, rms_db;
    int meter, has_rms;
    unsigned rev;
} noise_t;

/* Soft knee above -2 dBFS: the rare noise peaks round off below 0 dBFS instead of clipping (RMS changes < 0.01 dB). */
static inline float soft(float x) {
    float a = fabsf(x);
    if (a <= 0.8f) return x;
    a = 0.8f + 0.2f * tanhf((a - 0.8f) / 0.2f);
    return x < 0 ? -a : a;
}

static float calibrate(int range) {
    gen_t g; gen_init(&g, 12345, range);
    int n = (int)(SR * 6), warm = (int)(SR * 0.5);
    double s = 0;
    for (int i = 0; i < n + warm; i++) { float y = gen_next(&g, range); if (i >= warm) s += (double)y * y; }
    return (float)(1.0 / sqrt(s / n));
}
static void reset_gens(noise_t *e) {
    gen_init(&e->g[0], 0x9E3779B9u, e->range);
    gen_init(&e->g[1], 0x85EBCA6Bu, e->range);
}
static void retarget(noise_t *e) {
    e->target = e->on ? e->norm[e->range] * powf(10.f, LEVELS[e->level] / 20.f) : 0.f;
}

static void *create(const char *dir) {
    (void)dir;
    noise_t *e = calloc(1, sizeof *e);
    if (!e) return NULL;
    e->level = 4; e->norm[0] = calibrate(0); e->norm[1] = calibrate(1);
    e->rms_db = -120; e->peak = 0;
    reset_gens(e); retarget(e);
    return e;
}
static void destroy(void *p) { free(p); }
static void midi(void *p, const uint8_t *m, int n) { (void)p; (void)m; (void)n; }

static void set_param(void *p, const char *k, const char *v) {
    noise_t *e = p; int x = atoi(v);
    if (!strcmp(k, "on")) e->on = x ? 1 : 0;
    else if (!strcmp(k, "level")) e->level = x < 0 ? 0 : x > 5 ? 5 : x;
    else if (!strcmp(k, "width")) e->width = x ? 1 : 0;
    else if (!strcmp(k, "range")) { int r = x ? 1 : 0; if (r != e->range) { e->range = r; reset_gens(e); } }
    else if (!strcmp(k, "clear_peak")) { if (x) { e->peak = 0; e->rev++; } }
    else if (!strcmp(k, "state")) {   /* on=..;level=..;width=..;range=.. */
        const char *s;
        if ((s = strstr(v, "on="))) e->on = atoi(s + 3) ? 1 : 0;
        if ((s = strstr(v, "level="))) { x = atoi(s + 6); e->level = x < 0 ? 0 : x > 5 ? 5 : x; }
        if ((s = strstr(v, "width="))) e->width = atoi(s + 6) ? 1 : 0;
        if ((s = strstr(v, "range="))) { e->range = atoi(s + 6) ? 1 : 0; reset_gens(e); }
    }
    retarget(e);
}

static int get_param(void *p, const char *k, char *b, int n) {
    noise_t *e = p;
    if (!strcmp(k, "on")) return snprintf(b, n, "%d", e->on);
    if (!strcmp(k, "level")) return snprintf(b, n, "%d", e->level);
    if (!strcmp(k, "width")) return snprintf(b, n, "%d", e->width);
    if (!strcmp(k, "range")) return snprintf(b, n, "%d", e->range);
    if (!strcmp(k, "meter")) return snprintf(b, n, "%d", e->meter);
    if (!strcmp(k, "clear_peak")) return snprintf(b, n, "0");
    if (!strcmp(k, "display_rev")) return snprintf(b, n, "%u", e->rev);
    if (!strcmp(k, "state")) return snprintf(b, n, "on=%d;level=%d;width=%d;range=%d", e->on, e->level, e->width, e->range);
    if (!strcmp(k, "reading")) {
        if (!e->on && e->gain < 1e-5f) return snprintf(b, n, "NOISE OFF  -  TAP ON TO START");
        float pk = e->peak > 1e-6f ? 20.f * log10f(e->peak) : -120.f;
        return snprintf(b, n, "RMS %5.1f dB    PEAK %5.1f dB", e->has_rms ? e->rms_db : -99.9f, pk);
    }
    if (!strcmp(k, "tip")) {
        if (!e->on) return snprintf(b, n, "Set a level, tap ON, then solo each track and raise it until it's just audible over the noise.");
        if (e->level == 5) return snprintf(b, n, "-12 dB RMS: peaks can reach 0 dBFS. -14 dB leaves headroom.");
        return snprintf(b, n, "Solo a track and raise its fader until you can only just hear it over the noise.");
    }
    return 0;
}

static void render(void *p, int16_t *out, int frames) {
    noise_t *e = p;
    if (e->target != e->ramp_to) { e->ramp_to = e->target; e->ramp_step = (e->target - e->gain) / (0.02f * (float)SR); }
    for (int i = 0; i < frames; i++) {
        if (e->gain != e->ramp_to) {
            e->gain += e->ramp_step;
            if ((e->ramp_step > 0 && e->gain > e->ramp_to) || (e->ramp_step < 0 && e->gain < e->ramp_to)) e->gain = e->ramp_to;
        }
        float l = 0, r = 0;
        if (e->gain > 0 || e->target > 0) {
            l = gen_next(&e->g[0], e->range) * e->gain;
            r = e->width ? gen_next(&e->g[1], e->range) * e->gain : l;
            l = soft(l); r = soft(r);
        }
        float al = fabsf(l), ar = fabsf(r), a = al > ar ? al : ar;
        if (a > e->win_peak) e->win_peak = a;
        e->acc += 0.5 * ((double)l * l + (double)r * r); e->accn++;
        int il = (int)lrintf(l * 32767.f), ir = (int)lrintf(r * 32767.f);
        out[2 * i] = (int16_t)(il > 32767 ? 32767 : il < -32768 ? -32768 : il);
        out[2 * i + 1] = (int16_t)(ir > 32767 ? 32767 : ir < -32768 ? -32768 : ir);
    }
    if (e->accn >= (int)(SR * 0.3)) {   /* 300 ms window */
        double ms = e->acc / e->accn;
        float db = ms > 1e-12 ? (float)(10 * log10(ms)) : -120.f;
        e->rms_db = db; e->has_rms = ms > 1e-12;
        if (e->win_peak > e->peak) e->peak = e->win_peak;
        int m = (int)floorf(db + 31.f);   /* 1 dB a segment: 1 = -30 dB ... 30 = -1 dB */
        e->meter = m < 0 ? 0 : m > 30 ? 30 : m;
        e->acc = 0; e->accn = 0; e->win_peak = 0;
        e->rev++;
    }
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, NULL, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
