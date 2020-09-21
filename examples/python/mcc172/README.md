# Console-Based Examples

## About
The console-based examples demonstrate various classes and methods to
familiarize yourself with the daqhats library and MCC 172 DAQ HAT board. All
examples are ready-to-run and can be customized to suit your needs.

## Example Programs
- **continuous_scan**: continuously acquires blocks of analog input data from 
  specified channels until the scan is stopped. 

- **fft_scan**: acquires blocks of analog data from both channels, performs
  FFTs on the data, finds the peak frequency and harmonics, and saves the data
  and FFT to CSV files.  This example requires the NumPy library; if it is not
  installed you can install it with:
  ```sh
  sudo pip install numpy
  ```

- **finite_scan**: acquires a block of analog input data from user-specified 
  channels.

- **finite_scan_with_trigger**: waits for an external trigger to occur, and 
  then acquires blocks of analog input data for a user-specified group of 
  channels.

- **multi_hat_synchronous_scan**: acquires synchronous data from up to 
  eight MCC 172 HATs using the shared clock and trigger scan.
  One MCC 172 HAT (**master** device) provides the clock for synchronous acquisition.

  Wire the MCC 172 HATs as listed below to synchronously acquire data:
  * Stack the MCC 172 HATs onto the Pi per the documentation.
  * Connect an external trigger source to the **TRIG** terminal on the MCC 172
    master device.
  * Configure the source types to SLAVE for the trigger and clock on each MCC 172
    HAT except the master device.
  * Configure the source types to MASTER for the trigger and clock on the master
    MCC 172 device.
  * Set the **EXTTRIGGER** scan option on all devices.

  All channels will now be converting synchronously. When triggered, the data
  will be read by the devices.

  > Refer to **a_in_scan_start()**, **a_in_clock_config_write()**, and
    **trigger_config()** in the documentation for information about shared clock
    and trigger.

## Running an Example
To run an example, open a terminal window in the folder where the example is 
located (the default path is /daqhats/examples/python/mcc172/) and enter the 
following command:

```
./<example_name>.py
```

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).
