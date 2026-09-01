/* bench_float — shortest-print throughput (TODO.impl/14).
 * Corpora: nice (x.5), decimal-ish (i/d + 0.25), uniform bit patterns
 * (worst case: extreme exponents exercise the exact tier).
 * Baseline: snprintf("%.17g") on the same values.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <chrono>

#include "emit/float/api.h"

typedef std::chrono::steady_clock clk;

enum { N = 2000000 };
static double vals[N];

static double run_d2s(char* scratch) {
    auto t0 = clk::now();
    volatile int sink = 0;
    for (int i = 0; i < N; i++) {
        sink += yep_d2s_shortest(vals[i], scratch);
    }
    (void)sink;
    return std::chrono::duration<double>(clk::now() - t0).count();
}

static double run_printf(char* scratch) {
    auto t0 = clk::now();
    volatile int sink = 0;
    for (int i = 0; i < N; i++) {
        snprintf(scratch, 32, "%.17g", vals[i]);
        sink += scratch[0];
    }
    (void)sink;
    return std::chrono::duration<double>(clk::now() - t0).count();
}

int main(int argc, char** argv) {
    int quick = argc > 1 && strcmp(argv[1], "--quick") == 0;
    int reps = quick ? 2 : 5;
    char buf[32];
    const char* shapes[3] = {"nice(x.5)", "decimal-ish", "uniform-bits"};
    printf("| corpus | yeptris M/s | ns each | printf M/s | vs printf |\n|---|---|---|---|---|\n");
    for (int shape = 0; shape < 3; shape++) {
        for (int i = 0; i < N; i++) {
            switch (shape) {
            case 0:
                vals[i] = (double)(i % 1000000) + 0.5;
                break;
            case 1:
                vals[i] = (double)(i % 1000) / (double)(1 + (i % 97)) + 0.25;
                break;
            default: {
                unsigned long long s =
                    (unsigned long long)i * 6364136223846793005ull + 1442695040888963407ull;
                memcpy(&vals[i], &s, 8);
                double d = vals[i];
                if (d != d || d - d != 0) {
                    vals[i] = 1.5;
                }
                break;
            }
            }
        }
        double best_us = 1e9, best_pf = 1e9;
        for (int r = 0; r < reps; r++) {
            double a = run_d2s(buf);
            double b = run_printf(buf);
            if (a < best_us) {
                best_us = a;
            }
            if (b < best_pf) {
                best_pf = b;
            }
        }
        double ms = N / best_us / 1e6;
        double pms = N / best_pf / 1e6;
        printf("| %s | %.1f | %.1f | %.1f | %.2fx |\n", shapes[shape], ms, best_us / N * 1e9, pms,
               ms / pms);
    }
    return 0;
}
