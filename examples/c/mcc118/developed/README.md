# Channel 4 Data Logger

## About
This program acquires data from **Channel 4** of the MCC 118 HAT board:
- **Scan rate**: 10 kHz (10,000 samples/second)
- **Duration**: 10 seconds
- **Total samples**: 100,000 samples
- **Output**: CSV file with timestamp in filename

## CSV File Format
The CSV file contains three columns:
1. `Sample_Number`: Sequential sample number (1, 2, 3, ...)
2. `Time_Seconds`: Time in seconds from start of acquisition
3. `Channel_4_Voltage`: Voltage reading from channel 4 in Volts

Example:
```csv
Sample_Number,Time_Seconds,Channel_4_Voltage
1,0.000000,2.345678
2,0.000100,2.345789
3,0.000200,2.345890
...
```

## Compilation
```bash
cd ~/daqhats/examples/c/mcc118/developed
make
```

## Running
```bash
./channel4_logger
```

## Output
The program creates a CSV file with a timestamp in the filename:
- Format: `channel4_data_YYYYMMDD_HHMMSS.csv`
- Example: `channel4_data_20241215_143022.csv`
- Location: Same directory as the executable

## Notes
- The program will prompt you to press ENTER before starting acquisition
- Progress is shown with dots (.) every 10,000 samples
- The file is automatically saved when acquisition completes
- If an error occurs, the program will display an error message and exit

