/*****************************************************************************

    MCC 118 Channel 4 Data Logger
    
    Purpose:
        Acquire data from channel 4 at 4 kHz for 10 seconds and save to CSV file.

    Description:
        Performs a finite acquisition on channel 4 only:
        - Scan rate: 4 kHz (4000 samples/second)
        - Duration: 10 seconds
        - Total samples: 40000 samples
        - Output: CSV file in the same directory

*****************************************************************************/
#include "../../daqhats_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void)
{
    int result = RESULT_SUCCESS;
    uint8_t address = 0;
    char channel_string[512];
    char options_str[512];
    char c;
    int i;
    
    // Channel 4 only
    uint8_t channel_mask = CHAN4;
    convert_chan_mask_to_string(channel_mask, channel_string);
    
    uint8_t num_channels = 1;  // Only channel 4
    
    // 10 seconds at 4 kHz = 40000 samples
    double scan_rate = 4000.0;  // 4 kHz
    double duration_seconds = 10.0;
    uint32_t samples_per_channel = (uint32_t)(scan_rate * duration_seconds);  // 40000 samples
    double actual_scan_rate = 0.0;
    
    // Calculate actual scan rate
    mcc118_a_in_scan_actual_rate(num_channels, scan_rate, &actual_scan_rate);
    
    // Buffer size: enough for all samples
    uint32_t buffer_size = samples_per_channel * num_channels;
    double *read_buf = (double*)malloc(buffer_size * sizeof(double));
    if (read_buf == NULL)
    {
        printf("Error: Failed to allocate memory for buffer\n");
        return -1;
    }
    
    int32_t read_request_size = 10000;  // Read 10000 samples at a time
    double timeout = 10.0;
    uint32_t options = OPTS_DEFAULT;
    
    uint16_t read_status = 0;
    uint32_t samples_read_per_channel = 0;
    uint32_t total_samples_read = 0;
    
    // CSV file name with timestamp
    char csv_filename[256];
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(csv_filename, sizeof(csv_filename), "channel4_data_%Y%m%d_%H%M%S.csv", timeinfo);
    
    FILE *csv_file = NULL;
    
    // Select an MCC118 HAT device to use.
    if (select_hat_device(HAT_ID_MCC_118, &address))
    {
        printf("Error: No MCC 118 device found\n");
        free(read_buf);
        return -1;
    }
    
    printf("\nSelected MCC 118 device at address %d\n", address);
    
    // Open a connection to the device.
    result = mcc118_open(address);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        free(read_buf);
        return -1;
    }
    
    convert_options_to_string(options, options_str);
    
    printf("\nMCC 118 Channel 4 Data Logger\n");
    printf("    Channel: %s\n", channel_string);
    printf("    Scan rate: %.2f Hz (requested: %.2f Hz)\n", actual_scan_rate, scan_rate);
    printf("    Duration: %.1f seconds\n", duration_seconds);
    printf("    Total samples: %d\n", samples_per_channel);
    printf("    Output file: %s\n", csv_filename);
    printf("    Options: %s\n", options_str);
    
    printf("\nPress ENTER to start acquisition...\n");
    scanf("%c", &c);
    
    // Open CSV file for writing
    csv_file = fopen(csv_filename, "w");
    if (csv_file == NULL)
    {
        printf("Error: Failed to open CSV file for writing\n");
        mcc118_close(address);
        free(read_buf);
        return -1;
    }
    
    // Write CSV header
    fprintf(csv_file, "Sample_Number,Time_Seconds,Channel_4_Voltage\n");
    
    // Configure and start the scan.
    result = mcc118_a_in_scan_start(address, channel_mask, samples_per_channel,
        scan_rate, options);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        fclose(csv_file);
        mcc118_close(address);
        free(read_buf);
        return -1;
    }
    
    printf("\nStarting acquisition...\n");
    printf("Progress: ");
    fflush(stdout);
    
    double start_time = 0.0;
    double sample_time = 0.0;
    
    // Read data until all samples are acquired
    while (total_samples_read < samples_per_channel)
    {
        // Calculate how many samples we still need
        uint32_t remaining_samples = samples_per_channel - total_samples_read;
        if (remaining_samples < read_request_size)
        {
            read_request_size = remaining_samples;
        }
        
        // Read data from device
        result = mcc118_a_in_scan_read(address, &read_status, read_request_size,
            timeout, read_buf, buffer_size, &samples_read_per_channel);
        
        if (result != RESULT_SUCCESS)
        {
            print_error(result);
            break;
        }
        
        if (read_status & STATUS_HW_OVERRUN)
        {
            printf("\n\nHardware overrun\n");
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            printf("\n\nBuffer overrun\n");
            break;
        }
        
        // Write data to CSV file
        for (i = 0; i < samples_read_per_channel; i++)
        {
            sample_time = (double)(total_samples_read + i) / actual_scan_rate;
            fprintf(csv_file, "%d,%.6f,%.6f\n", 
                    total_samples_read + i + 1,
                    sample_time,
                    read_buf[i]);
        }
        
        total_samples_read += samples_read_per_channel;
        
        // Show progress
        if (total_samples_read % 10000 == 0)
        {
            printf(".");
            fflush(stdout);
        }
        
        // Check if scan is still running
        if (!(read_status & STATUS_RUNNING))
        {
            break;
        }
    }
    
    printf("\n");
    
    // Close CSV file
    fclose(csv_file);
    
    printf("Acquisition completed!\n");
    printf("Total samples read: %d\n", total_samples_read);
    printf("Data saved to: %s\n", csv_filename);
    
stop:
    // Stop the scan
    print_error(mcc118_a_in_scan_stop(address));
    print_error(mcc118_a_in_scan_cleanup(address));
    print_error(mcc118_close(address));
    
    // Free allocated memory
    free(read_buf);
    
    return 0;
}

