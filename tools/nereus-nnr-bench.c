// =================================================================
// tools/nereus-nnr-bench.c  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Developer benchmark that drives the
// vendored WDSP NNR block through its public API; no DSP logic is copied.
//
// Measures what the neural noise reduction models cost on this computer.
// It forces the built-in models, creates one NNR block per model with the
// arguments the receive chain uses, feeds a deterministic synthetic voice
// plus noise signal and times every call. The nereus-nnr-bench-relaxed
// build links the same source against a copy of nnet.c compiled with
// relaxed floating-point options, so the two builds can be compared for
// speed (timings) and for output (--dump, then --compare). Neither build is
// part of the application or its packages.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (R-R3-40).
// =================================================================

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

// The vendored WDSP headers declare a few functions without prototypes;
// keep that third-party diagnostic out of this tool's warning set.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#include "comm.h"
#include "nnet.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#endif

#ifndef NEREUS_NNR_BENCH_RELAXED
#define NEREUS_NNR_BENCH_RELAXED 0
#endif

// Receive-chain values the NNR block is created with. The argument order and
// the literal values follow the create_nnr() call in third_party/wdsp/src/
// RXA.c (the NNR block of create_rxa); the 4096-sample buffer is the DSP
// buffer size the application configures for the receive channel.
enum
{
    kDspRate = 48000,
    kBufferSize = 4096,
    kNetRate = 16000,
    kFftSize = 512,
    kOverlap = 2,
    kLookahead = 1,
    kOutputMode = 1,
    kPosition = 1
};
static const double kMaskFloorDb = -25.0;

// One network frame is one hop (fftsize / overlap samples) at the network
// rate, which is decim * hop samples at the DSP rate: 3 * 256 = 768 samples,
// 16 ms at 48 kHz.
enum
{
    kDecim = kDspRate / kNetRate,
    kHop = kFftSize / kOverlap,
    kSamplesPerFrame = kDecim * kHop
};
static const double kFrameUs = 1.0e6 * (double)kSamplesPerFrame / (double)kDspRate;

// The first second of audio is excluded from the statistics so that model
// state, caches and branch predictors settle first.
static const double kWarmupSeconds = 1.0;

typedef struct
{
    double seconds;
    int slot;            // -1 => both slots
    int cpu;             // -1 => not pinned
    const char* dump;
    int flushToZero;
} Options;

static const char* slotName(int slot)
{
    return slot == 0 ? "Standard" : "Premium";
}

static double nowUs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1.0e6 + (double)ts.tv_nsec * 1.0e-3;
}

static void printLoad(const char* when)
{
    double load[3] = {0.0, 0.0, 0.0};
    if (getloadavg(load, 3) == 3) {
        printf("load average (%s): %.2f %.2f %.2f\n", when, load[0], load[1], load[2]);
    } else {
        printf("load average (%s): unavailable\n", when);
    }
}

static int setFlushToZero(void)
{
#if defined(__aarch64__)
    uint64_t fpcr = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (UINT64_C(1) << 24);  // FPCR.FZ
    __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
    return 1;
#elif defined(__x86_64__) || defined(__i386__)
    _mm_setcsr(_mm_getcsr() | 0x8000u | 0x0040u);  // MXCSR FTZ and DAZ
    return 1;
#else
    return 0;
#endif
}

static int pinToCpu(int cpu)
{
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    const int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) {
        fprintf(stderr, "error: cannot pin to CPU %d: %s\n", cpu, strerror(rc));
        return 0;
    }
    return 1;
#else
    (void)cpu;
    fprintf(stderr, "note: --cpu is not supported on this platform; running unpinned\n");
    return 1;
#endif
}

static void describeCpu(void)
{
#if defined(__linux__)
    printf("running on CPU %d\n", sched_getcpu());
#endif
}

// ---------------------------------------------------------------------------
// Deterministic synthetic input: a voiced talker (gliding pitch, two moving
// formants, syllables and pauses) plus white Gaussian noise from a fixed-seed
// generator. Every run produces the same samples.
// ---------------------------------------------------------------------------
typedef struct
{
    int64_t n;
    double phase;
    uint64_t rng;
    int haveSpare;
    double spare;
} SignalState;

static uint64_t nextRandom(SignalState* s)
{
    uint64_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    s->rng = x;
    return x;
}

static double nextGaussian(SignalState* s)
{
    if (s->haveSpare) {
        s->haveSpare = 0;
        return s->spare;
    }
    double u1 = ((double)(nextRandom(s) >> 11) + 1.0) / 9007199254740993.0;
    double u2 = (double)(nextRandom(s) >> 11) / 9007199254740992.0;
    double r = sqrt(-2.0 * log(u1));
    s->spare = r * sin(2.0 * M_PI * u2);
    s->haveSpare = 1;
    return r * cos(2.0 * M_PI * u2);
}

