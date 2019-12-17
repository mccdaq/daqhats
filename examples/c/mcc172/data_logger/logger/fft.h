#ifndef FFT_H_INCLUDED
#define FFT_H_INCLUDED


double hann_window(int index, int max);
void calculate_real_fft(double* data, int n_samples, int stride,
     int channel, double max_v, double* spectrum, gboolean first_time);

#endif // FFT_H_INCLUDED
