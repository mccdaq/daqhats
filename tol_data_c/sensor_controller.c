/*****************************************************************************

    MCC 118 Channel 4 Sensor Controller with Socket Control
    
    Purpose:
        Acquire data from channel 4 with configurable rate, controlled via
        Unix domain socket. Uses ring buffer architecture with three threads:
        - Control thread: handles socket commands (START, STOP, STATUS, SET_RATE)
        - Producer thread: reads from sensor and writes to ring buffer
        - Consumer thread: reads from ring buffer and writes to chunk files
    
    Description:
        - Default scan rate: 120 Hz
        - Chunk duration: 2 seconds
        - Files saved to: DAD_Files/
        - File format: chunk_<sequence>_.bin.part (renamed to .bin when complete)
        - Socket: /run/sensor_ctrl.sock

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
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <errno.h>
#include <signal.h>
#include "../examples/c/daqhats_utils.h"

// Constants
#define DEFAULT_SCAN_RATE_HZ 120.0
#define CHUNK_DURATION_SEC 2.0
#define RECORD_SIZE 8  // sizeof(double) = 8 bytes per sample
#define RING_BUFFER_SIZE (4 * 1024 * 1024)  // 4 MB ring buffer
#define OUTPUT_DIR_RELATIVE "DAD_Files"
#define SOCKET_PATH "/run/sensor_ctrl.sock"
#define SOCKET_PATH_TMP "/tmp/sensor_ctrl.sock"
#define SOCKET_PATH_LOCAL "./sensor_ctrl.sock"
#define MAX_CMD_LEN 256

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
static atomic_bool g_capture_enabled = ATOMIC_VAR_INIT(false);
static atomic_bool g_running = ATOMIC_VAR_INIT(true);
static atomic_bool g_scan_active = ATOMIC_VAR_INIT(false);
static double g_scan_rate = DEFAULT_SCAN_RATE_HZ;
static pthread_mutex_t g_rate_mutex = PTHREAD_MUTEX_INITIALIZER;
static char g_output_dir[512] = {0};

// Function prototypes
static int init_ring_buffer(ring_buffer_t *rb, size_t size);
static void destroy_ring_buffer(ring_buffer_t *rb);
static size_t ring_buffer_write(ring_buffer_t *rb, const void *data, size_t len);
static size_t ring_buffer_read(ring_buffer_t *rb, void *data, size_t len);
static size_t ring_buffer_available(ring_buffer_t *rb);
static void* control_thread(void *arg);
static void* producer_thread(void *arg);
static void* consumer_thread(void *arg);
static uint64_t generate_boot_id(void);
static int ensure_output_dir(const char *path);
static int write_chunk_file(uint64_t seq_start, double *samples, uint32_t sample_count, double actual_rate);
static void handle_command(const char *cmd, int client_fd);
static void send_status(int client_fd);

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
    
    // File naming: chunk_<sequence>_.bin.part
    snprintf(filename_part, sizeof(filename_part), 
             "%s/chunk_%llu_.bin.part", 
             g_output_dir, (unsigned long long)seq_start);
    snprintf(filename_final, sizeof(filename_final), 
             "%s/chunk_%llu_.bin", 
             g_output_dir, (unsigned long long)seq_start);
    
    FILE *f = fopen(filename_part, "wb");
    if (!f)
    {
        fprintf(stderr, "Error: Failed to open file %s: %s\n", 
                filename_part, strerror(errno));
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

// Send status information to client
static void send_status(int client_fd)
{
    char status_msg[512];
    char serial[16] = {0};
    uint16_t fw_version = 0, boot_version = 0;
    int result;
    
    snprintf(status_msg, sizeof(status_msg), 
             "STATUS: running=%s, scan_active=%s, rate=%.2f Hz, seq=%llu, buffer_avail=%zu",
             atomic_load(&g_capture_enabled) ? "yes" : "no",
             atomic_load(&g_scan_active) ? "yes" : "no",
             g_scan_rate,
             (unsigned long long)g_seq_counter,
             ring_buffer_available(&g_ring_buffer));
    
    // Try to get board info if device is open
    if (mcc118_is_open(g_hat_addr))
    {
        result = mcc118_firmware_version(g_hat_addr, &fw_version, &boot_version);
        if (result == RESULT_SUCCESS)
        {
            char fw_str[64];
            snprintf(fw_str, sizeof(fw_str), ", fw=%X.%02X/%X.%02X",
                     (uint8_t)(fw_version >> 8), (uint8_t)fw_version,
                     (uint8_t)(boot_version >> 8), (uint8_t)boot_version);
            strncat(status_msg, fw_str, sizeof(status_msg) - strlen(status_msg) - 1);
        }
        
        result = mcc118_serial(g_hat_addr, serial);
        if (result == RESULT_SUCCESS)
        {
            char serial_str[32];
            snprintf(serial_str, sizeof(serial_str), ", serial=%s", serial);
            strncat(status_msg, serial_str, sizeof(status_msg) - strlen(status_msg) - 1);
        }
    }
    
    strncat(status_msg, "\n", sizeof(status_msg) - strlen(status_msg) - 1);
    send(client_fd, status_msg, strlen(status_msg), 0);
}

// Handle command from socket
static void handle_command(const char *cmd, int client_fd)
{
    char cmd_copy[MAX_CMD_LEN];
    char *token;
    
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    // Remove trailing newline
    size_t len = strlen(cmd_copy);
    if (len > 0 && cmd_copy[len - 1] == '\n')
        cmd_copy[len - 1] = '\0';
    
    token = strtok(cmd_copy, " \t");
    if (token == NULL)
        return;
    
    if (strcmp(token, "START") == 0)
    {
        atomic_store(&g_capture_enabled, true);
        const char *response = "OK: Acquisition started\n";
        send(client_fd, response, strlen(response), 0);
        printf("Command received: START\n");
    }
    else if (strcmp(token, "STOP") == 0)
    {
        atomic_store(&g_capture_enabled, false);
        const char *response = "OK: Acquisition stopped\n";
        send(client_fd, response, strlen(response), 0);
        printf("Command received: STOP\n");
    }
    else if (strcmp(token, "STATUS") == 0)
    {
        send_status(client_fd);
        printf("Command received: STATUS\n");
    }
    else if (strcmp(token, "SET_RATE") == 0)
    {
        token = strtok(NULL, " \t");
        if (token != NULL)
        {
            double new_rate = atof(token);
            if (new_rate > 0 && new_rate <= 100000.0)  // MCC 118 max is 100 kHz
            {
                pthread_mutex_lock(&g_rate_mutex);
                g_scan_rate = new_rate;
                pthread_mutex_unlock(&g_rate_mutex);
                char response[128];
                snprintf(response, sizeof(response), "OK: Rate set to %.2f Hz\n", new_rate);
                send(client_fd, response, strlen(response), 0);
                printf("Command received: SET_RATE %.2f\n", new_rate);
            }
            else
            {
                const char *response = "ERROR: Invalid rate (must be > 0 and <= 100000)\n";
                send(client_fd, response, strlen(response), 0);
            }
        }
        else
        {
            const char *response = "ERROR: SET_RATE requires a value\n";
            send(client_fd, response, strlen(response), 0);
        }
    }
    else
    {
        char response[128];
        snprintf(response, sizeof(response), "ERROR: Unknown command: %s\n", token);
        send(client_fd, response, strlen(response), 0);
    }
}

// Control thread: Handle socket commands
static void* control_thread(void *arg)
{
    int server_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char cmd_buffer[MAX_CMD_LEN];
    char socket_path[256];
    ssize_t n;
    
    // Determine socket path - try /tmp first (usually writable), then current directory
    struct stat st;
    if (stat("/tmp", &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Try /tmp first (usually writable by all users)
        snprintf(socket_path, sizeof(socket_path), "/tmp/sensor_ctrl.sock");
    }
    else if (stat("/run", &st) == 0 && S_ISDIR(st.st_mode))
    {
        // Try /run if /tmp doesn't exist
        snprintf(socket_path, sizeof(socket_path), "/run/sensor_ctrl.sock");
    }
    else
    {
        // Use current directory as fallback
        fprintf(stderr, "Warning: /tmp and /run not accessible, using current directory\n");
        snprintf(socket_path, sizeof(socket_path), "./sensor_ctrl.sock");
    }
    
    // Remove existing socket file if it exists
    unlink(socket_path);
    
    // Create socket
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return NULL;
    }
    
    // Setup address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        fprintf(stderr, "Failed to bind socket at: %s\n", socket_path);
        close(server_fd);
        return NULL;
    }
    
    // Listen
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        unlink(socket_path);
        return NULL;
    }
    
    printf("Control socket listening on: %s\n", socket_path);
    fflush(stdout);
    
    while (atomic_load(&g_running))
    {
        // Accept connection
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (atomic_load(&g_running))
                perror("accept");
            break;
        }
        
        // Read command
        n = recv(client_fd, cmd_buffer, sizeof(cmd_buffer) - 1, 0);
        if (n > 0)
        {
            cmd_buffer[n] = '\0';
            handle_command(cmd_buffer, client_fd);
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    unlink(socket_path);
    printf("Control thread stopped.\n");
    return NULL;
}

// Producer thread: Read from MCC 118 and write to ring buffer
static void* producer_thread(void *arg)
{
    int result = RESULT_SUCCESS;
    double actual_scan_rate = 0.0;
    uint8_t num_channels = 1;
    uint8_t channel_mask = CHAN4;
    bool scan_started = false;
    
    printf("Producer thread started.\n");
    
    uint32_t read_buffer_size = 1000;  // Read 1000 samples at a time
    double *read_buf = (double*)malloc(read_buffer_size * sizeof(double));
    if (!read_buf)
    {
        fprintf(stderr, "Error: Failed to allocate read buffer\n");
        return NULL;
    }
    
    uint16_t read_status = 0;
    uint32_t samples_read = 0;
    double timeout = 1.0;
    
    while (atomic_load(&g_running))
    {
        // Check if capture is enabled
        if (atomic_load(&g_capture_enabled))
        {
            if (!scan_started)
            {
                // Get current rate
                pthread_mutex_lock(&g_rate_mutex);
                double current_rate = g_scan_rate;
                pthread_mutex_unlock(&g_rate_mutex);
                
                // Calculate actual scan rate
                mcc118_a_in_scan_actual_rate(num_channels, current_rate, &actual_scan_rate);
                
                // Start continuous scan
                result = mcc118_a_in_scan_start(g_hat_addr, channel_mask, 0, 
                                               current_rate, OPTS_CONTINUOUS);
                if (result == RESULT_SUCCESS)
                {
                    scan_started = true;
                    atomic_store(&g_scan_active, true);
                    printf("Scan started at %.2f Hz (requested: %.2f Hz)\n", 
                           actual_scan_rate, current_rate);
                }
                else
                {
                    fprintf(stderr, "Error starting scan: %d\n", result);
                    atomic_store(&g_capture_enabled, false);
                    usleep(100000);  // 100 ms before retry
                    continue;
                }
            }
            
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
        }
        else
        {
            // Capture disabled - stop scan if running
            if (scan_started)
            {
                mcc118_a_in_scan_stop(g_hat_addr);
                mcc118_a_in_scan_cleanup(g_hat_addr);
                scan_started = false;
                atomic_store(&g_scan_active, false);
                printf("Scan stopped.\n");
            }
            usleep(100000);  // 100 ms sleep when not capturing
        }
        
        // Small sleep to prevent CPU spinning
        usleep(1000);  // 1 ms
    }
    
    // Stop scan if still running
    if (scan_started)
    {
        mcc118_a_in_scan_stop(g_hat_addr);
        mcc118_a_in_scan_cleanup(g_hat_addr);
        atomic_store(&g_scan_active, false);
    }
    
    // Mark producer as done
    pthread_mutex_lock(&g_ring_buffer.mutex);
    g_ring_buffer.producer_done = true;
    pthread_cond_signal(&g_ring_buffer.not_empty);
    pthread_mutex_unlock(&g_ring_buffer.mutex);
    
    free(read_buf);
    printf("Producer thread stopped.\n");
    return NULL;
}

// Consumer thread: Read from ring buffer and write to files
static void* consumer_thread(void *arg)
{
    double actual_rate = DEFAULT_SCAN_RATE_HZ;
    uint32_t samples_per_chunk;
    
    // Calculate samples per chunk based on current rate
    pthread_mutex_lock(&g_rate_mutex);
    double current_rate = g_scan_rate;
    pthread_mutex_unlock(&g_rate_mutex);
    samples_per_chunk = (uint32_t)(current_rate * CHUNK_DURATION_SEC);
    
    // Allocate buffer with some margin for rate changes (max 100 kHz = 200k samples)
    uint32_t max_samples_per_chunk = (uint32_t)(100000.0 * CHUNK_DURATION_SEC);
    double *chunk_buffer = (double*)malloc(max_samples_per_chunk * sizeof(double));
    if (!chunk_buffer)
    {
        fprintf(stderr, "Error: Failed to allocate chunk buffer\n");
        return NULL;
    }
    
    printf("Consumer thread started. Samples per chunk: %u\n", samples_per_chunk);
    
    uint32_t samples_collected = 0;
    
    while (atomic_load(&g_running) || ring_buffer_available(&g_ring_buffer) > 0)
    {
        // Update rate and chunk size if rate changed
        pthread_mutex_lock(&g_rate_mutex);
        double new_rate = g_scan_rate;
        pthread_mutex_unlock(&g_rate_mutex);
        
        if (new_rate != actual_rate)
        {
            // If we have partial data, write it before changing rate
            if (samples_collected > 0)
            {
                uint64_t seq_start = g_seq_counter;
                write_chunk_file(seq_start, chunk_buffer, samples_collected, actual_rate);
                g_seq_counter += samples_collected;
                samples_collected = 0;
            }
            
            actual_rate = new_rate;
            samples_per_chunk = (uint32_t)(actual_rate * CHUNK_DURATION_SEC);
            printf("Rate changed to %.2f Hz, new chunk size: %u samples\n", 
                   actual_rate, samples_per_chunk);
        }
        
        // Only collect samples if capture is enabled
        if (atomic_load(&g_capture_enabled) && samples_per_chunk > 0)
        {
            // Try to read enough samples for a chunk
            size_t needed_bytes = (samples_per_chunk - samples_collected) * sizeof(double);
            size_t bytes_read = ring_buffer_read(&g_ring_buffer, 
                                                 chunk_buffer + samples_collected,
                                                 needed_bytes);
            
            if (bytes_read > 0)
            {
                samples_collected += bytes_read / sizeof(double);
            }
            
            // If we have enough samples for a chunk, write it
            if (samples_collected >= samples_per_chunk)
            {
                uint64_t seq_start = g_seq_counter;
                int write_result = write_chunk_file(seq_start, chunk_buffer, samples_per_chunk, actual_rate);
                if (write_result == 0)
                {
                    printf("Chunk written: seq=%llu, samples=%u, rate=%.2f Hz\n", 
                           (unsigned long long)seq_start, samples_per_chunk, actual_rate);
                    g_seq_counter += samples_per_chunk;
                }
                else
                {
                    fprintf(stderr, "Error writing chunk file\n");
                }
                
                samples_collected = 0;
            }
        }
        else
        {
            // Capture disabled - clear collected samples
            samples_collected = 0;
        }
        
        // Small sleep if buffer is empty
        if (ring_buffer_available(&g_ring_buffer) == 0)
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

// Signal handler for graceful shutdown
static void signal_handler(int sig)
{
    (void)sig;
    atomic_store(&g_running, false);
    atomic_store(&g_capture_enabled, false);
}

int main(void)
{
    int result = RESULT_SUCCESS;
    pthread_t control_tid, producer_tid, consumer_tid;
    
    printf("\n=== MCC 118 Channel 4 Sensor Controller ===\n");
    printf("Default scan rate: %.0f Hz\n", DEFAULT_SCAN_RATE_HZ);
    printf("Chunk duration: %.1f seconds\n", CHUNK_DURATION_SEC);
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
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
        fprintf(stderr, "Error: Failed to create output directory: %s\n", g_output_dir);
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
    
    // Create threads
    if (pthread_create(&control_tid, NULL, control_thread, NULL) != 0)
    {
        fprintf(stderr, "Error: Failed to create control thread\n");
        mcc118_close(g_hat_addr);
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    if (pthread_create(&producer_tid, NULL, producer_thread, NULL) != 0)
    {
        fprintf(stderr, "Error: Failed to create producer thread\n");
        atomic_store(&g_running, false);
        pthread_join(control_tid, NULL);
        mcc118_close(g_hat_addr);
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    if (pthread_create(&consumer_tid, NULL, consumer_thread, NULL) != 0)
    {
        fprintf(stderr, "Error: Failed to create consumer thread\n");
        atomic_store(&g_running, false);
        pthread_join(control_tid, NULL);
        pthread_join(producer_tid, NULL);
        mcc118_close(g_hat_addr);
        destroy_ring_buffer(&g_ring_buffer);
        return -1;
    }
    
    printf("\nAll threads started. Waiting for commands on socket...\n");
    printf("Send 'START' command to begin acquisition.\n");
    
    // Wait for threads to finish
    pthread_join(control_tid, NULL);
    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);
    
    // Cleanup
    mcc118_close(g_hat_addr);
    destroy_ring_buffer(&g_ring_buffer);
    
    printf("\nProgram stopped. Total chunks: %llu\n", 
           (unsigned long long)(g_seq_counter / (uint64_t)(DEFAULT_SCAN_RATE_HZ * CHUNK_DURATION_SEC)));
    
    return 0;
}

