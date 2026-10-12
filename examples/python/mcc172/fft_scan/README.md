# FFT Scan Example

## About
The Python FFT scan example demonstrates acquiring blocks of analog input data from 
both channels, performing FFTs on the data, finding the peak frequency and harmonics,
and saving the data and FFT to CSV files in the current directory
(fft_scan_<channel>.csv).

## Dependencies
- NumPy: Python library for scientific computing and numerical data analysis

Enter the following commands from this directory to create a Python virtual environment
and install the dependencies and daqhats library (assumes daqhats is installed in
~/daqhats, modify the command if you use another location).

   ```
   python -m venv ~/daqhats-venv
   ~/daqhats-venv/bin/pip install ~/daqhats
   ~/daqhats-venv/bin/pip install -r requirements.txt
   ```

## Running an Example
To run the example, open a terminal window in the folder where the example is 
located (the default path is ~/daqhats/examples/python/mcc172/fft_scan) and enter the 
following command:

```
~/daqhats-venv/bin/python ~/daqhats/examples/python/mcc172/fft_scan/fft_scan.py
```

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).
