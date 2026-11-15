/* Measures every signal's level the slow way (6 s of noise each) and checks the numbers built into src/noise.c
 * (NORM, BNORM) still match, within 0.01 dB. Run it after changing a noise generator or a band filter:
 *   gcc -O2 -std=gnu11 -ffast-math -DNOISE_CALIBRATE -I src -I vst/build -I <kit>/wrapper tests/test_calibration.c -lm -lpthread */
#define NOISE_CALIBRATE
#include "../src/noise.c"
static int fails;
static void check(const char *what, float built, float measured) {
    double db = 20.0 * log10((double)built / measured);
    int ok = fabs(db) < 0.01;
    printf("%s %-8s built %.9g measured %.9g (%+.4f dB)\n", ok ? "ok  " : "FAIL", what, built, measured, db);
    fails += !ok;
}
int main(void) {
    char w[16];
    for (int r = 0; r < 2; r++) { snprintf(w, sizeof w, "range %d", r); check(w, NORM[r], calibrate(r)); }
    for (int b = 0; b < NBAND; b++) { snprintf(w, sizeof w, "band %d", b); check(w, BNORM[b], calibrate_band(b)); }
    struct timespec a, z; clock_gettime(CLOCK_MONOTONIC, &a);
    void *e = mpc_engine()->create(NULL); clock_gettime(CLOCK_MONOTONIC, &z); mpc_engine()->destroy(e);
    double ms = (z.tv_sec - a.tv_sec) * 1e3 + (z.tv_nsec - a.tv_nsec) / 1e6;
    printf("%s create() takes %.2f ms\n", ms < 2 ? "ok  " : "FAIL", ms); fails += ms >= 2;
    printf(fails ? "FAILED (%d)\n" : "PASSED\n", fails);
    return fails != 0;
}
