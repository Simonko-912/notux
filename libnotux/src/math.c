/*
 * libnotux — math helpers
 */

#include <notux/libc.h>

int64_t nx_abs(int64_t x) { return x < 0 ? -x : x; }
int64_t nx_min(int64_t a, int64_t b) { return a < b ? a : b; }
int64_t nx_max(int64_t a, int64_t b) { return a > b ? a : b; }

/* xorshift64 PRNG */
static uint64_t g_rng = 0x9E3779B97F4A7C15ULL;

uint64_t nx_rand(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return g_rng;
}

void nx_srand(uint64_t seed) { g_rng = seed ? seed : 1; }

double nx_floor(double x) {
    long long i = (long long)x;
    if (x < 0 && x != (double)i) i -= 1;
    return (double)i;
}

double nx_ceil(double x) {
    long long i = (long long)x;
    if (x > 0 && x != (double)i) i += 1;
    return (double)i;
}

double nx_sqrt(double x) {
    if (x <= 0) return 0.0;
    double g = x;
    for (int i = 0; i < 64; i++) {
        g = (g + x / g) * 0.5;
        if (g == 0.0) break;
    }
    return g;
}

double nx_pow(double base, double exp) {
    if (exp == 0.0) return 1.0;
    int whole = (int)exp;
    if ((double)whole == exp) {
        double r = 1.0;
        for (int i = 0; i < (whole < 0 ? -whole : whole); i++) r *= base;
        return whole < 0 ? 1.0 / r : r;
    }
    /* exp * ln(base) via Taylor for |ln| small; approximate otherwise */
    double n = exp * nx_log(base);
    int iv = (int)n;
    double frac = n - iv;
    double r = 1.0;
    for (int i = 0; i < (iv < 0 ? -iv : iv); i++) r *= 2.718281828459045;
    if (iv < 0) r = 1.0 / r;
    return r * (1.0 + frac);
}

static double taylor_sin(double x) {
    double s = x, term = x, x2 = x * x;
    for (int i = 1; i < 12; i++) {
        term *= -x2 / ((double)(2*i) * (2*i + 1));
        s += term;
    }
    return s;
}

double nx_sin(double x) {
    double two_pi = 6.283185307179586;
    while (x > 3.141592653589793) x -= two_pi;
    while (x < -3.141592653589793) x += two_pi;
    return taylor_sin(x);
}

double nx_cos(double x) { return nx_sin(x + 1.5707963267948966); }

double nx_log(double x) {
    if (x <= 0) return 0.0;
    /* ln(x) using exponent extraction */
    int e = 0;
    while (x >= 2.0) { x /= 2.0; e++; }
    while (x < 1.0)  { x *= 2.0; e--; }
    double y = (x - 1.0) / (x + 1.0);
    double y2 = y * y;
    double s = 0.0, term = y;
    for (int i = 1; i < 24; i += 2) {
        s += term / (double)i;
        term *= y2;
    }
    return 2.0 * s + (double)e * 0.6931471805599453;
}

double nx_log2(double x) { return nx_log(x) / 0.6931471805599453; }