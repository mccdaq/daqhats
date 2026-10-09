# Python Web Server Example 

## About
The Python web server example demonstrates a simple web server that displays 
data acquired from the MCC 118 HAT. This example is intended to run on a 
single client.

## Dependencies
- Dash: Python framework for building Web-based applications
- Plotly: an interactive, browser-based graphing library for Python

Enter the following commands from this directory to create a Python virtual environment
and install the dependencies and daqhats library (assumes daqhats is installed in
~/daqhats, modify the code if you use another location). 

   ```
   python -m venv ~/webvenv
   ~/webvenv/bin/pip install ~/daqhats
   ~/webvenv/bin/pip install -r requirements.txt
   ```

## Start the web server
1. To start the web server and run the example, open a terminal window and enter the 
following commands: 

   ```sh
   ~/webvenv/bin/python ~/daqhats/examples/python/mcc118/web_server/web_server.py
   ```   
2. Open a web browser on a device on the same network as the host device and
   enter http://\<host\>:8080 in the address bar, replacing \<host\> with either 
   the IP Address or the hostname of the host device.

## Stop the web server
- To stop the web server, press **Ctrl+C** in the terminal window where the server 
was started.

## Support/Feedback
Contact technical support through our [support page](https://www.mccdaq.com/support/support_form.aspx). 

## More Information
- Dash: https://dash.plot.ly
- Plotly: https://plot.ly/python
