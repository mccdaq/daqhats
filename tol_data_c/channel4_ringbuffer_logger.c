/*****************************************************************************

    MCC 118 Channel 4 Ring Buffer Logger
    
    Purpose:
        Acquire data from channel 4 at 4 kHz using ring buffer and save to binary files.
        Uses two threads: one for reading from sensor, one for writing to disk.
    
    Description:
        - Producer thread: Reads from MCC 118 at 4 kHz → writes to ring buffer
        - Consumer thread: Reads from ring buffer → writes to .bin.part files
        - Chunk duration: 2 seconds (8000 samples)
        - Files saved to: tol_data_c/DAD_Files/
        - File format: Binary with header (as per specification)

*****************************************************************************/
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "../examples/c/daqhats_utils.h"

// Constants
#define SCAN_RATE_HZ 4000.0
#define CHUNK_DURATION_SEC 2.0
#define SAMPLES_PER_CHUNK ((uint32_t)(SCAN_RATE_HZ * CHUNK_DURATION_SEC))  // 8000 samples
#define RECORD_SIZE 8  // sizeof(double) = 8 bytes per sample
#define RING_BUFFER_SIZE (4 * 1024 * 1024)  // 4 MB ring buffer
#define OUTPUT_DIR_RELATIVE "DAD_Files"

// Global variable for output directory path
static char g_output_dir[512] = {0};

// Binary file format constants
#define MAGIC "SDAT"
#define VERSION 1

// Ring buffer structure
typedef struct {
    uint8_t *buffer;
    size_t size;
    size_t write_pos;
    size_t read_pos;
    size_t available;  // bytes available to read
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    bool producer_done;
    bool consumer_done;
} ring_buffer_t;

// Global variables
static ring_buffer_t g_ring_buffer;
static uint8_t g_hat_addr = 0;
static uint64_t g_boot_id = 0;
static uint64_t g_seq_counter = 0;
static bool g_running = true;

// Function prototypes
static int init_ring_buffer(ring_buffer_t *rb, size_t size);
static void destroy_ring_buffer(ring_buffer_t *rb);
static size_t ring_buffer_write(ring_buffer_t *rb, const void *data, size_t len);
static size_t ring_buffer_read(ring_buffer_t *rb, void *data, size_t len);
static size_t ring_buffer_available(ring_buffer_t *rb);
static void* producer_thread(void *arg);
static void* consumer_thread(void *arg);
static uint64_t generate_boot_id(void);
static int ensure_output_dir(const char *path);
static int write_chunk_file(uint64_t seq_start, double *samples, uint32_t sample_count, double actual_rate);

// Initialize ring buffer
static int init_ring_buffer(ring_buffer_t *rb, size_t size)
{
    rb->buffer = (uint8_t*)malloc(size);
    if (rb->buffer == NULL)
        return -1;
    
    rb->size = size;
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->available = 0;
    rb->producer_done = false;
    rb->consumer_done = false;
    
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
    pthread_cond_init(&rb->not_full, NULL);
    
    return 0;
}

// Destroy ring buffer
static void destroy_ring_buffer(ring_buffer_t *rb)
{
    if (rb->buffer)
    {
        free(rb->buffer);
        rb->buffer = NULL;
    }
    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);
}

// Write to ring buffer (non-blocking, drops if full)
static size_t ring_buffer_write(ring_buffer_t *rb, const void *data, size_t len)
{
    pthread_mutex_lock(&rb->mutex);
    
    size_t free_space = rb->size - rb->available;
    if (free_space < len)
    {
        // Buffer full - drop oldest data (keep latest)
        size_t drop_bytes = len - free_space;
        rb->read_pos = (rb->read_pos + drop_bytes) % rb->size;
        rb->available -= drop_bytes;
        free_space = rb->size - rb->available;
    }
    
    size_t write_len = (len < free_space) ? len : free_space;
    
    if (write_len > 0)
    {
        size_t first_part = (rb->write_pos + write_len <= rb->size) ? 
                           write_len : (rb->size - rb->write_pos);
        memcpy(rb->buffer + rb->write_pos, data, first_part);
        
        if (write_len > first_part)
        {
            memcpy(rb->buffer, (uint8_t*)data + first_part, write_len - first_part);
        }
        
        rb->write_pos = (rb->write_pos + write_len) % rb->size;
        rb->available += write_len;
    }
    
    pthread_cond_signal(&rb->not_empty);
    pthread_mutex_unlock(&rb->mutex);
    
    return write_len;
}

