// no-port-check: NereusSDR-original test observability; no DSP logic copied.
// Runs blocks of I/Q through one WDSP meter (meter.c), as a receiver's S-meter
// stage does, and hands back the average and peak readings after each block.
#include "comm.h"

int nereus_meter_probe(const double* blocks, int size, int count, double* average, double* peak)
{
    if (!blocks || size <= 0 || count <= 0 || !average || !peak) {
        return 0;
    }
    double* buff = (double*)malloc((size_t)(2 * size) * sizeof(double));
    double result[3] = {0.0, 0.0, 0.0};
    CRITICAL_SECTION* locks[3] = {0, 0, 0};
    METER m = create_meter(1, 0, size, buff, 48000, 0.1, 0.1, result, locks, 0, 1, 2, 0);
    for (int b = 0; b < count; ++b) {
        memcpy(buff, blocks + (size_t)b * (size_t)(2 * size), (size_t)(2 * size) * sizeof(double));
        xmeter(m);
        average[b] = result[0];
        peak[b] = result[1];
    }
    destroy_meter(m);
    free(buff);
    return 1;
}
