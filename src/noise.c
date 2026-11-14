/* PotentialNoise: a calibrated pink noise reference for mixing (MPC OS VST2 instrument engine, mpc-vst-plugins kit).
 *
 * Pink noise (equal energy per octave, -3 dB/octave) from Paul Kellet's refined filter on near-gaussian white noise.
 * Each range is calibrated at startup by running ~6 s of noise through it and measuring the true RMS, so a level of
 * "-14 dB" means -14 dBFS RMS. READING shows the measured RMS and peak of what is actually sent out.
 *   FULL    : 20 Hz high-pass (removes the subsonic part of the filter's tail)
 *   LIMITED : 40 Hz high-pass and 16 kHz low-pass, like the band-limited pink noise files many engineers use
 * MONO sends the same noise to both channels; STEREO uses two uncorrelated generators.
 * Level and on/off changes ramp over ~20 ms, so there are no clicks.
 * SIGNAL (2.3.0): PINK NOISE (as above), TEST TONE (a sine at 1 kHz, 100, 60 or 40 Hz, its RMS at LEVEL), OCTAVE BAND
 * (pink noise through one octave, 31 Hz to 16 kHz, its RMS at LEVEL) or SPEAKER CHECK (pink noise left, right, both in
 * phase, both out of phase, 2 s each, round and round). Changing signal fades out and back in.
 * GUIDE page (2.2.0): the method one tap per track. NEXT steps through the parts of a beat (drums first, or the main
 * sample first), saying what to solo and what to listen for; the first tap turns the noise on and the last turns it off.
 * MIT licence. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "../shared/looks/potential_theme.h"   /* the LOOK switch (parameter "theme") */
#include "../shared/potential_manual.h"        /* the MANUAL tab's page */

#define SR 44100.0
static const float LEVELS[6] = {-24, -20, -18, -16, -14, -12};
enum { SIG_PINK, SIG_TONE, SIG_BAND, SIG_CHECK };
static const double TONE_HZ[4] = {1000, 100, 60, 40};
#define NBAND 10
static const double BAND_HZ[NBAND] = {31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
static const char *const BAND_NAME[NBAND] = {"31 Hz", "63 Hz", "125 Hz", "250 Hz", "500 Hz", "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"};
#define CHECK_SEC 2.0   /* each speaker-check position */

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
static void bq_bp(bq_t *f, double fc, double q) {   /* band-pass, 0 dB at the centre */
    if (fc > SR * 0.45) fc = SR * 0.45;
    double w = 2 * M_PI * fc / SR, c = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
    f->b0 = (float)(al / a0); f->b1 = 0; f->b2 = (float)(-al / a0);
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

/* ---- the GUIDE: step 0 gets ready, then one step per part, then done ---- */
typedef struct { const char *name, *hint; } part_t;
static const part_t PARTS[] = {
    {"KICK", "Listen for the thump, not the click."},
    {"SNARE / CLAP", "Set it by its crack."},
    {"BASS", "Listen for the notes, not the rumble. Small speakers help here."},
    {"HI-HATS", "They cut through fast: raise the fader slowly."},
    {"PERCUSSION", "Shakers, rims, toms: one at a time."},
    {"MAIN SAMPLE / CHORDS", "The loop or chords the beat is built on."},
    {"MELODY / LEAD", "Leads, hooks, counter-melodies."},
    {"VOCALS", "Set them by the words: raise until you can just make them out."},
    {"PADS / TEXTURE", "Pads, vinyl, ambience: these usually end up lowest."},
    {"FX / EXTRAS", "Risers, drops, ear candy. Then tap NEXT to finish."},
};
#define NPART (int)(sizeof PARTS / sizeof *PARTS)
static const int ORDER[2][NPART] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},   /* DRUMS FIRST */
    {5, 0, 1, 2, 3, 4, 6, 7, 8, 9},   /* SAMPLE FIRST */
};
#define GSTEPS (NPART + 2)   /* 0 = get ready, 1..NPART = parts, NPART + 1 = done */

