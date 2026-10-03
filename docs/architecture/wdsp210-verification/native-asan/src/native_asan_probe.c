// no-port-check: NereusSDR-original native lifecycle and bounds verification harness.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ps3_abi.h"

void OpenChannel(int channel, int in_size, int dsp_size,
                 int input_samplerate, int dsp_rate, int output_samplerate,
                 int type, int state, double tdelayup, double tslewup,
                 double tdelaydown, double tslewdown, int bfo);
void CloseChannel(int channel);
int SetChannelState(int channel, int state, int dmode);
void RXASetNC(int channel, int nc);
void TXASetNC(int channel, int nc);
void TXASetSipMode(int channel, int mode);
void fexchange2(int channel, float* iin, float* qin,
                float* iout, float* qout, int* error);
void fexchange0(int channel, double* in, double* out, int* error);
void init_impulse_cache(int use);
void destroy_impulse_cache(void);
void PSRestoreCorr(int channel, char* filename);
void GetPSDisp(int channel, double* x, double* ym, double* yc, double* ys,
               double* xm_cor, double* ym_cor, double* xa_cor, double* ya_cor,
               int* nsamps_out, int* cpts_out, double* phs_ref_deg_out);

#define RX_CHANNEL 0
#define TX_CHANNEL 1
#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        exit(2); \
    } \
} while (0)

static double tx_in[512];
static double tx_out[512];

static void pump_tx(int blocks)
{
    for (int block = 0; block < blocks; ++block) {
        int error = -1;
        fexchange0(TX_CHANNEL, tx_in, tx_out, &error);
        CHECK(error == 0);
        for (size_t i = 0; i < sizeof(tx_out) / sizeof(tx_out[0]); ++i) {
            CHECK(isfinite(tx_out[i]));
        }
    }
}

static PSFileOperationStatus wait_file(int kind, int require_pending,
                                       int pump, int timeout_ms)
{
    PSFileOperationStatus status = {0, 0, 1};
    int saw_pending = 0;
    for (int elapsed = 0; elapsed < timeout_ms; ++elapsed) {
        CHECK(GetPSFileOperationStatus(TX_CHANNEL, kind, &status) == 1);
        if (status.pending) {
            saw_pending = 1;
        }
        if (!status.pending && (!require_pending || saw_pending)) {
            return status;
        }
        if (pump) {
            pump_tx(1);
        }
        usleep(1000);
    }
    fprintf(stderr, "file operation %d timeout (pending=%d result=%d generation=%llu)\n",
            kind, status.pending, status.result,
            (unsigned long long)status.generation);
    exit(2);
}

static void wait_correction(int wanted_run, int timeout_ms)
{
    int run = -1;
    int busy = -1;
    for (int elapsed = 0; elapsed < timeout_ms; ++elapsed) {
        CHECK(GetPSCorrectionState(TX_CHANNEL, &run, &busy) == 1);
        if (run == wanted_run && busy == 0) {
            return;
        }
        pump_tx(1);
        usleep(1000);
    }
    fprintf(stderr, "correction timeout (wanted=%d run=%d busy=%d)\n",
            wanted_run, run, busy);
    exit(2);
}

static void probe_filter_resize(void)
{
    float rx_i[256] = {0};
    float rx_q[256] = {0};
    float out_i[256] = {0};
    float out_q[256] = {0};
    int error = -1;

    RXASetNC(RX_CHANNEL, 4096);
    TXASetNC(TX_CHANNEL, 4096);
    SetChannelState(RX_CHANNEL, 1, 0);
    SetChannelState(TX_CHANNEL, 1, 0);
    fexchange2(RX_CHANNEL, rx_i, rx_q, out_i, out_q, &error);
    CHECK(error == 0);
    for (size_t i = 0; i < 256; ++i) {
        CHECK(isfinite(out_i[i]));
        CHECK(isfinite(out_q[i]));
    }
    pump_tx(2);
    RXASetNC(RX_CHANNEL, 2048);
    TXASetNC(TX_CHANNEL, 2048);
    fexchange2(RX_CHANNEL, rx_i, rx_q, out_i, out_q, &error);
    CHECK(error == 0);
    pump_tx(2);
    fprintf(stdout, "PASS filter resize/process\n");
}

