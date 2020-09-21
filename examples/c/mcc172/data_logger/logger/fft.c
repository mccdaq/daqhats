#include <stdlib.h>
#include <math.h>
#include "kiss_fft/kiss_fftr.h"
#include "fft.h"

#define USE_WINDOW

// Function Prototypes
static double hann_window(int index, int max);
static double window_compensation(void);


static double hann_window(int index, int max)
{
#ifdef USE_WINDOW
    // Hann window function.
    return 0.5 - 0.5*cos(2*M_PI*index / max);
#else
    // No windowing.
    return 1.0;
#endif
}

static double window_compensation(void)
{
#ifdef USE_WINDOW
    // Hann window compensation factor.
    return 2.0;
#else
    // No compensation.
    return 1.0;
#endif
}

/* Calculate a real to real FFT, returning in units of dBFS. */
void calculate_real_fft(double* data, int n_samples, int stride,
     int chan_idx, double max_v, gfloat* spectrum)
{
    double real_part;
    double imag_part;
    int i;
    kiss_fftr_cfg cfg;
    kiss_fft_scalar* in;
    kiss_fft_cpx* out;

    // Allocate the FFT buffers and config
    cfg = kiss_fftr_alloc(n_samples, 0, 0, 0);
    in = (kiss_fft_scalar*)malloc(sizeof(kiss_fft_scalar) * n_samples);
    out = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * (n_samples/2 + 1));

    // Apply the window and normalize the time data.
    for (i = chan_idx; i < n_samples; i++)
    {
        in[i] = hann_window(i, n_samples) * data[(i*stride)+chan_idx] / max_v;
    }

    // Perform the FFT.
    kiss_fftr(cfg, in, out);

    // Convert the complex results to real and convert to dBFS.
    for (i = 0; i < n_samples/2 + 1; i++)
    {
        real_part = out[i].r;
        imag_part = out[i].i;

        if (i == 0)
        {
            // Don't multiply DC value times 2.
            spectrum[i] = 20*log10(window_compensation() *
                sqrt(real_part * real_part + imag_part * imag_part) /
                n_samples);
        }
        else
        {
            spectrum[i] = 20*log10(window_compensation() * 2 *
                sqrt(real_part * real_part + imag_part * imag_part) /
                n_samples);
        }
    }

    // Clean up the config and buffers.
    free(cfg);
    free(in);
    free(out);
}