typedef struct {
    int on, level, width, range;
    int gstep, gorder, pending;   /* GUIDE: the step, the order; pending: 1 noise_on, 2 signal changed, for the host */
    int sig, tone, band;          /* what's picked */
    int a_sig, a_tone, a_band, a_range, a_width;   /* what's playing: a change fades out, switches, fades back in */
    float bnorm[NBAND];           /* gain that makes each octave band 0 dBFS RMS */
    bq_t bp[2][2];                /* the octave band, per channel (two band-passes in a row) */
    double ph;                    /* TEST TONE phase */
    long ct; float cg[2];         /* SPEAKER CHECK: samples into the cycle, smoothed channel gains */
    float norm[2];          /* gain that makes each range exactly 0 dBFS RMS */
    gen_t g[2];
    float gain, target, ramp_to, ramp_step;   /* output gain, ramped linearly over ~20 ms */
    double acc; int accn;   /* RMS window */
    float win_peak, peak, rms_db;
    int meter, has_rms;
    unsigned rev;
    pt_theme_t theme;   /* which look MPC loads next time: not part of the saved state */
    pm_manual_t manual; /* the MANUAL tab's page: not part of the sound or the saved state */
} noise_t;

/* Soft knee above -2 dBFS: the rare noise peaks round off below 0 dBFS instead of clipping (RMS changes < 0.01 dB). */
static inline float soft(float x) {
    float a = fabsf(x);
    if (a <= 0.8f) return x;
    a = 0.8f + 0.2f * tanhf((a - 0.8f) / 0.2f);
    return x < 0 ? -a : a;
}

static void band_init(bq_t *b, int band) {   /* one octave, two band-passes (4th order), Q for an octave */
    bq_bp(&b[0], BAND_HZ[band], 1.414); bq_bp(&b[1], BAND_HZ[band], 1.414);
}
static float calibrate_band(int band) {
    gen_t g; gen_init(&g, 777, 0);
    bq_t b[2]; band_init(b, band);
    int n = (int)(SR * (band < 2 ? 4.0 : 1.5)), warm = (int)(SR * 0.3);
    double s = 0;
    for (int i = 0; i < n + warm; i++) { float y = bq(&b[1], bq(&b[0], gen_next(&g, 0))); if (i >= warm) s += (double)y * y; }
    return (float)(1.0 / sqrt(s / n));
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
    float n = e->a_sig == SIG_TONE ? 1.41421356f : e->a_sig == SIG_BAND ? e->bnorm[e->a_band] : e->norm[e->a_range];
    int changing = e->sig != e->a_sig || e->tone != e->a_tone || e->band != e->a_band || e->range != e->a_range || e->width != e->a_width;
    e->target = e->on && !changing ? n * powf(10.f, LEVELS[e->level] / 20.f) : 0.f;
}
static void activate(noise_t *e) {   /* the picked signal starts playing (from silence) */
    int regen = e->range != e->a_range;
    e->a_sig = e->sig; e->a_tone = e->tone; e->a_band = e->band; e->a_range = e->range; e->a_width = e->width;
    if (regen) reset_gens(e);
    band_init(e->bp[0], e->band); band_init(e->bp[1], e->band);
    e->ph = 0; e->ct = 0; e->cg[0] = e->cg[1] = 0;
    retarget(e);
}

static void *create(const char *dir) {
    noise_t *e = calloc(1, sizeof *e);
    if (!e) return NULL;
    pt_theme_init(&e->theme, dir);
    e->level = 4; e->norm[0] = calibrate(0); e->norm[1] = calibrate(1);
    for (int b = 0; b < NBAND; b++) e->bnorm[b] = calibrate_band(b);
    band_init(e->bp[0], 0); band_init(e->bp[1], 0);
    e->rms_db = -120; e->peak = 0;
    reset_gens(e); retarget(e);
    return e;
}
static void destroy(void *p) { free(p); }
static void midi(void *p, const uint8_t *m, int n) { (void)p; (void)m; (void)n; }