static double resonance(double f, double centre, double width)
{
    const double d = (f - centre) / width;
    return 1.0 / (1.0 + d * d);
}

static double nextSample(SignalState* s)
{
    const double t = (double)s->n / (double)kDspRate;
    ++s->n;

    // Phrases of 2.5 s: 1.8 s of speech, then a pause. Syllables of 0.2 s.
    const double phrase = fmod(t, 2.5);
    double envelope = 0.0;
    if (phrase < 1.8) {
        const double syl = sin(M_PI * fmod(t, 0.2) / 0.2);
        envelope = syl * syl;
    }

    const double f0 = 120.0 + 30.0 * sin(2.0 * M_PI * 0.7 * t) + 10.0 * sin(2.0 * M_PI * 3.1 * t);
    s->phase += 2.0 * M_PI * f0 / (double)kDspRate;
    if (s->phase > 2.0 * M_PI) {
        s->phase -= 2.0 * M_PI;
    }

    double voice = 0.0;
    if (envelope > 0.0) {
        const double f1 = 500.0 + 200.0 * sin(2.0 * M_PI * 1.3 * t);
        const double f2 = 1500.0 + 500.0 * sin(2.0 * M_PI * 0.9 * t);
        for (int k = 1; k * f0 < 3500.0; ++k) {
            const double fk = k * f0;
            const double amp = (resonance(fk, f1, 100.0) + 0.5 * resonance(fk, f2, 150.0)) / k;
            voice += amp * sin(k * s->phase);
        }
    }

    return 0.2 * envelope * voice + 0.02 * nextGaussian(s);
}

// ---------------------------------------------------------------------------

static int compareDoubles(const void* a, const void* b)
{
    const double x = *(const double*)a;
    const double y = *(const double*)b;
    return (x > y) - (x < y);
}

static double percentile(double* values, int count, double p)
{
    if (count <= 0) {
        return 0.0;
    }
    qsort(values, (size_t)count, sizeof(double), compareDoubles);
    int idx = (int)ceil(p * (double)count) - 1;
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= count) {
        idx = count - 1;
    }
    return values[idx];
}

static double maxOf(const double* values, int count)
{
    double m = 0.0;
    for (int i = 0; i < count; ++i) {
        if (values[i] > m) {
            m = values[i];
        }
    }
    return m;
}

