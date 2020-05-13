# Python IFTTT Example

## About
IFTTT (If This Then That) is a free web-based service that interacts with apps
and hardware. The IFTTT service is used to automate tasks via applets that
define services, events, and actions.

The Python IFTTT example reads two MCC 128 channels every 5 minutes, and writes
the data to a Google Sheets spreadsheet. Users can remotely monitor the
spreadsheet with Google Drive.

Perform the procedures below to configure a new applet and run the IFTTT example.

## Prerequisites
- IFTTT account
- Google Drive account

## Create the IFTTT applet

1. Sign in to your account at [https://ifttt.com](https://ifttt.com), select
"My Applets", and click the **New Applet** button.
2. Click "this" then search for Webhooks. Click the Webhooks icon when found.
3. On the "Choose trigger" screen, click **Receive a web request**.
4. On the "Complete trigger fields" screen, enter an event name ("voltage_data"
for this example) and click the **Create trigger** button.
5. Click "that", then search for "Google Sheets" on the "Choose action service"
screen and select the **Google Sheets** icon when found.
6. On the "Choose action" screen, select **Add row to spreadsheet**.
7. On the "Complete action fields" screen, enter a spreadsheet name, modify the
"Formatted row" field so it contains "{{OccurredAt}} ||| {{Value1}} ||| {{Value2}}"
(without quotes). Change the Google Drive folder path if desired, then
click the **Create action** button.
8. Review the summary statement, then click the **Finish** button.

## Obtain the IFTTT WebHooks Documentation key
Update the variable "key" in the example program with the key from the IFTTT
Webhooks documentation for the variable "key".  You can find this key by:
1. At [https://ifttt.com](https://ifttt.com) click **My Applets** and then
**Services**.
2. Enter "Webhooks" in the search field, and select the **Webhooks** icon.
3. Click the **Documentation** button and copy the key listed at the top of the
page.
4. Open "ifttt_log.py", locate the "IFTTT values" section, and replace <my_key>
with the key copied in step 3; take care to retain the quotes around the key
value.
    ```python
    # IFTTT values
    event_name = "voltage_data"
    key = "<my_key>"
    ```

## Start the IFTTT logging service
1. Open ifttt_log.py from an IDE, or execute in a terminal window:
   ```sh
   cd ~/daqhats/examples/python/mcc128/ifttt
   ./ifttt_log.py
   ```

2. Launch Google Drive on the web; log into your account if you are not already
signed in.
3. Open the Google Sheets file named **voltage_data** in the path specified when
you created the applet. The spreadsheet dynamically updates with data as it is
acquired.

> If your applet fails to run, verify the trigger and action settings.
> Some users may need to edit or update their connection settings to work with
the Google Sheets service. This may be found by searching for Google Sheets in
the **Services** section of **My Applets**, then selecting **Settings**.

## Support/Feedback
Contact technical support through our
[support page](https://www.mccdaq.com/support/support_form.aspx).

## More Information
- IFTTT: https://ifttt.com/discover
