#!/usr/bin/env python3
"""
Convert all .bin files in DAD_Files directory to CSV format.

Usage:
    python3 convert_all_bin_to_csv.py
"""

import os
import glob
import struct
from datetime import datetime

# Binary file format constants
MAGIC = b"SDAT"

def convert_bin_to_csv(bin_file):
    """Convert a single .bin file to CSV."""
    csv_file = os.path.splitext(bin_file)[0] + ".csv"
    
    # Skip if CSV already exists
    if os.path.exists(csv_file):
        print(f"Skipping {os.path.basename(bin_file)} (CSV already exists)")
        return True
    
    try:
        with open(bin_file, 'rb') as f:
            # Read header
            magic = f.read(4)
            if magic != MAGIC:
                print(f"Error: Invalid file format in {bin_file}")
                return False
            
            version = struct.unpack('<H', f.read(2))[0]
            device_id = struct.unpack('<I', f.read(4))[0]
            boot_id = struct.unpack('<Q', f.read(8))[0]
            seq_start = struct.unpack('<Q', f.read(8))[0]
            sample_rate = struct.unpack('<I', f.read(4))[0]
            record_size = struct.unpack('<H', f.read(2))[0]
            sample_count = struct.unpack('<I', f.read(4))[0]
            sensor_time_start = struct.unpack('<Q', f.read(8))[0]
            sensor_time_end = struct.unpack('<Q', f.read(8))[0]
            payload_crc32 = struct.unpack('<I', f.read(4))[0]
            
            # Read payload (samples)
            samples = []
            for i in range(sample_count):
                sample = struct.unpack('<d', f.read(8))[0]
                samples.append(sample)
            
            # Write CSV file
            with open(csv_file, 'w') as csv:
                csv.write("Sample_Number,Time_Seconds,Channel_4_Voltage\n")
                
                time_per_sample = 1.0 / sample_rate
                for i, voltage in enumerate(samples):
                    time_seconds = i * time_per_sample
                    csv.write(f"{i+1},{time_seconds:.6f},{voltage:.6f}\n")
            
            print(f"[OK] Converted: {os.path.basename(bin_file)} -> {os.path.basename(csv_file)} "
                  f"({sample_count} samples, {sample_count/sample_rate:.2f}s)")
            return True
            
    except Exception as e:
        print(f"[ERROR] Error converting {bin_file}: {e}")
        return False

def main():
    # Get the directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    dad_files_dir = os.path.join(script_dir, "DAD_Files")
    
    if not os.path.exists(dad_files_dir):
        print(f"Error: Directory not found: {dad_files_dir}")
        return 1
    
    # Find all .bin files
    bin_files = glob.glob(os.path.join(dad_files_dir, "*.bin"))
    
    if not bin_files:
        print(f"No .bin files found in {dad_files_dir}")
        return 1
    
    print(f"Found {len(bin_files)} .bin file(s) in DAD_Files/")
    print("=" * 60)
    
    converted = 0
    failed = 0
    skipped = 0
    
    for bin_file in sorted(bin_files):
        csv_file = os.path.splitext(bin_file)[0] + ".csv"
        if os.path.exists(csv_file):
            skipped += 1
            continue
        
        if convert_bin_to_csv(bin_file):
            converted += 1
        else:
            failed += 1
    
    print("=" * 60)
    print(f"Conversion complete!")
    print(f"  Converted: {converted}")
    print(f"  Skipped (already exists): {skipped}")
    print(f"  Failed: {failed}")
    print(f"  Total: {len(bin_files)}")
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    import sys
    sys.exit(main())

