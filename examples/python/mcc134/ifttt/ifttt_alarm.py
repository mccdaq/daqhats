#!/usr/bin/env python
"""
MCC 134 example program

Continuously read channel 0 and send an IFTTT trigger if the temperature exceeds
a threshold or when it drops back below the threshold.

This uses the IFTTT Webhooks channel for inputs. You create your own event and
enter the name here for EVENT_NAME (we used temperature_alarm for the name),
then choose an activity to occur when the event happens. We used the Email
activity to send the time, state, temperature, and alarm threshold.

1. Sign in to your account at [https://ifttt.com](https://ifttt.com), select
"My Applets", and click the "New Applet" button.
2. Click "this" then search for Webhooks. Click the Webhooks icon when found.
3. On the "Choose trigger" screen, click "Receive a web request".
4. On the "Complete trigger fields" screen, enter an event name
("temperature_alarm" for this example) and click the **Create trigger** button.
5. Click "that", then search for "email" on the "Choose action service"
screen and select the **Send me an email** icon when found.
6. On the "Complete action fields" screen modify the Subject field so it
contains "Temperature alarm occurred" (without quotes) and the body so it
contains "Temperature alarm {{Value3}} at {{OccurredAt}}. Temp: {{Value1}},
Threshold: {{Value2}}" (without quotes). Click the **Create action** button.
7. Review the summary statement, then click the **Finish** button.

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

# Alarm type - rising or falling
ALARM_RISING = True

# Temperature thresholds - use a bit of hysteresis so you don't get a lot of
# events if the temperature is hovering right around the threshold
ALARM_THRESHOLD = 27.0
CLEAR_THRESHOLD = 26.5

CHANNEL = 0

# IFTTT values
EVENT_NAME = "temperature_alarm"
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

    # Configure for type T thermocouple
    board.tc_type_write(CHANNEL, TcTypes.TYPE_T)

    # Read and display current temperature
    temperature = board.t_in_read(CHANNEL)
    print("Current temperature: {:.2f}.".format(temperature))
    print("Monitoring temperature, Ctrl-C to exit. ")

    alarm_set = False
    error_value = 0.0

    while True:
        # read the temperature
        temperature = board.t_in_read(CHANNEL)

        # check for errors
        if temperature == mcc134.OPEN_TC_VALUE:
            if error_value != temperature:
                error_value = temperature
                send_trigger(EVENT_NAME, "{:.0f}".format(temperature),
                             "{:.2f}".format(CLEAR_THRESHOLD), "Open thermocouple")
                print("Open thermocouple.")
        elif temperature == mcc134.OVERRANGE_TC_VALUE:
            if error_value != temperature:
                error_value = temperature
                send_trigger(EVENT_NAME, "{:.0f}".format(temperature),
                             "{:.2f}".format(CLEAR_THRESHOLD), "Overrange")
                print("Overrange.")
        elif temperature == mcc134.COMMON_MODE_TC_VALUE:
            if error_value != temperature:
                error_value = temperature
                send_trigger(EVENT_NAME, "{:.0f}".format(temperature),
                             "{:.2f}".format(CLEAR_THRESHOLD), "Common mode error")
                print("Common mode error.")
        else:
            # compare against the thresholds; change the logic if using falling
            # temperature alarm
            if alarm_set:
                if ((ALARM_RISING and (temperature < CLEAR_THRESHOLD)) or
                        (not ALARM_RISING and (temperature > CLEAR_THRESHOLD))):
                    # we crossed the clear threshold, send a trigger
                    alarm_set = False
                    send_trigger(EVENT_NAME, "{:.2f}".format(temperature),
                                 "{:.2f}".format(CLEAR_THRESHOLD), "cleared")
                    print("Temp: {:.2f}, alarm cleared.".format(temperature))
            else:
                if ((ALARM_RISING and (temperature >= ALARM_THRESHOLD)) or
                        (not ALARM_RISING and (temperature <= ALARM_THRESHOLD))):
                    # we crossed the alarm threshold, send a trigger
                    alarm_set = True
                    send_trigger(EVENT_NAME, "{:.2f}".format(temperature),
                                 "{:.2f}".format(ALARM_THRESHOLD), "set")
                    print("Temp: {:.2f}, alarm set.".format(temperature))

        time.sleep(1)

if __name__ == '__main__':
    main()
