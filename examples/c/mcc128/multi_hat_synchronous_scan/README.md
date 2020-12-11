# Multi-HAT Synchronous Scan Example

## About
This example demonstrates how to acquire synchronous data from multiple
MCC 128 HATs using the external clock and external trigger scan options.
One MCC 128 HAT is used to pace the synchronous acquisition.

The current sample count read from each scan, the total count of samples read
for the acquisition, and the current value of each active channel are displayed
for each device.

The acquisition is stopped when the specified number of samples are acquired,
the Enter key is pressed, or an error occurs.

This example is compiled and ready-to-run, and can be customized to suit
your needs.

## Configuring the MCC 128 HATs for a Synchronous Acquisition
Perform the following procedure to configure a stack of MCC 128 HATs to
synchronously acquire data.

> **Note**: The board stack may contain up to eight MCC 128 HATs.
Designate one MCC 128 as the master device.

1. Stack the MCC 128 HATs onto the Rasberry Pi per the DAQ HAT documentation.
2. Wire the **CLK** terminals together on each MCC 128 HAT board.
3. Connect an external trigger source to the **TRIG** terminal on the MCC 128
master device.
4. Set the **OPTS_EXTCLOCK** scan option on each MCC 128 HAT except the master device.
5. Set the **OPTS_EXTTRIGGER** scan option on the MCC 128 master.

When triggered, the master device outputs the sample clock to the other MCC 128
HATs. The acquisition begins when the MCC 128 HATs receive the clock signal.

> Refer to the documentation for **mcc128_a_in_scan_start()** for information
about the OPTS_EXTCLOCK and OPTS_EXTTRIGGER scan options.

## Running the example
To run the example, open a terminal window and enter the following commands:
```sh
cd ~/daqhats/examples/c/mcc128/multi_hat_synchronous_scan
./multi_hat_synchronous_scan
```

## Support/Feedback
Contact technical support through our
[support page](https://www.mccdaq.com/support/support_form.aspx).
