# Finite Scan with Trigger Example

## About
This example demonstrates how to perform a triggered finite acquisition on 
one or more channels. 

This example waits for an external trigger to occur, and then acquires blocks
of analog input data for a user-specified group of channels. 

The last sample of data for each channel is displayed for each block of data 
received from the device. The acquisition is stopped when the specified number 
of samples is acquired for each channel, or a specified timeout value is 
reached.

This example is compiled and ready-to-run, and can be customized to suit 
your needs.

## Running the example
To run the example, open a terminal window and enter the following commands:
```sh
   cd ~/daqhats/examples/c/mcc118/finite_scan_with_trigger
   ./finite_scan_with_trigger
```

## Support/Feedback
Contact technical support through our
[support page](https://www.mccdaq.com/support/support_form.aspx).
