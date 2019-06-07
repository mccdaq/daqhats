#!/usr/bin/env python
"""
    MCC 152 Functions / Methods Demonstrated:
        mcc152.dio_reset
        mcc152.dio_config_write_port
        mcc152.dio_input_read_port
        mcc152.dio_int_status_read_port
        mcc152.info
        interrupt_callback_enable
        interrupt_callback_disable

    Purpose:
        Configure the inputs to interrupt on change then wait for changes.

    Description:
        This example demonstrates using the digital I/O as inputs and enable
        interrupts on change.  It waits for changes on any input and displays
        the change.
"""
from __future__ import print_function
from sys import version_info
from daqhats import mcc152, HatIDs, DIOConfigItem, \
    interrupt_callback_enable, HatCallback, interrupt_callback_disable
from daqhats_utils import select_hat_device

# Use a global variable for our board object so it is accessible from the
# interrupt callback.
HAT = None

def interrupt_callback(user_data):
    """
    This function is called when a DAQ HAT interrupt occurs.
    """
    print("Interrupt number {}".format(user_data[0]))
    user_data[0] += 1

    # An interrupt occurred, make sure this board was the source.
    status = HAT.dio_int_status_read_port()

    if status != 0:
        i = HAT.info().NUM_DIO_CHANNELS
        print("Input channels that changed: ", end="")
        for i in range(8):
            if (status & (1 << i)) != 0:
                print("{} ".format(i), end="")

        # Read the inputs to clear the active interrupt.
        value = HAT.dio_input_read_port()

        print("\nCurrent port value: 0x{:02X}".format(value))

    return

def main():
    """
    This function is executed automatically when the module is run directly.
    """

    print("MCC 152 digital input interrupt example.")
    print("Enables interrupts on the inputs and displays their state when")
    print("they change.")
    print("   Functions / Methods demonstrated:")
    print("      mcc152.dio_reset")
    print("      mcc152.dio_config_write_port")
    print("      mcc152.dio_input_read_port")
    print("      mcc152.dio_int_status_read_port")
    print("      mcc152.info")
    print("      interrupt_callback_enable")
    print("      interrupt_callback_disable")
    print()

    # Get an instance of the selected HAT device object.
    address = select_hat_device(HatIDs.MCC_152)

    print("\nUsing address {}.\n".format(address))

    global HAT  # pylint: disable=global-statement

    HAT = mcc152(address)

    # Reset the DIO to defaults (all channels input, pull-up resistors
    # enabled).
    HAT.dio_reset()

    # Read the initial input values so we don't trigger an interrupt when
    # we enable them.
    value = HAT.dio_input_read_port()

    # Enable latched inputs so we know that a value changed even if it changes
    # back to the original value before the interrupt callback.
    HAT.dio_config_write_port(DIOConfigItem.INPUT_LATCH, 0xFF)

    # Unmask (enable) interrupts on all channels.
    HAT.dio_config_write_port(DIOConfigItem.INT_MASK, 0x00)

    print("Current input values are 0x{:02X}".format(value))
    print("Waiting for changes, enter any text to exit. ")

    # Create a HAT callback object for our function
    callback = HatCallback(interrupt_callback)

    # Enable the interrupt callback function. Provide a mutable value for
    # user_data that counts the number of interrupt occurrences.

    int_count = [0]
    interrupt_callback_enable(callback, int_count)

    # Wait for the user to enter anything, then exit.
    if version_info.major > 2:
        input("")
    else:
        raw_input("")

    # Return the digital I/O to default settings.
    HAT.dio_reset()

    # Disable the interrupt callback.
    interrupt_callback_disable()

if __name__ == '__main__':
    # This will only be run when the module is called directly.
    main()