// Read from ring buffer
static size_t ring_buffer_read(ring_buffer_t *rb, void *data, size_t len)
{
    pthread_mutex_lock(&rb->mutex);
    
    while (rb->available == 0 && !rb->producer_done)
    {
        pthread_cond_wait(&rb->not_empty, &rb->mutex);
    }
    
    if (rb->available == 0)
    {
        pthread_mutex_unlock(&rb->mutex);
        return 0;
    }
    
    size_t read_len = (len < rb->available) ? len : rb->available;
    
    size_t first_part = (rb->read_pos + read_len <= rb->size) ? 
                       read_len : (rb->size - rb->read_pos);
    memcpy(data, rb->buffer + rb->read_pos, first_part);
    
    if (read_len > first_part)
    {
        memcpy((uint8_t*)data + first_part, rb->buffer, read_len - first_part);
    }
    
    rb->read_pos = (rb->read_pos + read_len) % rb->size;
    rb->available -= read_len;
    
    pthread_cond_signal(&rb->not_full);
    pthread_mutex_unlock(&rb->mutex);
    
    return read_len;
}

// Get available bytes in ring buffer
static size_t ring_buffer_available(ring_buffer_t *rb)
{
    pthread_mutex_lock(&rb->mutex);
    size_t avail = rb->available;
    pthread_mutex_unlock(&rb->mutex);
    return avail;
}

// Generate boot ID (random 64-bit)
static uint64_t generate_boot_id(void)
{
    uint64_t id = 0;
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom)
    {
        fread(&id, sizeof(id), 1, urandom);
        fclose(urandom);
    }
    else
    {
        // Fallback: use time
        id = (uint64_t)time(NULL);
    }
    return id;
}

// Ensure output directory exists
static int ensure_output_dir(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1)
    {
        if (mkdir(path, 0755) != 0)
        {
            // Try to create parent directories
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
            if (system(cmd) != 0)
            {
                return -1;
            }
        }
    }
    return 0;
}

// Write chunk file with binary format
static int write_chunk_file(uint64_t seq_start, double *samples, uint32_t sample_count, double actual_rate)
{
    char filename_part[512];
    char filename_final[512];
    time_t now = time(NULL);
    
    snprintf(filename_part, sizeof(filename_part), 
             "%s/chunk_%016llx_%016llx.bin.part", 
             g_output_dir, (unsigned long long)g_boot_id, (unsigned long long)seq_start);
    snprintf(filename_final, sizeof(filename_final), 
             "%s/chunk_%016llx_%016llx.bin", 
             g_output_dir, (unsigned long long)g_boot_id, (unsigned long long)seq_start);
    
    FILE *f = fopen(filename_part, "wb");
    if (!f)
    {
        fprintf(stderr, "Error: Failed to open file %s: %s (current dir: ", 
                filename_part, strerror(errno));
        char cwd[512];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            fprintf(stderr, "%s)\n", cwd);
        }
        else
        {
            fprintf(stderr, "unknown)\n");
        }
        return -1;
    }
    
    // Write header (little-endian)
    fwrite(MAGIC, 4, 1, f);  // magic
    uint16_t version = VERSION;
    fwrite(&version, sizeof(version), 1, f);  // version
    uint32_t device_id = 0;  // Can be set to actual device ID
    fwrite(&device_id, sizeof(device_id), 1, f);  // device_id
    fwrite(&g_boot_id, sizeof(g_boot_id), 1, f);  // boot_id
    fwrite(&seq_start, sizeof(seq_start), 1, f);  // seq_start
    uint32_t sample_rate = (uint32_t)actual_rate;
    fwrite(&sample_rate, sizeof(sample_rate), 1, f);  // sample_rate_hz
    uint16_t record_size = RECORD_SIZE;
    fwrite(&record_size, sizeof(record_size), 1, f);  // record_size
    fwrite(&sample_count, sizeof(sample_count), 1, f);  // sample_count
    uint64_t sensor_time_start = (uint64_t)now;
    fwrite(&sensor_time_start, sizeof(sensor_time_start), 1, f);  // sensor_time_start
    uint64_t sensor_time_end = (uint64_t)now;
    fwrite(&sensor_time_end, sizeof(sensor_time_end), 1, f);  // sensor_time_end
    uint32_t payload_crc32 = 0;  // Optional, can be calculated if needed
    fwrite(&payload_crc32, sizeof(payload_crc32), 1, f);  // payload_crc32
    
    // Write payload (samples)
    fwrite(samples, RECORD_SIZE, sample_count, f);
    
    fclose(f);
    
    // Atomic rename: .part -> .bin
    if (rename(filename_part, filename_final) != 0)
    {
        fprintf(stderr, "Error: Failed to rename %s to %s: %s\n", 
                filename_part, filename_final, strerror(errno));
        unlink(filename_part);  // Clean up
        return -1;
    }
    
    return 0;
}