static void probe_display(void)
{
    double *x = calloc(PS3_MAX_DISPLAY_SAMPLES, sizeof(*x));
    double *ym = calloc(PS3_MAX_DISPLAY_SAMPLES, sizeof(*ym));
    double *yc = calloc(PS3_MAX_DISPLAY_SAMPLES, sizeof(*yc));
    double *ys = calloc(PS3_MAX_DISPLAY_SAMPLES, sizeof(*ys));
    double *xm = calloc(PS3_DISPLAY_CORRECTION_POINTS, sizeof(*xm));
    double *ymc = calloc(PS3_DISPLAY_CORRECTION_POINTS, sizeof(*ymc));
    double *xa = calloc(PS3_DISPLAY_CORRECTION_POINTS, sizeof(*xa));
    double *ya = calloc(PS3_DISPLAY_CORRECTION_POINTS, sizeof(*ya));
    int nsamps = -1;
    int cpts = -1;
    double phase = NAN;
    CHECK(x && ym && yc && ys && xm && ymc && xa && ya);
    GetPSDisp(TX_CHANNEL, x, ym, yc, ys, xm, ymc, xa, ya,
              &nsamps, &cpts, &phase);
    CHECK(nsamps >= 0 && nsamps <= PS3_MAX_DISPLAY_SAMPLES);
    CHECK(cpts >= 0 && cpts <= PS3_DISPLAY_CORRECTION_POINTS);
    CHECK(isfinite(phase));
    free(ya); free(xa); free(ymc); free(xm);
    free(ys); free(yc); free(ym); free(x);
    fprintf(stdout, "PASS bounded display (%d/%d)\n", nsamps, cpts);
}

static void probe_ps3(const char* fixture)
{
    PSFileOperationStatus before = {0, 0, 0};
    CHECK(GetPSFileOperationStatus(TX_CHANNEL, 1, &before) == 1);
    PSRestoreCorr(TX_CHANNEL, (char*)fixture);
    PSFileOperationStatus restored = wait_file(1, 1, 1, 8000);
    CHECK(restored.generation == before.generation + 1);
    CHECK(restored.result == 0);
    wait_correction(1, 3000);

    int available = 0;
    CHECK(GetPSCorrectionAvailable(TX_CHANNEL, &available) == 1);
    CHECK(available == 1);
    CHECK(RequestPSCorrectionStop(TX_CHANNEL) == 1);
    wait_correction(0, 3000);
    CHECK(ApplyPSCorrection(TX_CHANNEL) == 1);
    wait_correction(1, 3000);

    SetChannelState(TX_CHANNEL, 0, 1);
    CHECK(StopPSCorrectionQuiescent(TX_CHANNEL) == 1);
    {
        int run = -1;
        int busy = -1;
        CHECK(GetPSCorrectionState(TX_CHANNEL, &run, &busy) == 1);
        CHECK(run == 0 && busy == 0);
    }
    CHECK(GetPSCorrectionAvailable(TX_CHANNEL, &available) == 1);
    CHECK(available == 1);

    before = restored;
    PSRestoreCorr(TX_CHANNEL, (char*)fixture);
    int saw_post_parse_pending = 0;
    for (int elapsed = 0; elapsed < 3000; ++elapsed) {
        PSFileOperationStatus status;
        int run = 0;
        int busy = 0;
        CHECK(GetPSFileOperationStatus(TX_CHANNEL, 1, &status) == 1);
        CHECK(GetPSCorrectionState(TX_CHANNEL, &run, &busy) == 1);
        if (status.pending && run == 1 && busy == 1) {
            saw_post_parse_pending = 1;
            CHECK(CancelPSFileOperation(TX_CHANNEL, 1) == 1);
            break;
        }
        usleep(1000);
    }
    CHECK(saw_post_parse_pending);
    CHECK(StopPSCorrectionQuiescent(TX_CHANNEL) == 1);
    PSFileOperationStatus cancelled = wait_file(1, 0, 0, 8000);
    CHECK(cancelled.generation == before.generation + 1);
    CHECK(cancelled.result == 2);
    fprintf(stdout, "PASS PS3 restore/active-stop/apply/quiescent-stop/cancel\n");
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    setvbuf(stdout, NULL, _IOLBF, 0);
    init_impulse_cache(0);

    OpenChannel(RX_CHANNEL, 256, 1024, 48000, 48000, 48000,
                0, 0, 0.010, 0.025, 0.000, 0.010, 1);
    OpenChannel(TX_CHANNEL, 256, 2048, 48000, 96000, 48000,
                1, 0, 0.000, 0.010, 0.000, 0.010, 1);
    TXASetSipMode(TX_CHANNEL, 0);

    probe_filter_resize();
    probe_ps3(argv[1]);
    probe_display();

    SetChannelState(TX_CHANNEL, 0, 1);
    SetChannelState(RX_CHANNEL, 0, 1);
    CloseChannel(TX_CHANNEL);
    CloseChannel(RX_CHANNEL);
    destroy_impulse_cache();
    fprintf(stdout, "PASS teardown\n");
    return 0;
}
