# Console-Based Examples

## About
The console-based examples demonstrate various classes and functions to 
familiarize yourself with the daqhats library and MCC 152 DAQ HAT board.
All examples are ready-to-run and can be customized to suit your needs.

>Additional information about each example is in the README file in the 
folder where the example is located.

## Example Programs
- **analog_output_write**: gets a user value and updates an analog output 
channel.

- **analog_output_write_all**: gets user values and updates both analog output 
channels.

- **digital_input_interrupt**: enables interrupts on all of the inputs, sets 
up a callback function, then waits for inputs to change and displays the 
changes.

- **digital_input_read_bit**: reads all of the digital inputs individually 
and displays the values.

- **digital_input_read_port**: reads all of the digital inputs in a single 
call and displays the values.

- **digital_output_write_bit**: sets all of the digital I/O to outputs, then 
gets a channel and value from the user and updates the specified output.

- **digital_output_write_port**: sets all of the digital I/O to outputs, then
 gets user values and updates all outputs in a single call.

## Running an Example
To run an example, open a terminal window in the folder where the example is 
located (the default path is /daqhats/examples/c/mcc152/) and enter the 
following command:

```
   ./<example_name>
```

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx).