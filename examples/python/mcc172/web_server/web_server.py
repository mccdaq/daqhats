#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
This example demonstrates a simple web server providing visualization of data
from a MCC 172 DAQ HAT device for a single client.  It makes use of the Dash
Python framework for web-based interfaces and a plotly graph.  To install the
dependencies for this example, run:
   $ pip install dash

Running this example:
1. Start the server by running the web_server.py module in a terminal.
   $ ./web_server.py
2. Open a web browser on a device on the same network as the host device and
   enter http://<host>:8080 in the address bar,
   replacing <host> with the IP Address or hostname of the host device.

Stopping this example:
1. To stop the server press Ctrl+C in the terminal window where there server
   was started.
"""
import socket
import json
from time import sleep
from collections import deque
from dash import Dash
from dash.dependencies import Input, Output, State
import dash_core_components as dcc
import dash_html_components as html
import plotly.graph_objs as go
from daqhats import hat_list, mcc172, HatIDs, OptionFlags, SourceType


_app = Dash(__name__)   # pylint: disable=invalid-name,no-member
_app.css.config.serve_locally = True
_app.scripts.config.serve_locally = True

_HAT = None  # Store the hat object in a global for use in multiple callbacks.

MCC172_CHANNEL_COUNT = 2
ALL_AVAILABLE = -1
RETURN_IMMEDIATELY = 0


def create_hat_selector():
    """
    Gets a list of available MCC 172 devices and creates a corresponding
    dash-core-components Dropdown element for the user interface.

    Returns:
        dcc.Dropdown: A dash-core-components Dropdown object.
    """
    hats = hat_list(filter_by_id=HatIDs.MCC_172)
    hat_selection_options = []
    for hat in hats:
        # Create the label from the address and product name
        label = '{0}: {1}'.format(hat.address, hat.product_name)
        # Create the value by converting the descriptor to a JSON object
        option = {'label': label, 'value': json.dumps(hat._asdict())}
        hat_selection_options.append(option)

    selection = None
    if hat_selection_options:
        selection = hat_selection_options[0]['value']

    return dcc.Dropdown(        # pylint: disable=no-member
        id='hatSelector', options=hat_selection_options,
        value=selection, clearable=False)


def init_chart_data(number_of_channels, number_of_samples):
    """
    Initializes the chart with the specified number of samples.

    Args:
        number_of_channels (int): The number of channels to be displayed.
        number_of_samples (int): The number of samples to be displayed.

    Returns:
        str: A string representation of a JSON object containing the chart data.
    """
    samples = []
    for i in range(number_of_samples):
        samples.append(i)
    data = []
    for _ in range(number_of_channels):
        data.append([None]*number_of_samples)

    chart_data = {'data': data, 'samples': samples, 'sample_count': 0}

    return json.dumps(chart_data)


# Define the HTML layout for the user interface, consisting of
# dash-html-components and dash-core-components.
# pylint: disable=no-member
_app.layout = html.Div([
    html.H1(
        children='MCC 172 DAQ HAT Web Server Example',
        id='exampleTitle'
    ),
    html.Div([
        html.Div(
            id='rightContent',
            children=[
                dcc.Graph(id='stripChart'),
                html.Div(id='errorDisplay',
                         children='',
                         style={'font-weight': 'bold', 'color': 'red'})
            ], style={'width': '100%', 'box-sizing': 'border-box',
                      'float': 'left', 'padding-left': 320}
        ),
        html.Div(
            id='leftContent',
            children=[
                html.Label('Select a HAT...', style={'font-weight': 'bold'}),
                create_hat_selector(),
                html.Label('Sample Rate (Hz)',
                           style={'font-weight': 'bold', 'display': 'block',
                                  'margin-top': 10}),
                dcc.Input(id='sampleRate', type='number', max=51200.0,
                          step=1, value=1000.0,
                          style={'width': 100, 'display': 'block'}),
                html.Label('Samples to display',
                           style={'font-weight': 'bold',
                                  'display': 'block', 'margin-top': 10}),
                dcc.Input(id='samplesToDisplay', type='number', min=1,
                          max=1000, step=1, value=100,
                          style={'width': 100, 'display': 'block'}),
                html.Label('Active Channels',
                           style={'font-weight': 'bold', 'display': 'block',
                                  'margin-top': 10}),
                dcc.Checklist(
                    id='channelSelections',
                    options=[
                        {'label': 'Channel 0', 'value': 0},
                        {'label': 'Channel 1', 'value': 1},
                    ],
                    labelStyle={'display': 'block'},
                    value=[0],
                    style={'width': 130, 'display': 'inline-block'}
                ),
                dcc.Checklist(
                    id='iepeSelections',
                    options=[
                        {'label': 'Enable IEPE', 'value': 0},
                        {'label': 'Enable IEPE', 'value': 1},
                    ],
                    labelStyle={'display': 'block'},
                    value=[],
                    style={'width': 130, 'display': 'inline-block'}
                ),
                html.Label('Sensor Sensitivity (mV/unit)',
                           style={'font-weight': 'bold', 'display': 'block',
                                  'margin-top': 10}),
                html.Div(id='sensitivityWrapper0', children=[
                    html.Label('Channel 0: ',
                               style={'margin-left': 15,
                                      'display': 'inline-block'}),
                    dcc.Input(id='sensitivity0', type='number', max=100000.0,
                              step=1.0, value=1000.0,
                              style={'width': 100, 'display': 'inline-block',
                                     'margin': '5px 0px 0px 10px'}),
                ]),
                html.Div(id='sensitivityWrapper1', children=[
                    html.Label('Channel 1: ',
                               style={'margin-left': 15,
                                      'display': 'inline-block'}),
                    dcc.Input(id='sensitivity1', type='number', max=100000.0,
                              step=1.0, value=1000.0,
                              style={'width': 100, 'display': 'inline-block',
                                     'margin': '5px 0px 0px 10px'}),
                ]),
                html.Button(
                    children='Configure',
                    id='startStopButton',
                    style={'width': 100, 'height': 35, 'text-align': 'center',
                           'margin-top': 20}
                ),
            ], style={'width': 320, 'box-sizing': 'border-box', 'padding': 10,
                      'position': 'absolute', 'top': 0, 'left': 0}
        ),
    ], style={'position': 'relative', 'display': 'block',
              'overflow': 'hidden'}),
    dcc.Interval(
        id='timer',
        interval=1000*60*60*24,  # in milliseconds
        n_intervals=0
    ),
    html.Div(
        id='chartData',
        style={'display': 'none'},
        children=init_chart_data(1, 1000)
    ),
    html.Div(
        id='chartInfo',
        style={'display': 'none'},
        children=json.dumps({'sample_count': 0})
    ),
    html.Div(
        id='status',
        style={'display': 'none'}
    ),
])
# pylint: enable=no-member

@_app.callback(
    Output('status', 'children'),
    [Input('startStopButton', 'n_clicks')],
    [State('startStopButton', 'children'),
     State('hatSelector', 'value'),
     State('sampleRate', 'value'),
     State('samplesToDisplay', 'value'),
     State('channelSelections', 'value'),
     State('iepeSelections', 'value')]
)   # pylint: disable=too-many-arguments
def start_stop_click(n_clicks, button_label, hat_descriptor_json_str,
                     sample_rate_val, samples_to_display, active_channels,
                     iepe_enabled):
    """
    A callback function to change the application status when the Configure,
    Start or Stop button is clicked.

    Args:
        n_clicks (int): Number of button clicks - triggers the callback.
        button_label (str): The current label on the button.
        hat_descriptor_json_str (str): A string representation of a JSON object
            containing the descriptor for the selected MCC 172 DAQ HAT.
        sample_rate_val (float): The user specified sample rate value.
        samples_to_display (float): The number of samples to be displayed.
        active_channels ([int]): A list of integers corresponding to the user
            selected Active channel checkboxes.
        iepe_enabled ([int]): A list of integers corresponding to the user
            selected enable IEPE checkboxes for each channel.

    Returns:
        str: The new application status - "idle", "configured", "running"
        or "error"

    """
    output = 'idle'
    if n_clicks is not None and n_clicks > 0:
        if button_label == 'Configure':
            if (1 < samples_to_display <= 1000
                    and active_channels
                    and sample_rate_val <= 51200):
                # If configuring, create the hat object.
                if hat_descriptor_json_str:
                    hat_descriptor = json.loads(hat_descriptor_json_str)
                    # The hat object is retained as a global for use in
                    # other callbacks.
                    global _HAT     # pylint: disable=global-statement
                    hat = mcc172(hat_descriptor['address'])
                    hat.a_in_clock_config_write(SourceType.LOCAL, sample_rate_val)
                    for channel in range(MCC172_CHANNEL_COUNT):
                        if channel in iepe_enabled:
                            hat.iepe_config_write(channel, 1)
                        else:
                            hat.iepe_config_write(channel, 0)
                    _HAT = hat
                    output = 'configured'
            else:
                output = 'error'
        elif button_label == 'Start':
            # If starting, call the a_in_scan_start function.
            channel_mask = 0x0
            hat = globals()['_HAT']
            for channel in active_channels:
                channel_mask |= 1 << channel
            # Buffer 5 seconds of data
            samples_to_buffer = int(5 * sample_rate_val)
            hat.a_in_scan_start(channel_mask, samples_to_buffer,
                                OptionFlags.CONTINUOUS)
            sleep(0.5)
            output = 'running'
        elif button_label == 'Stop':
            # If stopping, call the a_in_scan_stop and a_in_scan_cleanup
            # functions.
            hat = globals()['_HAT']
            hat.a_in_scan_stop()
            hat.a_in_scan_cleanup()
            output = 'idle'

    return output


@_app.callback(
    Output('timer', 'interval'),
    [Input('status', 'children'),
     Input('chartData', 'children'),
     Input('chartInfo', 'children')],
    [State('channelSelections', 'value'),
     State('samplesToDisplay', 'value')]
)
def update_timer_interval(acq_state, chart_data_json_str, chart_info_json_str,
                          active_channels, samples_to_display):
    """
    A callback function to update the timer interval.  The timer is temporarily
    disabled while processing data by setting the interval to 1 day and then
    re-enabled when the data read has been plotted.  The interval value when
    enabled is calculated based on the data throughput necessary with a minimum
    of 500 ms and maximum of 4 seconds.

    Args:
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.
        chart_data_json_str (str): A string representation of a JSON object
            containing the current chart data - triggers the callback.
        chart_info_json_str (str): A string representation of a JSON object
            containing the current chart status - triggers the callback.
        active_channels ([int]): A list of integers corresponding to the user
            selected active channel checkboxes.
        samples_to_display (float): The number of samples to be displayed.

    Returns:

    """
    chart_data = json.loads(chart_data_json_str)
    chart_info = json.loads(chart_info_json_str)
    num_channels = int(len(active_channels))
    refresh_rate = 1000*60*60*24  # 1 day

    if acq_state == 'running':
        # Activate the timer when the sample count displayed to the chart
        # matches the sample count of data read from the HAT device.
        if 0 < chart_info['sample_count'] == chart_data['sample_count']:
            # Determine the refresh rate based on the amount of data being
            # displayed.
            refresh_rate = int(num_channels * samples_to_display / 2)
            if refresh_rate < 500:
                refresh_rate = 500  # Minimum of 500 ms

    return refresh_rate


@_app.callback(
    Output('sampleRate', 'value'),
    [Input('status', 'children')],
    [State('sampleRate', 'value'),
     State('sensitivity0', 'value'),
     State('sensitivity1', 'value')]
)
def update_sample_rate_input(acq_state, sample_rate_val, sensitivity0_val,
                             sensitivity1_val):
    """
    A callback function to set the sample rate input to the
    actual sample rate and set the sensory sensitivity values for each channel
    when the application status changes to configured.

    Args:
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.
        sample_rate_val (float): The user specified sample rate value.
        sensitivity0_val (float): The user specified sensor sensitivity for
            channel 0.
        sensitivity1_val (float): The user specified sensor sensitivity for
            channel 1.

    Returns:
        float: The actual sample rate used for the acquisition
    """
    actual_scan_rate = sample_rate_val
    if acq_state == 'configured':
        synced = False
        hat = globals()['_HAT']
        while not synced:
            (_source, actual_scan_rate, synced) = hat.a_in_clock_config_read()
            if not synced:
                sleep(0.005)
        hat.a_in_sensitivity_write(0, sensitivity0_val)
        hat.a_in_sensitivity_write(1, sensitivity1_val)

    return actual_scan_rate


@_app.callback(
    Output('hatSelector', 'disabled'),
    [Input('status', 'children')]
)
def disable_hat_selector_dropdown(acq_state):
    """
    A callback function to disable the HAT selector dropdown when the
    application status changes to configured or running.
    """
    disabled = False
    if acq_state == 'configured' or acq_state == 'running':
        disabled = True
    return disabled


@_app.callback(
    Output('sampleRate', 'disabled'),
    [Input('status', 'children')]
)
def disable_sample_rate_input(acq_state):
    """
    A callback function to disable the sample rate input when the
    application status changes to configured or running.
    """
    disabled = False
    if acq_state == 'configured' or acq_state == 'running':
        disabled = True
    return disabled


@_app.callback(
    Output('samplesToDisplay', 'disabled'),
    [Input('status', 'children')]
)
def disable_samples_to_disp_input(acq_state):
    """
    A callback function to disable the number of samples to display input
    when the application status changes to configured or running.
    """
    disabled = False
    if acq_state == 'configured' or acq_state == 'running':
        disabled = True
    return disabled


@_app.callback(
    Output('channelSelections', 'options'),
    [Input('status', 'children')]
)
def disable_channel_checkboxes(acq_state):
    """
    A callback function to disable the active channel checkboxes when the
    application status changes to configured or running.
    """
    options = []
    for channel in range(MCC172_CHANNEL_COUNT):
        label = 'Channel ' + str(channel)
        disabled = False
        if acq_state == 'configured' or acq_state == 'running':
            disabled = True
        options.append({'label': label, 'value': channel, 'disabled': disabled})
    return options


@_app.callback(
    Output('iepeSelections', 'options'),
    [Input('status', 'children')]
)
def disable_iepe_checkboxes(acq_state):
    """
    A callback function to disable the enable IEPE checkboxes when the
    application status changes to configured or running.
    """
    options = []
    label = 'Enable IEPE'
    for channel in range(MCC172_CHANNEL_COUNT):
        disabled = False
        if acq_state == 'configured' or acq_state == 'running':
            disabled = True
        options.append({'label': label, 'value': channel, 'disabled': disabled})
    return options


@_app.callback(
    Output('sensitivity0', 'disabled'),
    [Input('status', 'children')]
)
def disable_sensitivity0_input(acq_state):
    """
    A callback function to disable the sensor sensitivity input for channel 0
    when the application status changes to configured or running.
    """
    disabled = False
    if acq_state == 'configured' or acq_state == 'running':
        disabled = True
    return disabled


@_app.callback(
    Output('sensitivity1', 'disabled'),
    [Input('status', 'children')]
)
def disable_sensitivity1_input(acq_state):
    """
    A callback function to disable the sensor sensitivity input for channel 1
    when the application status changes to configured or running.
    """
    disabled = False
    if acq_state == 'configured' or acq_state == 'running':
        disabled = True
    return disabled


@_app.callback(
    Output('startStopButton', 'children'),
    [Input('status', 'children')]
)
def update_start_stop_button_name(acq_state):
    """
    A callback function to update the label on the button when the application
    status changes.

    Args:
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.

    Returns:
        str: The new button label of "Configure", "Start" or "Stop"
    """
    output = 'Configure'
    if acq_state == 'configured':
        output = 'Start'
    elif acq_state == 'running':
        output = 'Stop'
    return output


@_app.callback(
    Output('chartData', 'children'),
    [Input('timer', 'n_intervals'),
     Input('status', 'children')],
    [State('chartData', 'children'),
     State('samplesToDisplay', 'value'),
     State('channelSelections', 'value')]
)
def update_strip_chart_data(_n_intervals, acq_state, chart_data_json_str,
                            samples_to_display_val, active_channels):
    """
    A callback function to update the chart data stored in the chartData HTML
    div element.  The chartData element is used to store the existing data
    values, which allows sharing of data between callback functions.  Global
    variables cannot be used to share data between callbacks (see
    https://dash.plot.ly/sharing-data-between-callbacks).

    Args:
        _n_intervals (int): Number of timer intervals - triggers the callback.
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.
        chart_data_json_str (str): A string representation of a JSON object
            containing the current chart data.
        samples_to_display_val (float): The number of samples to be displayed.
        active_channels ([int]): A list of integers corresponding to the user
            selected active channel checkboxes.

    Returns:
        str: A string representation of a JSON object containing the updated
        chart data.
    """
    updated_chart_data = chart_data_json_str
    samples_to_display = int(samples_to_display_val)
    num_channels = len(active_channels)
    if acq_state == 'running':
        hat = globals()['_HAT']
        if hat is not None:
            chart_data = json.loads(chart_data_json_str)

            # By specifying -1 for the samples_per_channel parameter, the
            # timeout is ignored and all available data is read.
            read_result = hat.a_in_scan_read(ALL_AVAILABLE, RETURN_IMMEDIATELY)

            if ('hardware_overrun' not in chart_data.keys()
                    or not chart_data['hardware_overrun']):
                chart_data['hardware_overrun'] = read_result.hardware_overrun
            if ('buffer_overrun' not in chart_data.keys()
                    or not chart_data['buffer_overrun']):
                chart_data['buffer_overrun'] = read_result.buffer_overrun

            # Add the samples read to the chart_data object.
            sample_count = add_samples_to_data(samples_to_display, num_channels,
                                               chart_data, read_result)

            # Update the total sample count.
            chart_data['sample_count'] = sample_count
            updated_chart_data = json.dumps(chart_data)

    elif acq_state == 'configured':
        # Clear the data in the strip chart when Configure is clicked.
        updated_chart_data = init_chart_data(num_channels, samples_to_display)

    return updated_chart_data


def add_samples_to_data(samples_to_display, num_chans, chart_data, read_result):
    """
    Adds the samples read from the mcc172 hat device to the chart_data object
    used to update the strip chart.

    Args:
        samples_to_display (int): The number of samples to be displayed.
        num_chans (int): The number of selected channels.
        chart_data (dict): A dictionary containing the data used to update the
            strip chart display.
        read_result (namedtuple): A namedtuple containing status and data
            returned from the mcc172 read.

    Returns:
        int: The updated total sample count after the data is added.

    """
    num_samples_read = int(len(read_result.data) / num_chans)
    current_sample_count = int(chart_data['sample_count'])

    # Convert lists to deque objects with the maximum length set to the number
    # of samples to be displayed.  This will pop off the oldest data
    # automatically when new data is appended.
    chart_data['samples'] = deque(chart_data['samples'],
                                  maxlen=samples_to_display)
    for chan in range(num_chans):
        chart_data['data'][chan] = deque(chart_data['data'][chan],
                                         maxlen=samples_to_display)

    start_sample = 0
    if num_samples_read > samples_to_display:
        start_sample = num_samples_read - samples_to_display

    for sample in range(start_sample, num_samples_read):
        chart_data['samples'].append(current_sample_count + sample)
        for chan in range(num_chans):
            data_index = sample * num_chans + chan
            chart_data['data'][chan].append(read_result.data[data_index])

    # Convert deque objects back to lists so they can be written to to div
    # element.
    chart_data['samples'] = list(chart_data['samples'])
    for chan in range(num_chans):
        chart_data['data'][chan] = list(chart_data['data'][chan])

    return current_sample_count + num_samples_read


@_app.callback(
    Output('stripChart', 'figure'),
    [Input('chartData', 'children')],
    [State('channelSelections', 'value')]
)
def update_strip_chart(chart_data_json_str, active_channels):
    """
    A callback function to update the strip chart display when new data is read.

    Args:
        chart_data_json_str (str): A string representation of a JSON object
            containing the current chart data - triggers the callback.
        active_channels ([int]): A list of integers corresponding to the user
            selected Active channel checkboxes.

    Returns:
        object: A figure object for a dash-core-components Graph, updated with
        the most recently read data.
    """
    data = []
    xaxis_range = [0, 1000]
    chart_data = json.loads(chart_data_json_str)
    if 'samples' in chart_data and chart_data['samples']:
        xaxis_range = [min(chart_data['samples']), max(chart_data['samples'])]
    if 'data' in chart_data:
        data = chart_data['data']

    plot_data = []
    colors = ['#DD3222', '#FFC000', '#3482CB', '#FF6A00',
              '#75B54A', '#808080', '#6E1911', '#806000']
    # Update the serie data for each active channel.
    for chan_idx, channel in enumerate(active_channels):
        scatter_serie = go.Scatter(
            x=list(chart_data['samples']),
            y=list(data[chan_idx]),
            name='Channel {0:d}'.format(channel),
            marker={'color': colors[channel]}
        )
        plot_data.append(scatter_serie)

    figure = {
        'data': plot_data,
        'layout': go.Layout(
            xaxis=dict(title='Samples', range=xaxis_range),
            yaxis=dict(title='Sensor Units'),
            margin={'l': 40, 'r': 40, 't': 50, 'b': 40, 'pad': 0},
            showlegend=True,
            title='Strip Chart'
        )
    }

    return figure


@_app.callback(
    Output('chartInfo', 'children'),
    [Input('stripChart', 'figure')],
    [State('chartData', 'children')]
)
def update_chart_info(_figure, chart_data_json_str):
    """
    A callback function to set the sample count for the number of samples that
    have been displayed on the chart.

    Args:
        _figure (object): A figure object for a dash-core-components Graph for
            the strip chart - triggers the callback.
        chart_data_json_str (str): A string representation of a JSON object
            containing the current chart data - triggers the callback.

    Returns:
        str: A string representation of a JSON object containing the chart info
        with the updated sample count.

    """
    chart_data = json.loads(chart_data_json_str)
    chart_info = {'sample_count': chart_data['sample_count']}
    return json.dumps(chart_info)


@_app.callback(
    Output('errorDisplay', 'children'),
    [Input('chartData', 'children'),
     Input('status', 'children')],
    [State('hatSelector', 'value'),
     State('sampleRate', 'value'),
     State('samplesToDisplay', 'value'),
     State('channelSelections', 'value')]
)  # pylint: disable=too-many-arguments
def update_error_message(chart_data_json_str, acq_state, hat_selection,
                         sample_rate, samples_to_display, active_channels):
    """
    A callback function to display error messages.

    Args:
        chart_data_json_str (str): A string representation of a JSON object
            containing the current chart data - triggers the callback.
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.
        hat_selection (str): A string representation of a JSON object
            containing the descriptor for the selected MCC 172 DAQ HAT.
        sample_rate (float): The user specified sample rate value.
        samples_to_display (float): The number of samples to be displayed.
        active_channels ([int]): A list of integers corresponding to the user
            selected Active channel checkboxes.

    Returns:
        str: The error message to display.

    """
    error_message = ''
    if acq_state == 'running':
        chart_data = json.loads(chart_data_json_str)
        if ('hardware_overrun' in chart_data.keys()
                and chart_data['hardware_overrun']):
            error_message += 'Hardware overrun occurred; '
        if ('buffer_overrun' in chart_data.keys()
                and chart_data['buffer_overrun']):
            error_message += 'Buffer overrun occurred; '
    elif acq_state == 'error':
        num_active_channels = len(active_channels)
        max_sample_rate = 51200
        if not hat_selection:
            error_message += 'Invalid HAT selection; '
        if num_active_channels <= 0:
            error_message += 'Invalid channel selection (min 1); '
        if sample_rate > max_sample_rate:
            error_message += 'Invalid Sample Rate (max: '
            error_message += str(max_sample_rate) + '); '
        if samples_to_display <= 1 or samples_to_display > 1000:
            error_message += 'Invalid Samples to display (range: 2-1000); '

    return error_message


def get_ip_address():
    """ Utility function to get the IP address of the device. """
    ip_address = '127.0.0.1'  # Default to localhost
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        sock.connect(('1.1.1.1', 1))  # Does not have to be reachable
        ip_address = sock.getsockname()[0]
    finally:
        sock.close()

    return ip_address


if __name__ == '__main__':
    # This will only be run when the module is called directly.
    _app.run_server(host=get_ip_address(), port=8080)
