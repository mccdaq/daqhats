# Channel 4 Ring Buffer Logger

## Overview
This program acquires data from **Channel 4** of the MCC 118 HAT board using a **ring buffer architecture** with two threads:
- **Producer thread**: Reads from sensor at 4 kHz and writes to ring buffer
- **Consumer thread**: Reads from ring buffer and writes to binary chunk files

## Features
- **Scan rate**: 4 kHz (4000 samples/second)
- **Chunk duration**: 2 seconds (8000 samples per chunk)
- **Ring buffer**: 4 MB circular buffer to decouple reading from writing
- **File format**: Binary format with header (as per specification)
- **Atomic writes**: Files written as `.bin.part` then renamed to `.bin` when complete

## Architecture

### Ring Buffer
- **Size**: 4 MB (can hold ~500,000 samples)
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
- `sample_rate_hz` (uint32): 4000
- `record_size` (uint16): 8 (sizeof(double))
- `sample_count` (uint32): Number of samples in chunk
- `sensor_time_start` (uint64): Timestamp
- `sensor_time_end` (uint64): Timestamp
- `payload_crc32` (uint32): CRC32 (currently 0)

**Payload**:
- `sample_count` × `record_size` bytes of raw sample data (doubles)

## Compilation
```bash
cd ~/daqhats/tol_data_c
make
```

## Running
```bash
./channel4_ringbuffer_logger
```

The program will:
1. Prompt you to press ENTER to start
2. Begin acquisition (producer thread reads from sensor)
3. Write chunk files every 2 seconds to `tol_data_c/DAD_Files/`
4. Press ENTER again to stop

## Output Files
Files are saved to: `tol_data_c/DAD_Files/`

**File naming format**: `chunk_<boot_id>_<seq_start>.bin`

Example: `chunk_0123456789abcdef_0000000000000000.bin`

- Files are written as `.bin.part` during writing
- Automatically renamed to `.bin` when complete (atomic operation)
- Python uploader should only process `.bin` files, never `.part` files

## Thread Safety
- Producer thread has higher priority (reads sensor continuously)
- Consumer thread writes to disk (can be slower without blocking sensor reads)
- Ring buffer prevents blocking between threads

## Error Handling
- Ring buffer overflow: Oldest data is dropped (keeps latest)
- File write errors: Error message printed, program continues
- Device errors: Program stops gracefully

## Notes
- The ring buffer size (4 MB) provides several seconds of cushion for SD card stalls
- At 4 kHz with 8-byte samples, data rate is ~32 KB/s
- Ring buffer can hold ~125 seconds of data
- Chunk files are ~64 KB each (8000 samples × 8 bytes + header)
