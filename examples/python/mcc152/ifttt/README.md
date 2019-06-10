# Python IFTTT Example

## About
IFTTT (If This Then That) is a free web-based service that interacts with apps 
and hardware. The IFTTT service is used to automate tasks via applets that 
define services, events, and actions.

The Python IFTTT example sets up DIO channel 0 on the first MCC 152 as an input
with interrupt on change enabled. Whenever a change is detected on the input it
will send an IFTTT trigger that sends email.

Perform the procedures below to configure a new applet and run the IFTTT example.

## Prerequisites
- IFTTT account
- Google Drive account

## Create the IFTTT applet

1. Sign in to your account at [https://ifttt.com](https://ifttt.com), select
"My Applets", and click the **New Applet** button.
2. Click "this" then search for Webhooks. Click the Webhooks icon when found.
3. On the "Choose trigger" screen, click **Receive a web request**.
4. On the "Complete trigger fields" screen, enter an event name ("dio_trigger" 
for this example) and click the **Create trigger** button.
5. Click "that", then search for "email" on the "Choose action service"
screen and select the **Send me an email** icon when found.
6. On the "Complete action fields" screen modify the Subject field so it
contains "DIO trigger occurred" (without quotes) and the body so it contains 
"DIO trigger occurred at {{OccurredAt}}, current value {{Value1}}."
(without quotes). Click the **Create action** button.
7. Review the summary statement, then click the **Finish** button.

## Obtain the IFTTT WebHooks Documentation key
Update the variable "key" in the example program with the key from the IFTTT
Webhooks documentation for the variable "key".  You can find this key by:
1. At [https://ifttt.com](https://ifttt.com) click **My Applets** and then
**Services**.
2. Enter "Webhooks" in the search field, and select the **Webhooks** icon.
3. Click the **Documentation** button and copy the key listed at the top of the
page.
4. Open "ifttt_trigger.py", locate the "IFTTT values" section, and replace <my_key>
with the key copied in step 3; take care to retain the quotes around the key
value.
    ```python
    # IFTTT values
    event_name = "dio_trigger"
    key = "<my_key>"
    ```

## Start the IFTTT trigger service
1. Open ifttt_trigger.py from an IDE, or execute in a terminal window:  
   ```sh
   cd ~/daqhats/examples/python/mcc152/ifttt
   ./ifttt_trigger.py
   ```   
   
2. Shortly after a change occurs on DIO 0 you should receive an email from the
IFTTT service.

> If your applet fails to run, verify the trigger and action settings. 
> Some users may need to edit or update their connection settings to work with 
the Email service. This may be found by searching for Email in the **Services** 
section of **My Applets**, then selecting **Settings**.

## Support/Feedback
Contact technical support through our 
[support page](https://www.mccdaq.com/support/support_form.aspx). 

## More Information
- IFTTT: https://ifttt.com/discover
