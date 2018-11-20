# C/C++ Examples

## About
The C/C++ examples demonstrate various classes and functions to familiarize 
yourself with the daqhats library and MCC 118 DAQ HAT board. All examples can
be customized to suit your needs.

>Additional information about each example is in the README file in the 
folder where the example is located.

## Example Programs

### Console-Based
- **continuous_scan**: continuously acquires blocks of analog input data from 
specified channels until the scan is stopped.

- **finite_scan**: acquires a block of analog input data from user-specified 
channels.

- **finite_scan_with_trigger**: waits for an external trigger to occur, and 
then acquires blocks of analog input data for a user-specified group of 
channels.

- **multi_hat_synchronous_scan**: acquires synchronous data from up to 
eight MCC 118 HATs using external clock and trigger options. Refer to the 
README file in the multi_hat_synchronous_scan example folder for more 
information.

- **single_value_read**: reads and displays a single data value for each 
selected channel on each iteration of a software timed loop.

### User Interface
- **data_logger**: acquires and displays data on a strip chart, and logs the 
data to a CSV file. This example can be run from a terminal window or 
accessed with an IDE. Refer to the README file in the data_logger example 
folder for more information.

## Running an Example
To run a console example, open a terminal window in the folder where the 
example is located (the default path is /daqhats/examples/c/mcc118/) and 
enter the following command:

```
   ./<example name>
```
You can run the data_logger example from a terminal or IDE; refer to the 
README in the data_logger example folder for instructions.

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).