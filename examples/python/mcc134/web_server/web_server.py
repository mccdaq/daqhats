#!/usr/bin/env python
#  -*- coding: utf-8 -*-
"""
This example demonstrates a simple web server providing visualization of data
from a MCC 134 DAQ HAT device for a single client.  It makes use of the Dash
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
from collections import deque
from dash import Dash
from dash.dependencies import Input, Output, State
import dash_core_components as dcc
import dash_html_components as html
import plotly.graph_objs as go
from daqhats import hat_list, mcc134, HatIDs, TcTypes


_app = Dash(__name__)   # pylint: disable=invalid-name,no-member
_app.css.config.serve_locally = True
_app.scripts.config.serve_locally = True

_HAT = None  # Store the hat object in a global for use in multiple callbacks.

MCC134_CHANNEL_COUNT = 4


def create_hat_selector():
    """
    Gets a list of available MCC 134 devices and creates a corresponding
    dash-core-components Dropdown element for the user interface.

    Returns:
        dcc.Dropdown: A dash-core-components Dropdown object.
    """
    hats = hat_list(filter_by_id=HatIDs.MCC_134)
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

    return dcc.Dropdown(    # pylint: disable=no-member
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
_TC_TYPE_OPTIONS = [{'label': 'J', 'value': TcTypes.TYPE_J},
                    {'label': 'K', 'value': TcTypes.TYPE_K},
                    {'label': 'T', 'value': TcTypes.TYPE_T},
                    {'label': 'E', 'value': TcTypes.TYPE_E},
                    {'label': 'R', 'value': TcTypes.TYPE_R},
                    {'label': 'S', 'value': TcTypes.TYPE_S},
                    {'label': 'B', 'value': TcTypes.TYPE_B},
                    {'label': 'N', 'value': TcTypes.TYPE_N}]

# pylint: disable=no-member
_app.layout = html.Div([
    html.H1(
        children='MCC 134 DAQ HAT Web Server Example',
        id='exampleTitle'),
    html.Div(
        children=[
            html.Div(
                id='rightContent',
                children=[
                    dcc.Graph(id='stripChart'),
                    html.Div(id='errorDisplay',
                             children='',
                             style={'font-weight': 'bold', 'color': 'red'})],
                style={'width': '100%', 'box-sizing': 'border-box',
                       'float': 'left', 'padding-left': 320}),
            html.Div(
                id='leftContent',
                children=[
                    html.Label('Select a HAT...',
                               style={'font-weight': 'bold'}),
                    create_hat_selector(),
                    html.Label('Seconds per sample',
                               style={'font-weight': 'bold', 'display': 'block',
                                      'margin-top': 10}),
                    dcc.Input(id='secondsPerSample', type='number',
                              step=1, value=1.0, min=1.0,
                              style={'width': 100, 'display': 'block'}),
                    html.Label('Samples to display',
                               style={'font-weight': 'bold',
                                      'display': 'block', 'margin-top': 10}),
                    dcc.Input(id='samplesToDisplay', type='number', min=2,
                              max=1000, step=1, value=100,
                              style={'width': 100, 'display': 'block'}),
                    html.Div(
                        children=[
                            html.Label('Active Channels',
                                       style={'font-weight': 'bold',
                                              'display': 'block',
                                              'margin-top': 10,
                                              'margin-bottom': 8}),
                            dcc.Checklist(
                                id='channelSelections',
                                options=[
                                    {'label': 'Channel 0', 'value': 0},
                                    {'label': 'Channel 1', 'value': 1},
                                    {'label': 'Channel 2', 'value': 2},
                                    {'label': 'Channel 3', 'value': 3}],
                                labelStyle={'display': 'block', 'height': 36},
                                value=[0])],
                        style={'float': 'left', 'width': 150}),
                    html.Div(
                        id='tcTypeSelectors',
                        children=[
                            html.Label('TC Type',
                                       style={'font-weight': 'bold',
                                              'display': 'block',
                                              'margin-top': 10}),
                            dcc.Dropdown(id='tcTypeSelector0',
                                         options=_TC_TYPE_OPTIONS,
                                         value=TcTypes.TYPE_J,
                                         clearable=False),
                            dcc.Dropdown(id='tcTypeSelector1',
                                         options=_TC_TYPE_OPTIONS,
                                         value=TcTypes.TYPE_J,
                                         clearable=False),
                            dcc.Dropdown(id='tcTypeSelector2',
                                         options=_TC_TYPE_OPTIONS,
                                         value=TcTypes.TYPE_J,
                                         clearable=False),
                            dcc.Dropdown(id='tcTypeSelector3',
                                         options=_TC_TYPE_OPTIONS,
                                         value=TcTypes.TYPE_J,
                                         clearable=False)],
                        style={'float': 'left', 'width': 150,
                               'margin-bottom': 8}),
                    html.Button(
                        children='Configure',
                        id='startStopButton',
                        style={'width': 100, 'height': 35,
                               'text-align': 'center', 'margin-top': 30})],
                style={'width': 320, 'box-sizing': 'border-box', 'padding': 10,
                       'position': 'absolute', 'top': 0, 'left': 0})],
        style={'position': 'relative', 'display': 'block',
               'overflow': 'hidden', 'padding-bottom': 100}),
    dcc.Interval(
        id='timer',
        interval=1000*60*60*24,  # in milliseconds
        n_intervals=0),
    html.Div(
        id='chartData',
        style={'display': 'none'},
        children=init_chart_data(1, 1000)),
    html.Div(
        id='chartInfo',
        style={'display': 'none'},
        children=json.dumps({'sample_count': 0})),
    html.Div(
        id='status',
        style={'display': 'none'}),
])
# pylint: enable=no-member


@_app.callback(
    Output('status', 'children'),
    [Input('startStopButton', 'n_clicks')],
    [State('startStopButton', 'children'),
     State('hatSelector', 'value'),
     State('channelSelections', 'value'),
     State('tcTypeSelector0', 'value'),
     State('tcTypeSelector1', 'value'),
     State('tcTypeSelector2', 'value'),
     State('tcTypeSelector3', 'value')]
)   # pylint: disable=too-many-arguments
def start_stop_click(n_clicks, button_label, hat_descriptor_json_str,
                     active_channels, tc_type0, tc_type1, tc_type2, tc_type3):
    """
    A callback function to change the application status when the Configure,
    Start or Stop button is clicked.

    Args:
        n_clicks (int): Number of button clicks - triggers the callback.
        button_label (str): The current label on the button.
        hat_descriptor_json_str (str): A string representation of a JSON object
            containing the descriptor for the selected MCC 134 DAQ HAT.
        active_channels ([int]): A list of integers corresponding to the user
            selected Active channel checkboxes.
        tc_type0 (TcTypes): The selected TC Type for channel 0.
        tc_type1 (TcTypes): The selected TC Type for channel 0.
        tc_type2 (TcTypes): The selected TC Type for channel 0.
        tc_type3 (TcTypes): The selected TC Type for channel 0.

    Returns:
        str: The new application status - "idle", "configured", "running"
        or "error"

    """
    output = 'idle'
    if n_clicks is not None and n_clicks > 0:
        output = 'error'
        if button_label == 'Configure':
            # If configuring, create the hat object.
            if hat_descriptor_json_str:
                hat_descriptor = json.loads(hat_descriptor_json_str)
                # The hat object is retained as a global for use in
                # other callbacks.
                global _HAT     # pylint: disable=global-statement
                _HAT = mcc134(hat_descriptor['address'])

                if active_channels:
                    # Set the TC type for all active channels to the selected TC
                    # type prior to acquiring data.
                    tc_types = [tc_type0, tc_type1, tc_type2, tc_type3]
                    for channel in active_channels:
                        _HAT.tc_type_write(channel, tc_types[channel])
                    output = 'configured'
        elif button_label == 'Start':
            output = 'running'
        elif button_label == 'Stop':
            output = 'idle'

    return output


@_app.callback(
    Output('timer', 'interval'),
    [Input('status', 'children')],
    [State('secondsPerSample', 'value')]
)
def update_timer_interval(acq_state, seconds_per_sample):
    """
    A callback function to update the timer interval.  The timer interval is
    set to one day when idle.

    Args:
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.
        seconds_per_sample (float): The user specified sample rate value in
            seconds per sample.

    Returns:
        float: Timer interval value in ms.
    """
    interval = 1000*60*60*24  # 1 day
    if acq_state == 'running':
        interval = seconds_per_sample * 1000  # Convert to ms
    return interval


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
    Output('secondsPerSample', 'disabled'),
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
    for channel in range(MCC134_CHANNEL_COUNT):
        label = 'Channel ' + str(channel)
        disabled = False
        if acq_state == 'configured' or acq_state == 'running':
            disabled = True
        options.append({'label': label, 'value': channel, 'disabled': disabled})
    return options


@_app.callback(
    Output('tcTypeSelectors', 'style'),
    [Input('status', 'children')],
    [State('tcTypeSelectors', 'style')]
)
def disable_tc_type_selector_dropdowns(acq_state, div_style):
    # pylint: disable=invalid-name
    """
    A callback function to disable all TC Type selector dropdowns when
    the application status changes to configured or running.
    """
    div_style['pointer-events'] = 'auto'
    div_style['opacity'] = 1.0
    if acq_state == 'configured' or acq_state == 'running':
        div_style['pointer-events'] = 'none'
        div_style['opacity'] = 0.8
    return div_style


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

            # Reset error flags
            chart_data['open_tc_error'] = False
            chart_data['over_range_error'] = False
            chart_data['common_mode_range_error'] = False

            data = []
            for channel in active_channels:
                temp_val = hat.t_in_read(channel)
                if temp_val == mcc134.OPEN_TC_VALUE:
                    chart_data['open_tc_error'] = True
                    data.append(None)
                elif temp_val == mcc134.OVERRANGE_TC_VALUE:
                    chart_data['over_range_error'] = True
                    data.append(None)
                elif temp_val == mcc134.COMMON_MODE_TC_VALUE:
                    chart_data['common_mode_range_error'] = True
                    data.append(None)
                else:
                    data.append(temp_val)

            # Add the samples read to the chart_data object.
            sample_count = add_samples_to_data(samples_to_display, num_channels,
                                               chart_data, data)

            # Update the total sample count.
            chart_data['sample_count'] = sample_count
            updated_chart_data = json.dumps(chart_data)

    elif acq_state == 'configured':
        # Clear the data in the strip chart when Configure is clicked.
        updated_chart_data = init_chart_data(num_channels, samples_to_display)

    return updated_chart_data


def add_samples_to_data(samples_to_display, num_chans, chart_data, data):
    """
    Adds the samples read from the mcc134 hat device to the chart_data object
    used to update the strip chart.

    Args:
        samples_to_display (int): The number of samples to be displayed.
        num_chans (int): The number of selected channels.
        chart_data (dict): A dictionary containing the data used to update the
            strip chart display.
        data (list): A list of values for each active channel.

    Returns:
        int: The updated total sample count after the data is added.
    """
    num_samples_read = int(len(data) / num_chans)
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
            chart_data['data'][chan].append(data[data_index])

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
    colors = ['#DD3222', '#FFC000', '#3482CB', '#FF6A00']
    # Update the serie data for each active channel.
    for chan_idx, channel in enumerate(active_channels):
        scatter_serie = go.Scatter(
            x=list(chart_data['samples']),
            y=list(data[chan_idx]),
            name='Channel {0:d}'.format(channel),
            marker={'color': colors[channel]}
        )
        plot_data.append(scatter_serie)

    # Get min and max data values
    y_min = None
    y_max = None
    for chan_data in data:
        for y_val in chan_data:
            if y_min is None or (y_val is not None and y_val < y_min):
                y_min = y_val
            if y_max is None or (y_val is not None and y_val > y_max):
                y_max = y_val

    # Set the Y scale
    y_max = y_max + 5.0 if y_max is not None else 100.0
    y_min = y_min - 5.0 if y_min is not None else 0.0

    figure = {
        'data': plot_data,
        'layout': go.Layout(
            xaxis=dict(title='Samples', range=xaxis_range),
            yaxis=dict(title='Temperature (&deg;C)', range=[y_min, y_max]),
            margin={'l': 50, 'r': 40, 't': 50, 'b': 40, 'pad': 0},
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
     State('channelSelections', 'value')]
)  # pylint: disable=too-many-arguments
def update_error_message(chart_data_json_str, acq_state, hat_selection,
                         active_channels):
    """
    A callback function to display error messages.

    Args:
        chart_data_json_str (str): A string representation of a JSON object
            containing the current chart data - triggers the callback.
        acq_state (str): The application state of "idle", "configured",
            "running" or "error" - triggers the callback.
        hat_selection (str): A string representation of a JSON object
            containing the descriptor for the selected MCC 134 DAQ HAT.
        active_channels ([int]): A list of integers corresponding to the user
            selected Active channel checkboxes.

    Returns:
        str: The error message to display.
    """
    error_message = ''
    if acq_state == 'running':
        chart_data = json.loads(chart_data_json_str)
        if ('open_tc_error' in chart_data.keys()
                and chart_data['open_tc_error']):
            error_message += 'Open thermocouple; '
        if ('over_range_error' in chart_data.keys()
                and chart_data['over_range_error']):
            error_message += 'Temp outside valid range; '
        if ('common_mode_range_error' in chart_data.keys()
                and chart_data['common_mode_range_error']):
            error_message += 'Temp outside common-mode range; '
    elif acq_state == 'error':
        num_active_channels = len(active_channels)
        if not hat_selection:
            error_message += 'Invalid HAT selection; '
        if num_active_channels <= 0:
            error_message += 'Invalid channel selection (min 1); '

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