static int runSlot(int slot, const Options* opt, const char* dumpPath)
{
    const double createStart = nowUs();
    NNR a = create_nnr(1, kPosition, kBufferSize, NULL, NULL, kDspRate, kNetRate,
                       kFftSize, kOverlap, kLookahead, kMaskFloorDb, kOutputMode);
    const double createUs = nowUs() - createStart;
    if (!a) {
        fprintf(stderr, "error: create_nnr failed\n");
        return 0;
    }
    if (setModel_nnr(a, slot) != slot) {
        fprintf(stderr, "error: model slot %d (%s) did not load\n", slot, slotName(slot));
        destroy_nnr(a);
        return 0;
    }
    if (!getRun_nnr(a)) {
        fprintf(stderr, "error: the NNR block is not running (model or rate rejected)\n");
        destroy_nnr(a);
        return 0;
    }

    double* buffer = (double*)calloc(2u * kBufferSize, sizeof(double));
    const int64_t wanted = (int64_t)ceil(opt->seconds * kDspRate);
    const int blocks = (int)((wanted + kBufferSize - 1) / kBufferSize);
    const int warmupBlocks = (int)ceil(kWarmupSeconds * kDspRate / kBufferSize);
    double* blockUs = (double*)calloc((size_t)blocks, sizeof(double));
    double* frameUs = (double*)calloc((size_t)blocks, sizeof(double));
    int* blockFrames = (int*)calloc((size_t)blocks, sizeof(int));
    FILE* dump = NULL;
    if (!buffer || !blockUs || !frameUs || !blockFrames) {
        fprintf(stderr, "error: out of memory\n");
        free(buffer);
        free(blockUs);
        free(frameUs);
        free(blockFrames);
        destroy_nnr(a);
        return 0;
    }
    if (dumpPath) {
        dump = fopen(dumpPath, "wb");
        if (!dump) {
            fprintf(stderr, "error: cannot write %s: %s\n", dumpPath, strerror(errno));
            free(buffer);
            free(blockUs);
            free(frameUs);
            free(blockFrames);
            destroy_nnr(a);
            return 0;
        }
    }
    setBuffers_nnr(a, buffer, buffer);  // in place, as the receive chain runs it

    SignalState sig = {0, 0.0, UINT64_C(0x9E3779B97F4A7C15), 0, 0.0};
    int64_t fed = 0;
    for (int b = 0; b < blocks; ++b) {
        for (int i = 0; i < kBufferSize; ++i) {
            buffer[2 * i + 0] = nextSample(&sig);
            buffer[2 * i + 1] = 0.0;
        }
        const double t0 = nowUs();
        xnnr(a, kPosition);
        blockUs[b] = nowUs() - t0;
        const int64_t before = fed / kSamplesPerFrame;
        fed += kBufferSize;
        blockFrames[b] = (int)(fed / kSamplesPerFrame - before);
        if (dump) {
            for (int i = 0; i < kBufferSize; ++i) {
                fwrite(&buffer[2 * i + 0], sizeof(double), 1, dump);
            }
        }
    }
    if (dump) {
        fclose(dump);
    }

    double totalUs = 0.0;
    int64_t frames = 0;
    int measured = 0;
    for (int b = warmupBlocks; b < blocks; ++b) {
        totalUs += blockUs[b];
        frames += blockFrames[b];
        frameUs[measured] = blockFrames[b] > 0 ? blockUs[b] / blockFrames[b] : 0.0;
        blockUs[measured] = blockUs[b];
        ++measured;
    }

    printf("\nslot %d (%s)\n", slot, slotName(slot));
    printf("  create_nnr (loads both models; plans come from in-process wisdom): %.1f ms\n",
           createUs / 1000.0);
    if (measured <= 0 || frames <= 0) {
        printf("  too little audio to measure; raise --seconds\n");
    } else {
        const double mean = totalUs / (double)frames;
        const double framesPerBlock = (double)kBufferSize / (double)kSamplesPerFrame;
        printf("  frames: %lld measured (%.1f s of audio after %.1f s warm-up)\n",
               (long long)frames, (double)frames * kFrameUs / 1.0e6, kWarmupSeconds);
        printf("  us per %.0f ms frame: mean %.1f  p99 %.1f  max %.1f (p99 and max are per-block averages)\n",
               kFrameUs / 1000.0, mean,
               percentile(frameUs, measured, 0.99), maxOf(frameUs, measured));
        printf("  us per %d-sample block (%.2f frames, %.1f ms): mean %.1f  p99 %.1f  max %.1f\n",
               kBufferSize, framesPerBlock, 1000.0 * kBufferSize / kDspRate,
               totalUs / (double)measured, percentile(blockUs, measured, 0.99),
               maxOf(blockUs, measured));
        printf("  share of one core: %.2f%%\n", 100.0 * mean / kFrameUs);
    }
    if (dumpPath) {
        printf("  output written to %s (%lld native doubles)\n", dumpPath,
               (long long)blocks * kBufferSize);
    }

    free(buffer);
    free(blockUs);
    free(frameUs);
    free(blockFrames);
    destroy_nnr(a);
    return 1;
}

static double* readDump(const char* path, long* count)
{
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot read %s: %s\n", path, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    const long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (bytes <= 0 || bytes % (long)sizeof(double) != 0) {
        fprintf(stderr, "error: %s is not a dump of doubles\n", path);
        fclose(f);
        return NULL;
    }
    *count = bytes / (long)sizeof(double);
    double* data = (double*)malloc((size_t)bytes);
    if (!data || fread(data, sizeof(double), (size_t)*count, f) != (size_t)*count) {
        fprintf(stderr, "error: cannot read %s\n", path);
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return data;
}

static int compareDumps(const char* pathA, const char* pathB)
{
    long na = 0;
    long nb = 0;
    double* a = readDump(pathA, &na);
    if (!a) {
        return 1;
    }
    double* b = readDump(pathB, &nb);
    if (!b) {
        free(a);
        return 1;
    }
    if (na != nb) {
        printf("note: lengths differ (%ld and %ld samples); comparing the first %ld\n", na, nb,
               na < nb ? na : nb);
    }
    const long n = na < nb ? na : nb;
    double maxDiff = 0.0;
    double signal = 0.0;
    double error = 0.0;
    for (long i = 0; i < n; ++i) {
        const double d = a[i] - b[i];
        if (fabs(d) > maxDiff) {
            maxDiff = fabs(d);
        }
        signal += a[i] * a[i];
        error += d * d;
    }
    printf("samples compared: %ld\n", n);
    printf("max absolute difference: %.6g\n", maxDiff);
    if (error == 0.0) {
        printf("SNR: identical (infinite)\n");
    } else if (signal == 0.0) {
        printf("SNR: undefined (first dump is silent)\n");
    } else {
        printf("SNR (first dump as reference): %.2f dB\n", 10.0 * log10(signal / error));
    }
    free(a);
    free(b);
    return 0;
}

