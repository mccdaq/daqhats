# Sensor Controller with Socket Control

## Overview
This program acquires data from **Channel 4** of the MCC 118 HAT board with remote control via Unix domain socket. It uses a **ring buffer architecture** with three threads:
- **Control thread**: Handles socket commands (START, STOP, STATUS, SET_RATE)
- **Producer thread**: Reads from sensor and writes to ring buffer
- **Consumer thread**: Reads from ring buffer and writes to binary chunk files

## Features
- **Default scan rate**: 120 Hz (configurable via SET_RATE command)
- **Chunk duration**: 2 seconds per chunk file
- **Ring buffer**: 4 MB circular buffer to decouple reading from writing
- **File format**: Binary format with header (as per specification)
- **Atomic writes**: Files written as `.bin.part` then renamed to `.bin` when complete
- **Socket control**: Unix domain socket at `/run/sensor_ctrl.sock`

## Compilation
```bash
cd ~/daqhats/tol_data_c
make
```

This will build `sensor_controller` (and also `channel4_ringbuffer_logger`).

## Running
```bash
./sensor_controller
```

The program will:
1. Initialize the device and create the control socket
2. Wait for commands on the socket
3. Start/stop acquisition based on commands received

## Socket Commands

### START
Start data acquisition.

```bash
python send_command.py START
```

### STOP
Stop data acquisition.

```bash
python send_command.py STOP
```

### STATUS
Get current status information (running state, scan rate, sequence counter, buffer status, firmware version, serial number).

```bash
python send_command.py STATUS
```

Example response:
```
STATUS: running=yes, scan_active=yes, rate=120.00 Hz, seq=240, buffer_avail=1920, fw=1.03/1.00, serial=01234567
```

### SET_RATE
Set the sampling rate (in Hz). Valid range: 0 < rate <= 100000.

```bash
python send_command.py SET_RATE 10000
```

**Note**: If no SET_RATE command is sent, the default rate is 120 Hz.

## Python Client Script

A Python script `send_command.py` is provided for sending commands:

```bash
# Start acquisition
python send_command.py START

# Check status
python send_command.py STATUS

# Change rate to 10 kHz
python send_command.py SET_RATE 10000

# Stop acquisition
python send_command.py STOP
```

## Output Files
Files are saved to: `tol_data_c/DAD_Files/`

**File naming format**: `chunk_<sequence>_.bin`

Example: `chunk_0_.bin`, `chunk_240_.bin`, etc.

- Files are written as `.bin.part` during writing
- Automatically renamed to `.bin` when complete (atomic operation)
- Python uploader should only process `.bin` files, never `.part` files

## Architecture

### Ring Buffer
- **Size**: 4 MB (can hold ~500,000 samples at 8 bytes/sample)
- **Overflow policy**: Drops oldest data (keeps latest) if buffer fills
- **Thread-safe**: Uses mutex and condition variables

### File Format
Binary files with the following structure:

**Header** (fixed size, little-endian):
- `magic` (4 bytes): "SDAT"
- `version` (uint16): 1
- `device_id` (uint32): Device identifier
- `boot_id` (uint64): Random ID generated at program start
- `seq_start` (uint64): Monotonic sequence counter
- `sample_rate_hz` (uint32): Sampling rate
- `record_size` (uint16): 8 (sizeof(double))
- `sample_count` (uint32): Number of samples in chunk
- `sensor_time_start` (uint64): Timestamp
- `sensor_time_end` (uint64): Timestamp
- `payload_crc32` (uint32): CRC32 (currently 0)

**Payload**:
- `sample_count` × `record_size` bytes of raw sample data (doubles)

## Thread Safety
- Producer thread reads sensor continuously when enabled
- Consumer thread writes to disk (can be slower without blocking sensor reads)
- Ring buffer prevents blocking between threads
- Control thread handles socket commands independently

## Error Handling
- Ring buffer overflow: Oldest data is dropped (keeps latest)
- File write errors: Error message printed, program continues
- Device errors: Program stops gracefully
- Socket errors: Error messages printed, program continues

## Notes
- The ring buffer size (4 MB) provides several seconds of cushion for SD card stalls
- At 120 Hz with 8-byte samples, data rate is ~960 bytes/s
- At 10 kHz with 8-byte samples, data rate is ~80 KB/s
- Ring buffer can hold significant data even at high rates
- Chunk files are ~2 KB each at 120 Hz (240 samples × 8 bytes + header)
- Chunk files are ~160 KB each at 10 kHz (20000 samples × 8 bytes + header)

## Socket Location
The socket is created at `/run/sensor_ctrl.sock`. If `/run` doesn't exist (e.g., on some systems), the program will fall back to `./sensor_ctrl.sock` in the current directory.

## Graceful Shutdown
The program handles SIGINT and SIGTERM signals for graceful shutdown. Press Ctrl+C or send a kill signal to stop the program cleanly.