// Producer thread: Read from MCC 118 and write to ring buffer
static void* producer_thread(void *arg)
{
    int result = RESULT_SUCCESS;
    double actual_scan_rate = 0.0;
    uint8_t num_channels = 1;
    uint8_t channel_mask = CHAN4;
    
    // Calculate actual scan rate
    mcc118_a_in_scan_actual_rate(num_channels, SCAN_RATE_HZ, &actual_scan_rate);
    
    // Start continuous scan
    result = mcc118_a_in_scan_start(g_hat_addr, channel_mask, 0, 
                                     SCAN_RATE_HZ, OPTS_CONTINUOUS);
    if (result != RESULT_SUCCESS)
    {
        fprintf(stderr, "Error starting scan: %d\n", result);
        g_running = false;
        return NULL;
    }
    
    printf("Producer thread started. Scan rate: %.2f Hz\n", actual_scan_rate);
    
    uint32_t read_buffer_size = 1000;  // Read 1000 samples at a time
    double *read_buf = (double*)malloc(read_buffer_size * sizeof(double));
    if (!read_buf)
    {
        fprintf(stderr, "Error: Failed to allocate read buffer\n");
        g_running = false;
        return NULL;
    }
    
    uint16_t read_status = 0;
    uint32_t samples_read = 0;
    double timeout = 1.0;
    
    while (g_running)
    {
        // Read from device
        result = mcc118_a_in_scan_read(g_hat_addr, &read_status, 
                                       READ_ALL_AVAILABLE, timeout,
                                       read_buf, read_buffer_size, &samples_read);
        
        if (result != RESULT_SUCCESS)
        {
            if (result != RESULT_TIMEOUT)
            {
                fprintf(stderr, "Error reading from device: %d\n", result);
                break;
            }
            continue;
        }
        
        if (read_status & (STATUS_HW_OVERRUN | STATUS_BUFFER_OVERRUN))
        {
            fprintf(stderr, "Warning: Overrun detected\n");
        }
        
        // Write to ring buffer
        if (samples_read > 0)
        {
            size_t bytes_written = ring_buffer_write(&g_ring_buffer, read_buf, 
                                                     samples_read * sizeof(double));
            if (bytes_written < samples_read * sizeof(double))
            {
                fprintf(stderr, "Warning: Ring buffer overflow, dropped %zu bytes\n",
                        samples_read * sizeof(double) - bytes_written);
            }
        }
        
        // Small sleep to prevent CPU spinning
        usleep(1000);  // 1 ms
    }
    
    // Mark producer as done
    pthread_mutex_lock(&g_ring_buffer.mutex);
    g_ring_buffer.producer_done = true;
    pthread_cond_signal(&g_ring_buffer.not_empty);
    pthread_mutex_unlock(&g_ring_buffer.mutex);
    
    free(read_buf);
    
    // Stop scan
    mcc118_a_in_scan_stop(g_hat_addr);
    mcc118_a_in_scan_cleanup(g_hat_addr);
    
    printf("Producer thread stopped.\n");
    return NULL;
}

// Consumer thread: Read from ring buffer and write to files
static void* consumer_thread(void *arg)
{
    double *chunk_buffer = (double*)malloc(SAMPLES_PER_CHUNK * sizeof(double));
    if (!chunk_buffer)
    {
        fprintf(stderr, "Error: Failed to allocate chunk buffer\n");
        return NULL;
    }
    
    printf("Consumer thread started.\n");
    
    uint32_t samples_collected = 0;
    double actual_rate = SCAN_RATE_HZ;
    
    while (g_running || ring_buffer_available(&g_ring_buffer) > 0)
    {
        // Try to read enough samples for a chunk
        size_t needed_bytes = (SAMPLES_PER_CHUNK - samples_collected) * sizeof(double);
        size_t bytes_read = ring_buffer_read(&g_ring_buffer, 
                                             chunk_buffer + samples_collected,
                                             needed_bytes);
        
        if (bytes_read > 0)
        {
            samples_collected += bytes_read / sizeof(double);
        }
        
        // If we have enough samples for a chunk, write it
        if (samples_collected >= SAMPLES_PER_CHUNK)
        {
            uint64_t seq_start = g_seq_counter;
            int write_result = write_chunk_file(seq_start, chunk_buffer, SAMPLES_PER_CHUNK, actual_rate);
            if (write_result == 0)
            {
                printf("Chunk written: seq=%llu, samples=%u\n", 
                       (unsigned long long)seq_start, SAMPLES_PER_CHUNK);
                g_seq_counter += SAMPLES_PER_CHUNK;
            }
            else
            {
                fprintf(stderr, "Error writing chunk file (error code: %d)\n", write_result);
            }
            
            samples_collected = 0;
        }
        
        // Small sleep if buffer is empty
        if (bytes_read == 0)
        {
            usleep(10000);  // 10 ms
        }
    }
    
    // Write remaining samples if any
    if (samples_collected > 0)
    {
        uint64_t seq_start = g_seq_counter;
        write_chunk_file(seq_start, chunk_buffer, samples_collected, actual_rate);
        g_seq_counter += samples_collected;
    }
    
    free(chunk_buffer);
    printf("Consumer thread stopped.\n");
    return NULL;
}

