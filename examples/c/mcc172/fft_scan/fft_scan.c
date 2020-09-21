/*****************************************************************************

    MCC 172 Functions Demonstrated:
        mcc172_iepe_config_write
        mcc172_a_in_clock_config_write
        mcc172_a_in_clock_config_read
        mcc172_a_in_scan_start
        mcc172_a_in_scan_read
        mcc172_a_in_scan_stop
        mcc172_a_in_scan_cleanup

    Purpose:
        Perform a finite acquisition on a channel, calculate the FFT, and
        display peak information.

    Description:
        Acquires blocks of analog input data for a single channel then performs
        an FFT calculation to determine the frequency content. The highest
        frequency peak is detected and displayed, along with harmonics. The 
        time and frequency data are saved to a CSV file.
        
        This example uses the Kiss FFT library from 
        https://github.com/mborgerding/kissfft.

*****************************************************************************/
#include "../../daqhats_utils.h"
#include <math.h>
#include "kiss_fftr.h"

#define USE_WINDOW

double window(int index, int max)
{
#ifdef USE_WINDOW
    // Hann window function.
    return 0.5 - 0.5*cos(2*M_PI*index / max);
#else
    // No windowing.
    return 1.0;
#endif
}

double window_compensation(void)
{
#ifdef USE_WINDOW
    // Hann window compensation factor.
    return 2.0;
#else
    // No compensation.
    return 1.0;
#endif
}

double quadratic_interpolate(double bin0, double bin1, double bin2)
{
    // Interpolate between the bins of an FFT peak to find a more accurate
    // frequency.  bin1 is the FFT value at the detected peak, bin0 and bin2
    // are the values from adjacent bins above and below the peak. Returns
    // the offset value from the index of bin1.
    double y1, y2, y3;
    double d;

    y1 = fabs(bin0);
    y2 = fabs(bin1);
    y3 = fabs(bin2);

    d = (y3 - y1) / (2 * (2 * y2 - y1 - y3));

    return d;
}

const char* order_suffix(int index)
{
    switch (index)
    {
    case 1:
        return "st";
    case 2:
        return "nd";
    case 3:
        return "rd";
    default:
        return "th";
    }
}