static void usage(const char* argv0)
{
    fprintf(stderr,
            "usage: %s [--seconds N] [--slot 0|1] [--cpu N] [--dump FILE] [--flush-to-zero]\n"
            "       %s --compare A B\n"
            "\n"
            "  --seconds N      audio to process per model (default 60)\n"
            "  --slot 0|1       0 = Standard, 1 = Premium (default: both)\n"
            "  --cpu N          pin the benchmark thread to CPU N (Linux)\n"
            "  --dump FILE      write the output samples as native doubles; with both\n"
            "                   models the slot is appended (FILE.slot0, FILE.slot1)\n"
            "  --flush-to-zero  set flush-to-zero for the benchmark thread\n"
            "  --compare A B    print the max absolute difference and SNR of B against A\n",
            argv0, argv0);
}

int main(int argc, char** argv)
{
    Options opt = {60.0, -1, -1, NULL, 0};
    setvbuf(stdout, NULL, _IOLBF, 0);  // keep results in order with WDSP's stderr notes
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--compare") == 0 && i + 2 < argc) {
            return compareDumps(argv[i + 1], argv[i + 2]);
        } else if (strcmp(arg, "--seconds") == 0 && i + 1 < argc) {
            opt.seconds = atof(argv[++i]);
        } else if (strcmp(arg, "--slot") == 0 && i + 1 < argc) {
            opt.slot = atoi(argv[++i]);
        } else if (strcmp(arg, "--cpu") == 0 && i + 1 < argc) {
            opt.cpu = atoi(argv[++i]);
        } else if (strcmp(arg, "--dump") == 0 && i + 1 < argc) {
            opt.dump = argv[++i];
        } else if (strcmp(arg, "--flush-to-zero") == 0) {
            opt.flushToZero = 1;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (opt.seconds <= kWarmupSeconds || (opt.slot != -1 && opt.slot != 0 && opt.slot != 1)
        || opt.cpu < -1) {
        usage(argv[0]);
        return 2;
    }

    if (opt.cpu >= 0 && !pinToCpu(opt.cpu)) {
        return 1;
    }
    int ftz = 0;
    if (opt.flushToZero) {
        ftz = setFlushToZero();
        if (!ftz) {
            fprintf(stderr, "note: flush-to-zero is not supported on this CPU; left off\n");
        }
    }

    // Force the models compiled into the library; a stray model file in the
    // working directory would otherwise replace them.
    for (int k = 0; k < NNET_NSLOTS; ++k) {
        SetNNRModelPathSlot(k, "");
    }

    printf("nereus-nnr-bench: %s nnet.c, flush-to-zero %s\n",
           NEREUS_NNR_BENCH_RELAXED ? "relaxed floating-point" : "normal", ftz ? "on" : "off");
    printf("NNR: %d Hz in, %d Hz network, FFT %d, overlap %d, lookahead %d, buffer %d\n",
           kDspRate, kNetRate, kFftSize, kOverlap, kLookahead, kBufferSize);
    describeCpu();
    printLoad("start");

    // Plan the block's two FFTs here first so the FFTW planning time is
    // measured on its own; create_nnr then reuses the plans from FFTW's
    // in-process wisdom.
    {
        double* td = (double*)fftw_malloc(sizeof(double) * kFftSize);
        fftw_complex* fd = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * (kFftSize / 2 + 1));
        const double t0 = nowUs();
        fftw_plan pf = fftw_plan_dft_r2c_1d(kFftSize, td, fd, FFTW_PATIENT);
        fftw_plan pr = fftw_plan_dft_c2r_1d(kFftSize, fd, td, FFTW_PATIENT);
        const double planUs = nowUs() - t0;
        printf("FFTW planning (%d-point forward and inverse, FFTW_PATIENT): %.1f ms\n", kFftSize,
               planUs / 1000.0);
        fftw_destroy_plan(pf);
        fftw_destroy_plan(pr);
        fftw_free(td);
        fftw_free(fd);
    }

    int ok = 1;
    for (int slot = 0; slot < NNET_NSLOTS && ok; ++slot) {
        if (opt.slot != -1 && slot != opt.slot) {
            continue;
        }
        char path[1024];
        const char* dumpPath = NULL;
        if (opt.dump) {
            if (opt.slot == -1) {
                snprintf(path, sizeof(path), "%s.slot%d", opt.dump, slot);
                dumpPath = path;
            } else {
                dumpPath = opt.dump;
            }
        }
        ok = runSlot(slot, &opt, dumpPath);
    }

    printf("\n");
    printLoad("end");
    return ok ? 0 : 1;
}
