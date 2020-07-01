#!/usr/bin/env python
"""
MCC 128 example program

Read channels 0 and 1 every 5 minutes and send an IFTTT event for logging to a
Google sheets spreadsheet. This uses the IFTTT Webhooks channel for inputs.
You create your own event and enter the name here for event_name (we used
voltage_data for the name), then choose an activity to occur when the event
happens. We used the Google Sheets activity and selected "add a row to a
spreadsheet", then set it up to log the time and voltages (Value0 and Value1).

1. Sign in to your account at [https://ifttt.com](https://ifttt.com), select
"My Applets", and click the "New Applet" button.
2. Click "this" then search for Webhooks. Click the Webhooks icon when found.
3. On the "Choose trigger" screen, click "Receive a web request".
4. On the "Complete trigger fields" screen, enter an event name ("voltage_data"
for this example) and click the "Create trigger" button.
5. Click "that", then search for "Google Sheets" on the "Choose action service"
screen and select the "Google Sheets" icon when found.
6. On the "Choose action" screen, select "Add row to spreadsheet".
7. On the "Complete action fields" screen, enter a spreadsheet name, modify the
"Formatted row" field so it contains "{{OccurredAt}} ||| {{Value1}} ||| {{Value2}}"
(without quotes). Change the Google Drive folder path if desired, then
click the "Create action" button.
8. Review the summary statement, then click the **Finish** button.

Enter your key from IFTTT Webhooks documentation for the variable "key". You
can find this key by:

1. At IFTTT.com go to My Applets, then click the Services heading.
2. Enter Webhooks in the Filter services field, then click on Webhooks.
3. Click on Documentation. Your key will be listed at the top; copy that key
and paste it inside the quotes below, replacing <my_key>.
"""
from __future__ import print_function
import time
import sys
import daqhats as hats
import requests

# IFTTT values
EVENT_NAME = "voltage_data"
KEY = "<my_key>"

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
    log_period = 5*60

    if KEY == "<my_key>":
        print("The default key must be changed to the user's personal IFTTT "
              "Webhooks key before using this example.")
        sys.exit()

    # Find the first MCC 128
    mylist = hats.hat_list(filter_by_id=hats.HatIDs.MCC_128)
    if not mylist:
        print("No MCC 128 boards found")
        sys.exit()

    board = hats.mcc128(mylist[0].address)

    board.a_in_mode_write(hats.AnalogInputMode.SE)
    board.a_in_range_write(hats.AnalogInputRange.BIP_10V)

    while True:
        # read the voltages
        value_0 = board.a_in_read(0)
        value_1 = board.a_in_read(1)

        send_trigger(EVENT_NAME, "{:.3f}".format(value_0),
                     "{:.3f}".format(value_1))
        print("Sent data {0:.3f}, {1:.3f}.".format(value_0, value_1))
        time.sleep(log_period)

if __name__ == '__main__':
    main()