int main(void)
{
    int result = RESULT_SUCCESS;
    pthread_t producer_tid, consumer_tid;
    
    printf("\n=== MCC 118 Channel 4 Ring Buffer Logger ===\n");
    printf("Scan rate: %.0f Hz\n", SCAN_RATE_HZ);
    printf("Chunk duration: %.1f seconds\n", CHUNK_DURATION_SEC);
    printf("Samples per chunk: %u\n", SAMPLES_PER_CHUNK);
    
    // Build absolute path for output directory
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        fprintf(stderr, "Error: Failed to get current working directory: %s\n", strerror(errno));
        return -1;
    }
    
    // Build full path: current_dir/DAD_Files
    snprintf(g_output_dir, sizeof(g_output_dir), "%s/%s", cwd, OUTPUT_DIR_RELATIVE);
    printf("Output directory: %s\n", g_output_dir);
    
    // Generate boot ID
    g_boot_id = generate_boot_id();
    g_seq_counter = 0;
    printf("Boot ID: %016llx\n", (unsigned long long)g_boot_id);
    
    // Ensure output directory exists
    if (ensure_output_dir(g_output_dir) != 0)
    {
        fprintf(stderr, "Error: Failed to create output directory: %s (errno: %s)\n", 
                g_output_dir, strerror(errno));
        return -1;
    }
    
    // Verify directory was created
    struct stat st;
    if (stat(g_output_dir, &st) != 0)
    {
        fprintf(stderr, "Error: Output directory does not exist: %s (errno: %s)\n", 
                g_output_dir, strerror(errno));
        return -1;
    }
    printf("Output directory verified: %s\n", g_output_dir);
    
    // Initialize ring buffer
    if (init_ring_buffer(&g_ring_buffer, RING_BUFFER_SIZE) != 0)
    {
        fprintf(stderr, "Error: Failed to initialize ring buffer\n");
        return -1;
    }
    printf("Ring buffer initialized: %u bytes\n", (unsigned int)RING_BUFFER_SIZE);
    
    // Select MCC 118 device
    if (select_hat_device(HAT_ID_MCC_118, &g_hat_addr))
    {
        fprintf(stderr, "Error: No MCC 118 device found\n");
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    printf("Selected MCC 118 device at address %d\n", g_hat_addr);
    
    // Open device
    result = mcc118_open(g_hat_addr);
    if (result != RESULT_SUCCESS)
    {
        print_error(result);
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    printf("\nPress ENTER to start acquisition...\n");
    getchar();
    
    // Create threads
    if (pthread_create(&producer_tid, NULL, producer_thread, NULL) != 0)
    {
        fprintf(stderr, "Error: Failed to create producer thread\n");
        mcc118_close(g_hat_addr);
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    if (pthread_create(&consumer_tid, NULL, consumer_thread, NULL) != 0)
    {
        fprintf(stderr, "Error: Failed to create consumer thread\n");
        g_running = false;
        pthread_join(producer_tid, NULL);
        mcc118_close(g_hat_addr);
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    printf("\nAcquisition started. Press ENTER to stop...\n");
    getchar();
    
    // Stop acquisition
    g_running = false;
    
    // Wait for threads to finish
    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);
    
    // Cleanup
    mcc118_close(g_hat_addr);
    destroy_ring_buffer(&g_ring_buffer);
    
    printf("\nAcquisition stopped. Total chunks: %llu\n", 
           (unsigned long long)(g_seq_counter / SAMPLES_PER_CHUNK));
    
    return 0;
}