/* Calculate a real to real FFT, returning in units of dBFS. */
void calculate_real_fft(double* data, int n_samples, double max_v,
    double* spectrum)
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
    for (i = 0; i < n_samples; i++)
    {
        in[i] = window(i, n_samples) * data[i] / max_v;
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

int main(void)
{
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char channel_string[512];
    char options_str[512];
    char c;
    int i;

    // Set the channel mask which is used by the library function
    // mcc172_a_in_scan_start to specify the channels to acquire.
    // The functions below, will parse the channel mask into a
    // character string for display purposes.
    uint8_t channel_mask = {CHAN0 | CHAN1};
    convert_chan_mask_to_string(channel_mask, channel_string);

    int max_channel_array_length = mcc172_info()->NUM_AI_CHANNELS;
    int channel_array[max_channel_array_length];
    uint8_t num_channels = convert_chan_mask_to_array(channel_mask,
        channel_array);

    uint32_t samples_per_channel = 12800;
    double scan_rate = 51200.0;
    double actual_scan_rate = 0.0;
    
    uint32_t buffer_size = samples_per_channel * num_channels;
    double read_buf[buffer_size];

    double timeout = 5.0;
    uint32_t options = OPTS_DEFAULT;
    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;
    uint8_t synced;
    uint8_t clock_source;
    uint8_t iepe_enable;
    uint8_t channel;
    char logname[64];

    FILE* logfile;
    
    double* spectrum;
    double f_i;
    int peak_index;
    double peak_val;
    double peak_offset;
    double peak_freq;
    double h_freq;
    double h_val;
    int h_index;

    // Select an MCC172 HAT device to use.
    if (select_hat_device(HAT_ID_MCC_172, &address))
    {
        // Error getting device.
        return -1;
    }
    
    printf ("\nSelected MCC 172 device at address %d\n", address);

    // Open a connection to the device.
    result = mcc172_open(address);
    STOP_ON_ERROR(result);

    // Turn on IEPE supply?
    printf("Enable IEPE power [y or n]?  ");
    scanf("%c", &c);
    if ((c == 'y') || (c == 'Y'))
    {
        iepe_enable = 1;
    }
    else if ((c == 'n') || (c == 'N'))
    {
        iepe_enable = 0;
    }
    else
    {
        printf("Error: Invalid selection\n");
        mcc172_close(address);
        return 1;
    }
    flush_stdin();
    
    for (i = 0; i < num_channels; i++)
    {
        result = mcc172_iepe_config_write(address, channel_array[i], 
            iepe_enable);
        STOP_ON_ERROR(result);
    }
    
    // Set the ADC clock to the desired rate.
    result = mcc172_a_in_clock_config_write(address, SOURCE_LOCAL, scan_rate);
    STOP_ON_ERROR(result);
    
    // Wait for the ADCs to synchronize.
    do
    {
        result = mcc172_a_in_clock_config_read(address, &clock_source,
            &actual_scan_rate, &synced);
        STOP_ON_ERROR(result);
        usleep(5000);
    } while (synced == 0);

    convert_options_to_string(options, options_str);

    printf("\nMCC 172 Multi channel FFT example\n");
    printf("    Functions demonstrated:\n");
    printf("        mcc172_iepe_config_write\n");
    printf("        mcc172_a_in_clock_config_write\n");
    printf("        mcc172_a_in_clock_config_read\n");
    printf("        mcc172_a_in_scan_start\n");
    printf("        mcc172_a_in_scan_read\n");
    printf("        mcc172_a_in_scan_stop\n");
    printf("        mcc172_a_in_scan_cleanup\n");
    printf("    IEPE power: %s\n", iepe_enable ? "on" : "off");
    printf("    Channels %s\n", channel_string);
    printf("    Samples per channel: %d\n", samples_per_channel);
    printf("    Requested scan rate: %-10.2f\n", scan_rate);
    printf("    Actual scan rate: %-10.2f\n", actual_scan_rate);
    printf("    Options: %s\n", options_str);

    printf("\nPress ENTER to continue\n");
    scanf("%c", &c);

    // Configure and start the scan.
    result = mcc172_a_in_scan_start(address, channel_mask, samples_per_channel,
        options);
    STOP_ON_ERROR(result);

    printf("Scanning inputs...\n\n");

    // Read the specified number of samples.
    result = mcc172_a_in_scan_read(address, &read_status, samples_per_channel,
        timeout, read_buf, buffer_size, &samples_read_per_channel);
    STOP_ON_ERROR(result);

    if (read_status & STATUS_HW_OVERRUN)
    {
        printf("\n\nHardware overrun\n");
        goto stop;
    }
    else if (read_status & STATUS_BUFFER_OVERRUN)
    {
        printf("\n\nBuffer overrun\n");
        goto stop;
    }

    if (samples_read_per_channel >= samples_per_channel)
    {
        // allocate buffers
        double* channel_data = (double*)malloc(sizeof(double) * 
            samples_per_channel);
        spectrum = (double*)malloc(sizeof(double) * 
            (samples_per_channel/2 + 1));

        for (channel = 0; channel < num_channels; channel++)
        {
            printf("===== Channel %d:\n", channel);
            
            // separate the data
            for (i = 0; i < samples_per_channel; i++)
            {
                channel_data[i] = read_buf[i*num_channels + channel];
            }

            // open the log file
            sprintf(logname, "fft_scan_%d.csv", channel);
            logfile = fopen(logname, "wt");
            fprintf(logfile, 
                "Time data (V), Frequency (Hz), Spectrum (dBFS)\n");

            // Calculate and display the FFT.
            calculate_real_fft(channel_data, samples_per_channel, 
                mcc172_info()->AI_MAX_RANGE, spectrum);

            // Calculate dBFS and find peak
            f_i = 0.0;
            peak_index = 0;
            peak_val = -1000.0;
            
            for (i = 0; i < (samples_per_channel/2 + 1); i++)
            {
                // Find the peak value and index.
                if (spectrum[i] > peak_val)
                {
                    peak_val = spectrum[i];
                    peak_index = i;
                }
                
                // Save to the CSV file.
                fprintf(logfile, "%f,%f,%f\n", read_buf[i], f_i, spectrum[i]);
                
                f_i += scan_rate / samples_per_channel;
            }
        
            fclose(logfile);

            if ((peak_index > 0) && (peak_index < (samples_per_channel/2)))
            {
                // Interpolate for a more precise peak frequency.
                peak_offset = quadratic_interpolate(spectrum[peak_index - 1], 
                    spectrum[peak_index], spectrum[peak_index + 1]);
                peak_freq = (peak_index + peak_offset) * scan_rate / 
                    samples_per_channel;
            }
            else
            {
                peak_freq = peak_index * scan_rate / samples_per_channel;
            }
            printf("Peak: %.1f dBFS at %.1f Hz\n", peak_val, peak_freq);
        
            // Find and display harmonic levels.
            i = 2;
            do
            {
                h_freq = peak_freq * i;
                if (h_freq <= (scan_rate / 2.0))
                {
                    h_index = (uint32_t)(h_freq * samples_per_channel / scan_rate +
                        0.5);
                    h_val = spectrum[h_index];
                    printf("%d%s harmonic: %.1f dBFS at %.1f Hz\n", i, 
                        order_suffix(i), h_val, h_freq);
                }
                i++;
                // Stop when frequency exceeds Nyquist rate or at the 8th harmonic.
            } while ((i < 8) && (h_freq <= (scan_rate / 2.0)));
            
            printf("Data and FFT saved in %s.\n\n", logname);
        }
        
        free(spectrum);

    }
    else
    {
        printf("Error, %d samples read.\n", samples_read_per_channel);
    }
 

stop:
    print_error(mcc172_a_in_scan_stop(address));
    print_error(mcc172_a_in_scan_cleanup(address));
    
    // Turn off IEPE supply
    for (i = 0; i < num_channels; i++)
    {
        result = mcc172_iepe_config_write(address, channel_array[i], 0);
        STOP_ON_ERROR(result);
    }
    
    print_error(mcc172_close(address));

    return 0;
}