static void set_param(void *p, const char *k, const char *v) {
    noise_t *e = p; int x = atoi(v);
    if (pt_theme_set(&e->theme, k, v)) return;
    if (pm_set(&e->manual, k, v)) return;
    /* "on" (parameter 0) is an unused slot since 2.0.2: MPC's host flicks parameter 0 on STOP and on Overdub while playing,
     * which briefly switched the noise on. ON/OFF is "noise_on" now; saved projects still restore it from the state. */
    if (!strcmp(k, "on")) return;
    if (!strcmp(k, "noise_on")) e->on = x ? 1 : 0;
    else if (!strcmp(k, "level")) e->level = x < 0 ? 0 : x > 5 ? 5 : x;
    else if (!strcmp(k, "width")) e->width = x ? 1 : 0;
    else if (!strcmp(k, "range")) e->range = x ? 1 : 0;
    else if (!strcmp(k, "signal")) e->sig = x < 0 ? 0 : x > 3 ? 3 : x;
    else if (!strcmp(k, "tone")) e->tone = x < 0 ? 0 : x > 3 ? 3 : x;
    else if (!strcmp(k, "band")) e->band = x < 0 ? 0 : x > NBAND - 1 ? NBAND - 1 : x;
    else if (!strcmp(k, "clear_peak")) { if (x) { e->peak = 0; e->rev++; } }
    else if (!strcmp(k, "guide_next") || !strcmp(k, "guide_back")) {
        if (!x) return;
        int fwd = k[6] == 'n', st = e->gstep + (fwd ? 1 : -1);
        if (st < 0) st = 0;
        if (st >= GSTEPS) st = 0;   /* after DONE: start again */
        if (fwd && st == 1 && e->sig != SIG_PINK) { e->sig = SIG_PINK; e->pending |= 2; }   /* the method is pink noise */
        if (fwd && st == 1 && !e->on) { e->on = 1; e->pending |= 1; }            /* GET READY -> the first part: noise on */
        if (fwd && st == GSTEPS - 1 && e->on) { e->on = 0; e->pending |= 1; }    /* the last part -> DONE: noise off */
        e->gstep = st; e->rev++;
    }
    else if (!strcmp(k, "guide_order")) { int o = x ? 1 : 0; if (o != e->gorder) { e->gorder = o; e->gstep = 0; e->rev++; } }
    else if (!strcmp(k, "state")) {   /* on=..;level=..;width=..;range=.. */
        const char *s;
        if ((s = strstr(v, "on="))) e->on = atoi(s + 3) ? 1 : 0;
        if ((s = strstr(v, "level="))) { x = atoi(s + 6); e->level = x < 0 ? 0 : x > 5 ? 5 : x; }
        if ((s = strstr(v, "width="))) e->width = atoi(s + 6) ? 1 : 0;
        if ((s = strstr(v, "range="))) { e->range = atoi(s + 6) ? 1 : 0; }
        e->sig = (s = strstr(v, "sig=")) ? atoi(s + 4) : 0; if (e->sig < 0 || e->sig > 3) e->sig = 0;
        e->tone = (s = strstr(v, "tone=")) ? atoi(s + 5) : 0; if (e->tone < 0 || e->tone > 3) e->tone = 0;
        e->band = (s = strstr(v, "band=")) ? atoi(s + 5) : 0; if (e->band < 0 || e->band >= NBAND) e->band = 0;
        activate(e);
        e->gorder = (s = strstr(v, "gorder=")) ? (atoi(s + 7) ? 1 : 0) : 0;
        e->gstep = 0;   /* the guide starts at the beginning when a project opens */
    }
    retarget(e);
}

