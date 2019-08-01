# Multi-HAT Synchronous Scan Example

## About
This example demonstrates how to acquire synchronous data from multiple 
MCC 172 HATs using the shared clock and trigger options.
One MCC 172 HAT is used as the master clock and trigger source.

The current number of samples read from each scan, the total count of samples read 
for the acquisition, and the current RMS voltage of each active channel are displayed
for each device.

The acquisition is stopped when the specified number of samples are acquired, 
the Enter key is pressed, or an error occurs. 

This example is compiled and ready-to-run, and can be customized to suit 
your needs.

## Configuring the MCC 172 HATs for a Synchronous Acquisition
Perform the following procedure to configure a stack of MCC 172 HATs to 
synchronously acquire data.

> **Note**: The board stack may contain up to eight MCC 172 HATs. 
Designate one MCC 172 as the master device.

1. Stack the MCC 172 HATs onto the Raspberry Pi per the DAQ HAT documentation.
2. Connect an external trigger source to the **TRIG** terminal on the MCC 172 
master device.
3. Set the clock and trigger sources to **SOURCE_SLAVE** on each MCC 172 HAT 
except the master device.
4. Set the clock and trigger sources to **SOURCE_MASTER** on the master MCC 172 
device.
5. Set the **OPTS_EXTTRIGGER** scan option on all devices.

The ADCs on all devices will now be converting synchronously.  When triggered, 
all devices will beging reading the synchronous data.

> Refer to the documentation for **mcc172_a_in_scan_start()**, **mcc172_a_in_clock_config_write()**,
and **mcc172_trigger_config()** for information about configuring the clock and trigger.

## Running the example
To run the example, open a terminal window and enter the following commands:
```sh
cd ~/daqhats/examples/c/mcc172/multi_hat_synchronous_scan
./multi_hat_synchronous_scan
```

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).
