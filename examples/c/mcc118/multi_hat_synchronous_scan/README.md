# Multi-HAT Synchronous Scan Example

## About
This example demonstrates how to acquire synchronous data from multiple 
MCC 118 HATs using the external clock and external trigger scan options.
One MCC 118 HAT is used to pace the synchronous acquisition.

The current sample read from each scan, the total count of samples read 
for the acquisition, and the current value of each active channel display
for each device.

The acquisition is stopped when the specified number of samples are acquired, 
the Enter key is pressed, a read timeout error occurs, or an overrun error
occurs. 

This example is compiled and ready-to-run, and can be customized to suit 
your needs.

## Configuring the MCC 118 HATs for a Synchronous Acquisition
Perform the following procedure to configure a stack of MCC 118 HATs to 
synchronously acquire data.

> **Note**: The board stack may contain up to eight MCC 118 HATs. 
Designate one MCC 118 as the master device.

1. Stack the MCC 118 HATs onto the Rasberry Pi; refer to 
[Installing the HAT board](https://www.mccdaq.com/PDFs/Manuals/DAQ-HAT/hardware.html)
for instructions.
2. Wire the **CLK** terminals together on each MCC 118 HAT board.
3. Connect an external trigger source to the **TRIG** terminal on the MCC 118 
master device.
4. Set the **OPTS_EXTCLOCK** scan option on each MCC 118 HAT except the master device.
5. Set the **OPTS_EXTTRIGGER** scan option on the MCC 118 master.

When triggered, the master device outputs the sample clock to the other MCC 118
HATs. The acquisition begins when the MCC 118 HATs receive the clock signal.

> Refer to 
[mcc118_a_in_scan_start()](https://www.mccdaq.com/PDFs/Manuals/DAQ-HAT/c.html#c.mcc118_a_in_scan_start) 
for information about the OPTS_EXTCLOCK and OPTS_EXTTRIGGER scan options.

## Running the example
To run the example, open a terminal window and enter the following commands:
```sh
   cd ~/daqhats/examples/c/mcc118/multi_hat_synchronous_scan
   ./multi_hat_synchronous_scan
```

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).