static int get_param(void *p, const char *k, char *b, int n) {
    noise_t *e = p;
    { int pt_n = pt_theme_get(&e->theme, k, b, n); if (pt_n) return pt_n; }
    { int pm_n = pm_get(&e->manual, k, b, n); if (pm_n) return pm_n; }
    if (!strcmp(k, "on")) return snprintf(b, n, "0");
    if (!strcmp(k, "noise_on")) return snprintf(b, n, "%d", e->on);
    if (!strcmp(k, "level")) return snprintf(b, n, "%d", e->level);
    if (!strcmp(k, "width")) return snprintf(b, n, "%d", e->width);
    if (!strcmp(k, "range")) return snprintf(b, n, "%d", e->range);
    if (!strcmp(k, "meter")) return snprintf(b, n, "%d", e->meter);
    if (!strcmp(k, "clear_peak")) return snprintf(b, n, "0");
    if (!strcmp(k, "display_rev")) return snprintf(b, n, "%u", e->rev);
    if (!strcmp(k, "state")) return snprintf(b, n, "on=%d;level=%d;width=%d;range=%d;gorder=%d;sig=%d;tone=%d;band=%d", e->on, e->level, e->width,
                                             e->range, e->gorder, e->sig, e->tone, e->band);
    if (!strcmp(k, "signal")) return snprintf(b, n, "%d", e->sig);
    if (!strcmp(k, "tone")) return snprintf(b, n, "%d", e->tone);
    if (!strcmp(k, "band")) return snprintf(b, n, "%d", e->band);
    if (!strcmp(k, "guide_next") || !strcmp(k, "guide_back")) return snprintf(b, n, "0");
    if (!strcmp(k, "guide_order")) return snprintf(b, n, "%d", e->gorder);
    if (!strncmp(k, "guide_", 6)) {   /* the GUIDE page's lines */
        int st = e->gstep, part = st >= 1 && st <= NPART ? ORDER[e->gorder][st - 1] : -1;
        if (!strcmp(k, "guide_step")) {
            if (st == 0) return snprintf(b, n, "GET READY");
            if (st == GSTEPS - 1) return snprintf(b, n, "DONE");
            return snprintf(b, n, "STEP %d OF %d   ·   NOISE %s AT %d dB", st, NPART, e->on ? "ON" : "OFF", (int)LEVELS[e->level]);
        }
        if (!strcmp(k, "guide_title")) {
            if (st == 0) return snprintf(b, n, "PULL EVERY FADER DOWN");
            if (st == GSTEPS - 1) return snprintf(b, n, "THAT'S YOUR ROUGH MIX");
            return snprintf(b, n, "SOLO THE %s", PARTS[part].name);
        }
        if (!strcmp(k, "guide_text1")) {
            if (st == 0) return snprintf(b, n, "Turn every track's fader all the way down (or note where they are).");
            if (st == GSTEPS - 1) return snprintf(b, n, "The noise is off. Unsolo everything and have a listen.");
            return snprintf(b, n, "Raise its fader from silence until you can only just hear it over the noise.");
        }
        if (!strcmp(k, "guide_text2")) {
            if (st == 0) return snprintf(b, n, "Then tap NEXT: the noise comes on and the guide starts with the first part.");
            if (st == GSTEPS - 1) return snprintf(b, n, "Fine-tune by ear from here. Tap NEXT to start again.");
            return snprintf(b, n, "%s  Not in the beat? Tap NEXT.", PARTS[part].hint);
        }
        return 0;
    }
    if (!strcmp(k, "reading")) {
        if (!e->on && e->gain < 1e-5f) return snprintf(b, n, "NOISE OFF  -  TAP ON TO START");
        float pk = e->peak > 1e-6f ? 20.f * log10f(e->peak) : -120.f;
        return snprintf(b, n, "RMS %5.1f dB    PEAK %5.1f dB", e->has_rms ? e->rms_db : -99.9f, pk);
    }
    if (!strcmp(k, "tip")) {
        if (e->on && e->sig == SIG_TONE && e->tone == 0)
            return snprintf(b, n, "1 kHz tone at %d dB RMS: set a track or plugin input to read the same level (gain staging).", (int)LEVELS[e->level]);
        if (e->on && e->sig == SIG_TONE)
            return snprintf(b, n, "%d Hz tone: if you can't hear or feel it, your speakers or headphones don't reach that low.", (int)TONE_HZ[e->tone]);
        if (e->on && e->sig == SIG_BAND)
            return snprintf(b, n, "Only the %s octave: solo tracks and balance what lives in this range against it.", BAND_NAME[e->band]);
        if (e->on && e->sig == SIG_CHECK) {
            static const char *const POS[4] = {
                "NOW: LEFT ONLY - it should come from your left speaker.",
                "NOW: RIGHT ONLY - it should come from your right speaker.",
                "NOW: BOTH, IN PHASE - it should sound like one source in the middle.",
                "NOW: BOTH, OUT OF PHASE - wide and hollow, no centre. Centred instead? A speaker is wired backwards."};
            return snprintf(b, n, "%s", POS[(int)(e->ct / (long)(CHECK_SEC * SR)) & 3]);
        }
        if (!e->on && e->sig == SIG_CHECK) return snprintf(b, n, "Tap ON: noise plays left, right, both in phase and out of phase, 2 seconds each.");
        if (!e->on && e->sig == SIG_TONE) return snprintf(b, n, "Pick a tone and a level, then tap ON.");
        if (!e->on && e->sig == SIG_BAND) return snprintf(b, n, "Pick an octave and a level, then tap ON.");
        if (!e->on) return snprintf(b, n, "Set a level, tap ON, then solo each track and raise it until it's just audible over the noise.");
        if (e->level == 5) return snprintf(b, n, "-12 dB RMS: peaks can reach 0 dBFS. -14 dB leaves headroom.");
        return snprintf(b, n, "Solo a track and raise its fader until you can only just hear it over the noise.");
    }
    return 0;
}

