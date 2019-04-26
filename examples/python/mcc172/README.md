# Console-Based Examples

## About
The console-based examples demonstrate various classes and methods to
familiarize yourself with the daqhats library and MCC 118 DAQ HAT board. All
examples are ready-to-run and can be customized to suit your needs.

## Example Programs
- **continuous_scan**: continuously acquires blocks of analog input data from 
specified channels until the scan is stopped. The last sample of data
for each channel is displayed for each block of data received from the 
device. The acquisition stops when the specified number of samples is 
acquired for each channel or the Enter key is pressed. 

- **finite_scan**: acquires a block of analog input data from user-specified 
channels. The last sample of data for each channel is displayed for each block 
of data received from the device. The acquisition stops when the specified 
number of samples is acquired for each channel.

- **finite_scan_with_trigger**: waits for an external trigger to occur, and 
then acquires blocks of analog input data for a user-specified group of 
channels. The last sample of data for each channel is displayed for each 
block of data received from the device. The acquisition is stopped when the 
specified number of samples is acquired for each channel or a specified 
timeout value is reached.

- **multi_hat_synchronous_scan**: acquires synchronous data from up to 
eight MCC 118 HATs using the external clock and external trigger scan options.
One MCC 118 HAT (**master** device) is used to pace the synchronous acquisition.

    The current sample read from each scan, the total count of samples read 
for the acquisition, and the current value of each active channel display
for each device. The acquisition stops when the specified number of samples 
are acquired, the Enter key is pressed, a read timeout error occurs, or an 
overrun error occurs. 

  Wire the MCC 118 HATs as listed below to synchronously acquire data:
  * Stack the MCC 118 HATs onto the Pi using the instructions in 
  [Installing the HAT board](https://www.mccdaq.com/PDFs/Manuals/DAQ-HAT/hardware.html).
  * Wire the **CLK** terminals together on each MCC 118 HAT.
  * Connect an external trigger source to the **TRIG** terminal on the MCC 118 
master device.
  * Set the **EXTCLOCK** scan option on each MCC 118 HAT except the master 
  device.
  * Set the **EXTTRIGGER** scan option on the MCC 118 master.

   When triggered, the master device outputs the sample clock to the other 
   MCC 118 HATs. The acquisition begins when the MCC 118 HATs receive the 
   clock signal.

    > Refer to [a_in_scan_start()](https://www.mccdaq.com/PDFs/Manuals/DAQ-HAT//python.html#daqhats.mcc118.a_in_scan_start) 
for information about the EXTCLOCK and EXTTRIGGER scan options.

- **single_value_read**: reads and displays a single data value for each 
selected channel on each iteration of a software timed loop.

## Running an Example
To run an example, open a terminal window in the folder where the example is 
located (the default path is /daqhats/examples/python/mcc118/) and enter the 
following command:

```
   ./<example_name>.py
```

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).