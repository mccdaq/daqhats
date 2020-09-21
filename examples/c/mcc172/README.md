# C/C++ Examples

## About
The C/C++ examples demonstrate various classes and functions to familiarize 
yourself with the daqhats library and MCC 172 DAQ HAT board. All examples can
be customized to suit your needs.

>Additional information about each example is in the README file in the 
folder where the example is located.

## Example Programs

### Console-Based
- **continuous_scan**: continuously acquires blocks of analog input data from 
specified channels until the scan is stopped.

- **fft_scan**: acquires a block of analog data from a single channel, performs
an FFT on the data, finds the peak frequency and harmonics, and saves the data
and FFT to a CSV file.

- **finite_scan**: acquires a block of analog input data from user-specified 
channels.

- **finite_scan_with_trigger**: waits for an external trigger to occur, and 
then acquires blocks of analog input data for a user-specified group of 
channels.

- **multi_hat_synchronous_scan**: acquires synchronous data from up to 
eight MCC 172 HATs using shared clock and trigger options. Refer to the 
README file in the multi_hat_synchronous_scan example folder for more 
information.

### User Interface
- **data_logger**: acquires and displays data on a strip chart, 
calculates and displays FFT data, and logs the 
data to a CSV file. Refer to the README file in the data_logger example 
folder for more information.

## Running an Example
To run a console example, open a terminal window in the folder where the 
example is located (the default path is /daqhats/examples/c/mcc172/) and 
enter the following command:

```
   ./<example name>
```
The data_logger example requires a monitor connected to the **Raspberry Pi**; 
refer to the README in the data_logger example folder for instructions.

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).