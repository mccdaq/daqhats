#!/usr/bin/env python
"""
MCC 152 example program

Set DIO channel 0 as an input with interrupt on change enabled and send an IFTTT
event when a change occurs.

This uses the IFTTT Webhooks channel for inputs. You create your own event and
enter the name here for event_name (we used dio_trigger for the name), then
choose an activity to occur when the event happens. We used the Email activity
to send the time and DIO 0 value (Value0).

1. Sign in to your account at [https://ifttt.com](https://ifttt.com), select
"My Applets", and click the "New Applet" button.
2. Click "this" then search for Webhooks. Click the Webhooks icon when found.
3. On the "Choose trigger" screen, click "Receive a web request".
4. On the "Complete trigger fields" screen, enter an event name ("dio_trigger"
for this example) and click the **Create trigger** button.
5. Click "that", then search for "email" on the "Choose action service"
screen and select the **Send me an email** icon when found.
6. On the "Complete action fields" screen modify the Subject field so it
contains "DIO trigger occurred" (without quotes) and the body so it contains
"DIO trigger occurred at {{OccurredAt}} with value {{Value1}}."
(without quotes). Click the **Create action** button.
7. Review the summary statement, then click the **Finish** button.

Enter your key from IFTTT Webhooks documentation for the variable "key". You
can find this key by:

1. At IFTTT.com go to My Applets, then click the Services heading.
2. Enter Webhooks in the Filter services field, then click on Webhooks.
3. Click on Documentation. Your key will be listed at the top; copy that key
and paste it inside the quotes below, replacing <my_key>.
"""
from __future__ import print_function
import sys
from daqhats import mcc152, hat_list, HatIDs, wait_for_interrupt, DIOConfigItem
import requests

# IFTTT values
EVENT_NAME = "dio_trigger"
KEY = "<my_key>"

CHANNEL = 0

def send_trigger(event, value1="", value2="", value3=""):
    """ Send the IFTTT trigger. """
    report = {}
    report['value1'] = str(value1)
    report['value2'] = str(value2)
    report['value3'] = str(value3)
    requests.post("https://maker.ifttt.com/trigger/" + event + "/with/key/" +
                  KEY, data=report)

def main():
    """ Main function """

    if KEY == "<my_key>":
        print("The default key must be changed to the user's personal IFTTT "
              "Webhooks key before using this example.")
        sys.exit()

    # Find the first MCC 152
    mylist = hat_list(filter_by_id=HatIDs.MCC_152)
    if not mylist:
        print("No MCC 152 boards found")
        sys.exit()

    print("Using MCC 152 {}.".format(mylist[0].address))
    board = mcc152(mylist[0].address)

    # Reset the DIO to defaults (all channels input, pull-up resistors
    # enabled).
    board.dio_reset()

    # Read the initial input values so we don't trigger an interrupt when
    # we enable them.
    value = board.dio_input_read_bit(CHANNEL)

    # Enable latched input so we know that the value changed even if it changes
    # back to the original value before the interrupt callback.
    board.dio_config_write_bit(CHANNEL, DIOConfigItem.INPUT_LATCH, 1)

    # Unmask (enable) interrupts on the channel.
    board.dio_config_write_bit(CHANNEL, DIOConfigItem.INT_MASK, 0)

    print("Current input value is {}".format(value))
    print("Waiting for changes, Ctrl-C to exit. ")

    while True:
        # wait for a change
        wait_for_interrupt(-1)

        # a change occurred, verify it was from this channel before sending
        # a trigger
        status = board.dio_int_status_read_bit(CHANNEL)

        if status == 1:
            # Read the input to clear the active interrupt.
            value = board.dio_input_read_bit(CHANNEL)

            send_trigger(EVENT_NAME, "{}".format(value))
            print("Sent value {}.".format(value))

if __name__ == '__main__':
    main()
