#!/usr/bin/env python
"""
MCC 134 example program

Read channel 0 every 5 minutes and send an IFTTT event for logging to a
Google sheets spreadsheet. This uses the IFTTT Webhooks channel for inputs.
You create your own event and enter the name here for EVENT_NAME (we used
temperature_data for the name), then choose an activity to occur when the event
happens. We used the Google Sheets activity and selected "add a row to a
spreadsheet", then set it up to log the time and temperatures (Value0 and
Value1).

1. Sign in to your account at [https://ifttt.com](https://ifttt.com), select
"My Applets", and click the "New Applet" button.
2. Click "this" then search for Webhooks. Click the Webhooks icon when found.
3. On the "Choose trigger" screen, click "Receive a web request".
4. On the "Complete trigger fields" screen, enter an event name
("temperature_data" for this example) and click the "Create trigger" button.
5. Click "that", then search for "Google Sheets" on the "Choose action service"
screen and select the "Google Sheets" icon when found.
6. On the "Choose action" screen, select "Add row to spreadsheet".
7. On the "Complete action fields" screen, enter a spreadsheet name, modify the
"Formatted row" field so it contains "{{OccurredAt}} ||| {{Value1}}"
(without quotes). Change the Google Drive folder path if desired,
then click the "Create action" button.
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
from daqhats import mcc134, hat_list, HatIDs, TcTypes
import requests

# IFTTT values
EVENT_NAME = "temperature_data"
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
    channel = 0
    tc_type = TcTypes.TYPE_T

    if KEY == "<my_key>":
        print("The default key must be changed to the user's personal IFTTT "
              "Webhooks key before using this example.")
        sys.exit()

    # Find the first MCC 134
    mylist = hat_list(filter_by_id=HatIDs.MCC_134)
    if not mylist:
        print("No MCC 134 boards found")
        sys.exit()

    board = mcc134(mylist[0].address)

    # Configure the thermocouple type on the desired channel
    board.tc_type_write(channel, tc_type)

    # Set library update interval to a longer time since we are not reading
    # often.
    if log_period > 255:
        board.update_interval_write(60)
    else:
        board.update_interval_write(log_period)

    print("Logging temperatures, Ctrl-C to exit.")

    while True:
        # read the temperature
        temperature = board.t_in_read(channel)

        # check for errors
        if temperature == mcc134.OPEN_TC_VALUE:
            temp_val = "Open"
        elif temperature == mcc134.OVERRANGE_TC_VALUE:
            temp_val = "Overrange"
        elif temperature == mcc134.COMMON_MODE_TC_VALUE:
            temp_val = "Common mode"
        else:
            temp_val = "{:.2f}".format(temperature)

        send_trigger(EVENT_NAME, temp_val)

        time.sleep(log_period)

if __name__ == '__main__':
    main()