static void render(void *p, int16_t *out, int frames) {
    noise_t *e = p;
    int changing = e->sig != e->a_sig || e->tone != e->a_tone || e->band != e->a_band || e->range != e->a_range || e->width != e->a_width;
    if (changing && e->gain == 0) { activate(e); changing = 0; }   /* faded out: switch, and fade back in */
    retarget(e);
    if (e->target != e->ramp_to) { e->ramp_to = e->target; e->ramp_step = (e->target - e->gain) / (0.02f * (float)SR); }
    const long cyc = (long)(CHECK_SEC * SR);
    const float kc = 1.0f - expf(-1.0f / (0.01f * (float)SR));
    const int old_pos = (int)(e->ct / cyc) & 3;
    for (int i = 0; i < frames; i++) {
        if (e->gain != e->ramp_to) {
            e->gain += e->ramp_step;
            if ((e->ramp_step > 0 && e->gain > e->ramp_to) || (e->ramp_step < 0 && e->gain < e->ramp_to)) e->gain = e->ramp_to;
        }
        float l = 0, r = 0;
        if (e->gain > 0 || e->target > 0) {
            switch (e->a_sig) {
            case SIG_TONE:
                l = r = (float)sin(e->ph) * e->gain;
                e->ph += 2 * M_PI * TONE_HZ[e->a_tone] / SR; if (e->ph > 2 * M_PI) e->ph -= 2 * M_PI;
                break;
            case SIG_BAND:
                l = bq(&e->bp[0][1], bq(&e->bp[0][0], gen_next(&e->g[0], 0))) * e->gain;
                r = e->a_width ? bq(&e->bp[1][1], bq(&e->bp[1][0], gen_next(&e->g[1], 0))) * e->gain : l;
                break;
            case SIG_CHECK: {
                int pos = (int)(e->ct / cyc) & 3;
                static const float GL[4] = {1, 0, 1, 1}, GR[4] = {0, 1, 1, -1};
                e->cg[0] += (GL[pos] - e->cg[0]) * kc; e->cg[1] += (GR[pos] - e->cg[1]) * kc;
                float x = gen_next(&e->g[0], e->a_range) * e->gain;
                l = x * e->cg[0]; r = x * e->cg[1];
                e->ct++;
                break;
            }
            default:
                l = gen_next(&e->g[0], e->a_range) * e->gain;
                r = e->a_width ? gen_next(&e->g[1], e->a_range) * e->gain : l;
            }
            l = soft(l); r = soft(r);
        }
        float al = fabsf(l), ar = fabsf(r), a = al > ar ? al : ar;
        if (a > e->win_peak) e->win_peak = a;
        e->acc += 0.5 * ((double)l * l + (double)r * r); e->accn++;
        int il = (int)lrintf(l * 32767.f), ir = (int)lrintf(r * 32767.f);
        out[2 * i] = (int16_t)(il > 32767 ? 32767 : il < -32768 ? -32768 : il);
        out[2 * i + 1] = (int16_t)(ir > 32767 ? 32767 : ir < -32768 ? -32768 : ir);
    }
    if (e->a_sig == SIG_CHECK && ((int)(e->ct / cyc) & 3) != old_pos) e->rev++;   /* the tip says where it is now */
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

int mpc_engine_changed(void *inst, char *key, int len) {   /* the GUIDE switched the noise on or off: MPC's screen follows */
    noise_t *e = inst;
    if (e->pending & 1) { e->pending &= ~1; snprintf(key, len, "noise_on"); return 1; }
    if (e->pending & 2) { e->pending &= ~2; snprintf(key, len, "signal"); return 1; }
    return 0;
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, NULL, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
