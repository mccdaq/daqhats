#include <time.h>
#include <stdlib.h>

#include "../../../daqhats_utils.h"
#include "logger.h"


int main(void)
{
    int retval = 0;
    GtkApplication *app;

    // Set the application name.
    strcpy(application_name, "MCC 172 Data Logger");

    // Set the default filename.
    getcwd(csv_filename, sizeof(csv_filename));
    strcat(csv_filename, "/LogFiles/csv_test.csv");

    // Channel 0 - Red
    legendColor[0].red = 221.0/255;
    legendColor[0].green = 50.0/255;
    legendColor[0].blue = 34.0/255;
    legendColor[0].alpha = 1;
    graphChannelInfo[0].color = &legendColor[0];
    graphChannelInfo[0].channelNumber = 0;

    // Channel 1 - Blue
    legendColor[1].red = 52.0/255;
    legendColor[1].green = 130.0/255;
    legendColor[1].blue = 203.0/255;
    legendColor[1].alpha = 1;
    graphChannelInfo[1].color = &legendColor[1];
    graphChannelInfo[1].channelNumber = 1;

    // Create the application structure and set
    // an event handler for the activate event.
    app = gtk_application_new("org.mcc.example", G_APPLICATION_FLAGS_NONE);

    gtk_application_new("org.mcc.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate_event_handler),
        NULL);

    // Start running the GTK appliction.
    g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    // Find the hat devices and open the first one.
    retval = open_first_hat_device(&address);

    if (retval == 0)
    {
        context = g_main_context_default();

        // Start the GTK message loop.
        gtk_main();

        // Close the device.
        mcc172_close(address);
    }

    //Exit app...
    return 0;
}

// Allocate arrays for the indices and data for each channel in the scan.
int allocate_channel_xy_arrays(uint8_t current_channel_mask,
    uint32_t fft_size)
{
    int chan = 0;
    int chanMask = current_channel_mask;
    int num_channels = 0;
    int fft_buffer_size = fft_size / 2 + 1;
    gfloat frequency_interval = iRatePerChannel / fft_size;
    gfloat freq_val = 0.0;

    // Delete the previous arrays for each of the channels (if they exist)
    for (chan = 0; chan < MAX_172_CHANNELS; chan++)
    {
        if (graphChannelInfo[chan].graph != NULL)
        {
            gtk_databox_graph_remove (GTK_DATABOX(dataBox),
                GTK_DATABOX_GRAPH(graphChannelInfo[chan].graph));
            graphChannelInfo[chan].graph= NULL;
        }

        if (graphChannelInfo[chan].fft_graph != NULL)
        {
            gtk_databox_graph_remove (GTK_DATABOX(fftBox),
                GTK_DATABOX_GRAPH(graphChannelInfo[chan].fft_graph));
            graphChannelInfo[chan].fft_graph= NULL;
        }

        // Free any existing data arrays
        g_free(graphChannelInfo[chan].X);
        g_free(graphChannelInfo[chan].Y);
        g_free(graphChannelInfo[chan].fft_X);
        g_free(graphChannelInfo[chan].fft_Y);

        // If this channel is in the scan, allocate new arrays
        if (chanMask & 1)
        {
            // Allocate arrays for data (Y) values and initialized to zero
            graphChannelInfo[chan].Y = g_new0(gfloat, fft_size);
            graphChannelInfo[chan].fft_Y = g_new0(gfloat, fft_buffer_size);

            // Allocate arrays for sample/frequency (X) values and initialize
            graphChannelInfo[chan].X = g_new(gfloat, fft_size);
            graphChannelInfo[chan].fft_X = g_new(gfloat, fft_buffer_size);

            freq_val = 0.0;
            for (gint i = 0; i < fft_size; i++)
            {
                graphChannelInfo[chan].X[i] = (gfloat)i;
                if(i < fft_buffer_size) {
                    graphChannelInfo[chan].fft_X[i] = freq_val;
                    freq_val += frequency_interval;
                }
            }

            num_channels++;
        }

        chanMask >>= 1;
    }

    // Return the number of channels in the scan.
    return num_channels;
}


// Add each checked channel to the channel mask
int create_selected_channel_mask()
{
    gboolean checked = FALSE;
    int selected_channel_mask = 0;

    for (int i = 0; i < MAX_172_CHANNELS; i++)
    {
        // Is the channel checked?
        checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkChan[i]));

        // If checked, add the channel to the mask
        if (checked == TRUE)
        {
            selected_channel_mask |= 1<<i;
        }
    }

    // return the channel mask
    return selected_channel_mask;
}


// Set te IEPE power configuration
void set_iepe_configuration()
{
    gboolean checked = FALSE;

    for (int i = 0; i < MAX_172_CHANNELS; i++)
    {
        // Is the channel IEPE checked?
        checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkIepe[i]));
        mcc172_iepe_config_write(address, i, checked);
    }
    return;
}


// Enable/disable the controls in the main window.
// Controls are disabled when the acquisition is running
// and re-enabled when the acquisition is stopped.
void set_enable_state_for_controls(gboolean state)
{
    // Set the state of the check boxes
    for (int i = 0; i < MAX_172_CHANNELS; i++)
    {
        gtk_widget_set_sensitive (chkChan[i], state);
        gtk_widget_set_sensitive (chkIepe[i], state);
    }

    // Set the state of the text boxes
    gtk_widget_set_sensitive (spinRate, state);
    gtk_widget_set_sensitive (comboBoxFftSize, state);

    // Set the state of the radio buttons
    gtk_widget_set_sensitive (rbFinite, state);
    gtk_widget_set_sensitive (rbContinuous, state);

    // Set the state of the buttons
    gtk_widget_set_sensitive (btnSelectLogFile, state);
    gtk_widget_set_sensitive (btnQuit, state);

}

// Copy data from the hat read buffer to the display buffer
int copy_hat_data_to_display_buffer(double* hat_read_buf,
                                    int samples_per_chan_read,
                                    double* display_buf,
                                    int samples_per_chan_displayed,
                                    int display_buf_size_samples,
                                    int num_chans)
{
    size_t copy_size = 0;
    int samples_to_keep = 0;
    int start_idx = 0;

    if (samples_per_chan_read > 0) {
        // There are samples to be copied
        if((samples_per_chan_displayed + samples_per_chan_read)
            <= display_buf_size_samples)
        {
            // All of the samples read will fit in the display buffer
            // so copy all of the samples
            copy_size = samples_per_chan_read * num_chans * sizeof(double);
            memcpy(&display_buf[samples_per_chan_displayed * num_chans],
                   hat_read_buf, copy_size);
            samples_per_chan_displayed += samples_per_chan_read;
        }
        else if (samples_per_chan_read > display_buf_size_samples) {
            // The number of samples read is larger than the size of the
            // display buffer, so overwrite the entire display buffer with the
            // last samples read
            copy_size = display_buf_size_samples * num_chans * sizeof(double);
            start_idx = ((samples_per_chan_read - display_buf_size_samples)
                         * num_chans);
            memcpy(display_buf, &hat_read_buf[start_idx], copy_size);
            samples_per_chan_displayed = display_buf_size_samples;
        }
        else {
            // The number of samples read is larger than the remaining space in
            // the display buffer, but less than the display buffer size.
            // Therefore, the display buffer values must first be shifted.
            samples_to_keep = display_buf_size_samples - samples_per_chan_read;
            copy_size = samples_to_keep * num_chans * sizeof(double);
            start_idx = ((samples_per_chan_displayed - samples_to_keep)
                         * num_chans);
            memcpy(display_buf, &display_buf[start_idx], copy_size);
            samples_per_chan_displayed = samples_to_keep;

            copy_size = samples_per_chan_read * num_chans * sizeof(double);
            memcpy(&display_buf[samples_per_chan_displayed * num_chans],
                   hat_read_buf, copy_size);
            samples_per_chan_displayed += samples_per_chan_read;
        }
    }

    return samples_per_chan_displayed;
}


// Copy the data for the specified channel from the interleaved
// HAT buffer to the array for the specified channel.
void copy_data_to_xy_arrays(double* display_buf, int read_buf_start_index,
    int channel, int stride, int buffer_size_samples, uint32_t start_sample)
{
    uint32_t sample = start_sample;
    uint32_t data_array_idx = 0;
    // Set indices and data
    for (int i = read_buf_start_index; i < buffer_size_samples; i+=stride)
    {
        graphChannelInfo[channel].X[data_array_idx] = (gfloat)sample;
        graphChannelInfo[channel].Y[data_array_idx] = (gfloat)display_buf[i];
        data_array_idx++;
        sample++;
    }
}


// Refresh the graph with the new data.
gboolean refresh_graph(int* start_sample_ptr)
{
    int start_sample = *start_sample_ptr;

    g_mutex_lock (&data_mutex);

    // Set the new limits on the time domain graph
    gfloat start = (gfloat)start_sample;
    gfloat end = (gfloat)(start_sample + iFftSize);
    gtk_databox_set_total_limits(GTK_DATABOX (dataBox), start, end, 6.0, -6.0);

    // Re-draw the graphs
    gtk_widget_queue_draw(dataBox);
    gtk_widget_queue_draw(fftBox);

    // release the mutex
    g_mutex_unlock (&data_mutex);

    return FALSE;
}


// Initialize the time domain plot and the FFT plot based on the selected
// settings prior to starting an acquisition
gboolean initialize_graphs() {
    uint8_t chanMask = channel_mask;
    int channel = 0;
    int start_sample = 0;

    pthread_mutex_lock(&graph_init_mutex);

    // Allocate memory for the data arrays
    allocate_channel_xy_arrays(channel_mask, iFftSize);

    while (chanMask > 0)
    {
        // Create graph object for each channel and add it to the graph
        if (chanMask & 1)
        {
            graphChannelInfo[channel].graph = gtk_databox_lines_new
                ((guint)iFftSize, graphChannelInfo[channel].X,
                graphChannelInfo[channel].Y,
                graphChannelInfo[channel].color, 1);
            gtk_databox_graph_add(GTK_DATABOX (dataBox),
                GTK_DATABOX_GRAPH(graphChannelInfo[channel].graph));

            graphChannelInfo[channel].fft_graph = gtk_databox_lines_new
                ((guint)iFftSize/2 + 1, graphChannelInfo[channel].fft_X,
                graphChannelInfo[channel].fft_Y,
                graphChannelInfo[channel].color, 1);
            gtk_databox_graph_add(GTK_DATABOX (fftBox),
                GTK_DATABOX_GRAPH(graphChannelInfo[channel].fft_graph));
        }
        channel++;
        chanMask >>= 1;
    }

    // Set the limits for the FFT graph - only needs to be done once per scan
    gtk_databox_set_total_limits(GTK_DATABOX (fftBox), 0.0,
        (gfloat)iRatePerChannel/2, 10.0, -150.0);

    refresh_graph(&start_sample);

    pthread_cond_signal(&graph_init_cond);
    pthread_mutex_unlock(&graph_init_mutex);

    return FALSE;
}


// While the scan is running, read the data, write it to a
// CSV file, and plot it in the graph.
// This function runs as a background thread for the duration of the scan.
static void * read_and_display_data ()
{
    int chanMask = channel_mask;
    int channel = 0;
    uint32_t samples_read_per_channel = 0;
    int retval = 0;
    uint16_t read_status;
    uint32_t sample_count = 0;
    int start_sample = 0;

    int num_channels = 0;
    while (chanMask > 0)
    {
        if (chanMask & 1)
        {
            num_channels++;
        }
        chanMask >>= 1;
    }

    // Setup the buffers
    uint32_t display_buf_size_samples = iFftSize * num_channels;
    uint32_t read_buf_size_samples = iRatePerChannel * num_channels * 5;

    double hat_read_buf[read_buf_size_samples];
    memset(hat_read_buf, 0, read_buf_size_samples * sizeof(double));

    double display_buf[display_buf_size_samples];
    memset(display_buf, 0, display_buf_size_samples * sizeof(double));
    int samples_in_display_buf = 0;

    // Initialize the graphs and use signal to synchronize threads
    pthread_mutex_lock(&graph_init_mutex);
    g_main_context_invoke(context, (GSourceFunc)initialize_graphs, NULL);
    pthread_cond_wait(&graph_init_cond, &graph_init_mutex);
    pthread_mutex_unlock(&graph_init_mutex);

    // Write channel numbers to file header
	retval = init_log_file(log_file_ptr, channel_mask);
	if (retval < 0)
	{
		switch (retval)
		{
            case -1:
                error_code = MAXIMUM_FILE_SIZE_EXCEEDED;
                break;

            default:
                error_code = UNKNOWN_ERROR;
                break;
		}

		// Error dialog must be displayed on the main thread.
		g_main_context_invoke(context,
			(GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);

		// Call the Start/Stop event handler to reset the UI
		start_stop_event_handler(btnStart_Stop, NULL);

		return NULL;
	}

    // Wait for the scan to start running.
    do
    {
       retval = mcc172_a_in_scan_status(address, &read_status,
            &samples_read_per_channel);

        if (retval != RESULT_SUCCESS)
        {
            // Error dialog must be displayed on the main thread.
            error_code = retval;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread,
                (gpointer)&error_code);

            // If the scan fails to start clear it and
            // reset the app to start again
            g_print("Scan failure, Return code:  %d,  ReadStatus:  %d\n",
                retval, read_status);
            retval = mcc172_a_in_scan_stop(address);
            if (retval != RESULT_SUCCESS)
            {
                // Error dialog must be displayed on the main thread.
                error_code = retval;
                g_main_context_invoke(context,
                    (GSourceFunc)show_mcc172_error_main_thread,
                    (gpointer)&error_code);
            }

            retval = mcc172_a_in_scan_cleanup(address);
            if (retval != RESULT_SUCCESS)
            {
                // Error dialog must be displayed on the main thread.
                error_code = retval;
                g_main_context_invoke(context,
                    (GSourceFunc)show_mcc172_error_main_thread,
                    (gpointer)&error_code);
            }

            g_main_context_invoke(context, (GSourceFunc)stop_acquisition,
                                  NULL);
            return NULL;
        }
    } while ((retval == RESULT_SUCCESS) &&
             ((read_status & STATUS_RUNNING) != STATUS_RUNNING));

    // Loop to read data continuously
    while (done != TRUE)
    {
        // Read the data from the device
        samples_read_per_channel = 0;

        uint32_t samples_to_read = read_buf_size_samples;
        if(!continuous) {
            samples_to_read = (iFftSize - sample_count) * num_channels;
        }

        retval = mcc172_a_in_scan_read(address, &read_status,
            READ_ALL_AVAILABLE, 0, hat_read_buf,
            samples_to_read, &samples_read_per_channel);

        sample_count += samples_read_per_channel;

        if (retval != RESULT_SUCCESS)
        {
            // Error dialog must be displayed on the main thread.
            error_code = retval;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread,
                (gpointer)&error_code);
            break;
        }
        else if (read_status & STATUS_HW_OVERRUN)
        {
            // Error dialog must be displayed on the main thread.
            error_code = HW_OVERRUN;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread,
                (gpointer)&error_code);
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            // Error dialog must be displayed on the main thread.
            error_code = BUFFER_OVERRUN;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread,
                (gpointer)&error_code);
            break;
        }

        // Write the data to a log file as CSV data
        retval = write_log_file(log_file_ptr, hat_read_buf,
                                samples_read_per_channel, num_channels);
        if (retval < 0)
        {
            switch (retval)
            {
                case -1:
                    error_code = MAXIMUM_FILE_SIZE_EXCEEDED;
                    break;

                default:
                    error_code = UNKNOWN_ERROR;
                    break;
            }

            // Error dialog must be displayed on the main thread.
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread,
                (gpointer)&error_code);

            // Call the Start/Stop event handler to reset the UI
            start_stop_event_handler(btnStart_Stop, NULL);
        }

        samples_in_display_buf = copy_hat_data_to_display_buffer(
                                    hat_read_buf, samples_read_per_channel,
                                    display_buf, samples_in_display_buf,
                                    iFftSize, num_channels);
        // Set a mutex to prevent the data from changing while we plot it
        g_mutex_lock (&data_mutex);

        chanMask = channel_mask;
        channel = 0;
        int read_buf_index = 0;
        int chan_index = 0;

        start_sample = sample_count >= iFftSize ? (sample_count-iFftSize) : 0;
        // While there are channels to plot.
        while (chanMask > 0)
        {
            // If this channel is included in the acquisition, plot its data.
            if (chanMask & 1)
            {
                copy_data_to_xy_arrays(display_buf, read_buf_index++,
                    channel, num_channels, display_buf_size_samples,
                    start_sample);

                if (samples_in_display_buf >= iFftSize)
                {
                    // Calculate and display the FFT.
                    calculate_real_fft(display_buf, iFftSize, num_channels,
                        chan_index++, mcc172_info()->AI_MAX_RANGE,
                        graphChannelInfo[channel].fft_Y);
                }
            }
            channel++;
            chanMask >>= 1;
        }

        // Done with the data so fill the buffer with zeros.
        memset(hat_read_buf, 0, read_buf_size_samples * sizeof(double));

        // Update the display.
        g_main_context_invoke(context, (GSourceFunc)refresh_graph,
                              &start_sample);

        // Release the mutex
        g_mutex_unlock(&data_mutex);

        if(!continuous && sample_count == iFftSize) {
            g_main_context_invoke(context, (GSourceFunc)stop_acquisition,
                                  NULL);
        }

        // Allow 200 msec idle time between each read
        usleep(200000);
    }

    // Stop the scan.
    retval = mcc172_a_in_scan_stop(address);
    if (retval < 0)
    {
        show_mcc172_error(retval);
    }

    // Clean up after the scan completes
    retval = mcc172_a_in_scan_cleanup(address);
    if (retval != RESULT_SUCCESS)
    {
        show_mcc172_error(retval);
    }

    return NULL;
}


// A function to stop the acquisisiont that can be invoked
// from the worker thread
gboolean stop_acquisition()
{
    // Simulate a stop button press
    start_stop_event_handler(btnStart_Stop, NULL);

    return FALSE;
}


// Event handler for the Start/Stop button.
//
// If starting, change the button text to "Stop" and parse
// the UI settings before starting the acquisition.
// If stopping, change the button text to "Start" and stop the acquisition.
void start_stop_event_handler(GtkWidget *widget, gpointer data)
{
    const gchar* StartStopBtnLbl = gtk_button_get_label(GTK_BUTTON(widget));
    uint16_t options = 0;
    uint8_t clock_source;
    double actual_rate_per_channel;
    uint8_t synced;
    int retval = 0;

    if (strcmp(StartStopBtnLbl , "Start") == 0)
    {
        // Open the log file.
        log_file_ptr = open_log_file(csv_filename);

        if (log_file_ptr == NULL)
        {
            show_mcc172_error(UNABLE_TO_OPEN_FILE);
            done = TRUE;
            return;
        }

        set_enable_state_for_controls(FALSE);

        // Change the label on the start button to "Stop".
        gtk_button_set_label(GTK_BUTTON(widget), "Stop");

        done =  FALSE;

        // Set variables based on the UI settings.
        channel_mask = create_selected_channel_mask();

        gchar *fftText = gtk_combo_box_text_get_active_text(
                            GTK_COMBO_BOX_TEXT(comboBoxFftSize));
        iFftSize = atoi(fftText);

        iRatePerChannel =  gtk_spin_button_get_value_as_int(
                            GTK_SPIN_BUTTON(spinRate));

        set_iepe_configuration();

        mcc172_a_in_clock_config_write(address, SOURCE_LOCAL, iRatePerChannel);
        mcc172_a_in_clock_config_read(address, &clock_source,
                                      &actual_rate_per_channel, &synced);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinRate),
                                  actual_rate_per_channel);

        // Set the continuous option based on the UI setting.
        continuous =  gtk_toggle_button_get_active(
                            GTK_TOGGLE_BUTTON(rbContinuous));
        if (continuous == TRUE)
        {
            options |= OPTS_CONTINUOUS;
        }

        // Start the analog scan
        retval = mcc172_a_in_scan_start(address, channel_mask,
            10 * MAX_172_CHANNELS * iRatePerChannel, options);
        if (retval != RESULT_SUCCESS)
        {
            show_mcc172_error(retval);

            gtk_button_set_label(GTK_BUTTON(btnStart_Stop), "Start");
            done = TRUE;
            return;
        }

        // Start a thread to read the data from the device
        if (pthread_create(&threadh, NULL, &read_and_display_data, &tinfo) != 0)
        {
            printf("error creating thread..\n");
        }
    }
    else
    {
        done = TRUE;
        pthread_join(threadh, NULL);

        set_enable_state_for_controls(TRUE);

        // Change the label on the stop button to "Start".current
        gtk_button_set_label(GTK_BUTTON(widget), "Start");

        retval =  mcc172_a_in_scan_cleanup(address);
    }
}

// Event handler for the Select Log File button.
//
// Displays a GTK Open File dialog to select the log file to be opened.
// The file name will be shown in the main window.
void select_log_file_event_handler(GtkWidget* widget, gpointer user_data)
{
    // Get the initial file name.
    char* initial_filename = user_data;

    // Select the log file.
    strcpy(csv_filename, choose_log_file(window, initial_filename));

    // Display the CSV log file name.
    show_file_name();
}

// Event handler that is called when the application is launched to create
// the main window and its controls.
void activate_event_handler(GtkApplication *app, gpointer user_data)
{
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (cssProvider),
         "label#Chan0 {background-color: rgba(221, 50, 34, 1);}\n"
         "label#Chan1 {background-color: rgba(52, 130, 203, 1);}\n",
         -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                               GTK_STYLE_PROVIDER(cssProvider),
                               GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *hboxMain, *vboxMain;
    GtkWidget *hboxFile;
    GtkWidget *vboxConfig, *vboxSampleRateConfig, *vboxAcquireMode;
    GtkWidget *vboxButtons, *vboxGraph, *label;
    GtkWidget *hboxChannel, *hboxIepe, *hboxFftSize, *hboxRate;
    GtkWidget *vboxChannel, *vboxIepe, *vboxLegend;
    int i = 0;

    // Create the top level gtk window.
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window, 900, 700);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    // Create the GDK resources for the main window
    gtk_widget_realize(window);

    // Connect the event handler to the "delete_event" event
    g_signal_connect(window, "delete_event", G_CALLBACK (gtk_main_quit), NULL);

    // Display the CSV log file name.
    show_file_name();

    vboxMain = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_add(GTK_CONTAINER(window), vboxMain);

    hboxMain = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(vboxMain), hboxMain);

    vboxConfig = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(hboxMain), vboxConfig);

    hboxChannel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxChannel);

    label = gtk_label_new("    Channel select:  ");
    gtk_box_pack_start(GTK_BOX(hboxChannel), label, 0, 0, 0);

    vboxChannel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxChannel);

    vboxLegend=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxLegend);

    vboxSampleRateConfig = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(vboxConfig), vboxSampleRateConfig);

    hboxFftSize = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxSampleRateConfig), hboxFftSize);

    label = gtk_label_new("          FFT Size: ");
    gtk_box_pack_start(GTK_BOX(hboxFftSize), label, 0, 0, 0);

    hboxRate = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxSampleRateConfig), hboxRate);

    label = gtk_label_new("                 Rate: ");
    gtk_box_pack_start(GTK_BOX(hboxRate), label, 0, 0, 0);

    hboxIepe = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxIepe);

    label = gtk_label_new("    IEPE power on:  ");
    gtk_box_pack_start(GTK_BOX(hboxIepe), label, 0, 0, 0);

    vboxIepe = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxIepe), vboxIepe);

    char* chanBase = "Channel ";
    char* labelName = "Chan";
    i = 0;
    char intBuf[5];
    char chanNameBuffer[20];
    char labelNameBuffer[20];
    GtkWidget *legend[MAX_172_CHANNELS];
    for (i = 0; i < MAX_172_CHANNELS; i++)
    {
        strcpy(chanNameBuffer, chanBase);
        strcpy(labelNameBuffer, labelName);
        sprintf(intBuf, "%d", i);
        strcat(chanNameBuffer, intBuf);
        strcat(labelNameBuffer, intBuf);

        chkChan[i] = gtk_check_button_new_with_label(chanNameBuffer);
        gtk_box_pack_start(GTK_BOX(vboxChannel), chkChan[i], 0, 0, 0);

        legend[i] = gtk_label_new("  ");
        gtk_box_pack_start(GTK_BOX(vboxLegend), legend[i], 5, 0, 0);
        gtk_widget_set_name(legend[i], labelNameBuffer);

        chkIepe[i] = gtk_check_button_new_with_label(chanNameBuffer);
        gtk_box_pack_start(GTK_BOX(vboxIepe), chkIepe[i], 0, 0, 0);
   }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkChan[0]), TRUE);

    // Define FFT Size options
    comboBoxFftSize = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "256");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "512");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "1024");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "2048");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "4096");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "8192");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBoxFftSize), NULL, "16384");
    gtk_combo_box_set_active(GTK_COMBO_BOX(comboBoxFftSize), 3);
    gtk_box_pack_start(GTK_BOX(hboxFftSize), comboBoxFftSize, 0, 0, 0);

    // FILE indicator
    hboxFile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxMain), hboxFile);
    labelFile = gtk_label_new(csv_filename);

    spinRate = gtk_spin_button_new_with_range (10, 100000, 10);
    gtk_box_pack_start(GTK_BOX(hboxRate), spinRate, 0, 0, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinRate), 2048.);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER(hboxMain), separator);

    vboxGraph = gtk_box_new(GTK_ORIENTATION_VERTICAL, 50);
    gtk_container_add(GTK_CONTAINER(hboxMain), vboxGraph);

    // add the data graph
    gtk_databox_create_box_with_scrollbars_and_rulers_positioned (&dataBox,
        &dataTable, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
    gtk_box_pack_start(GTK_BOX(vboxGraph), dataTable, TRUE, TRUE, 0);

    GtkDataboxRuler* rulerY = gtk_databox_get_ruler_y(GTK_DATABOX(dataBox));
    gtk_databox_ruler_set_text_orientation(rulerY, GTK_ORIENTATION_HORIZONTAL);

    GtkDataboxRuler* rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(dataBox));
    gtk_databox_ruler_set_max_length(rulerX, 9);

    gchar* formatX = "%%.0Lf";
    gtk_databox_ruler_set_linear_label_format(rulerX, formatX);

    gtk_databox_ruler_set_range(rulerY, 6.0, -6.0, 0.0);
    gtk_databox_ruler_set_range(rulerX, 0.0, 2048.0, 0.0);

    gtk_databox_ruler_set_draw_subticks(rulerX, FALSE);

    // add the FFT graph
    gtk_databox_create_box_with_scrollbars_and_rulers_positioned (&fftBox,
        &fftTable, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
    gtk_box_pack_start(GTK_BOX(vboxGraph), fftTable, TRUE, TRUE, 0);

    rulerY = gtk_databox_get_ruler_y(GTK_DATABOX(fftBox));
    gtk_databox_ruler_set_text_orientation(rulerY, GTK_ORIENTATION_HORIZONTAL);

    rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(fftBox));
    gtk_databox_ruler_set_linear_label_format(rulerX, formatX);
    gtk_databox_ruler_set_draw_subticks(rulerX, FALSE);

    gtk_databox_ruler_set_range(rulerY, 10.0, -150.0, 0.0);
    gtk_databox_ruler_set_range(rulerX, 0.0, 1024.0, 0.0);

    // Set the background color for the graphs
    GdkRGBA background_color;
    background_color.red = 217.0/255.0;
    background_color.green = 217.0/255.0;
    background_color.blue = 217.0/255.0;
    background_color.alpha = 1;
    pgtk_widget_override_background_color(dataBox, GTK_STATE_FLAG_NORMAL,
                                          &background_color);
    pgtk_widget_override_background_color(fftBox, GTK_STATE_FLAG_NORMAL,
                                          &background_color);

    vboxAcquireMode= gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), vboxAcquireMode);

    rbContinuous = gtk_radio_button_new_with_label(NULL, "Continuous");
    gtk_box_pack_start(GTK_BOX(vboxAcquireMode), rbContinuous, 0, 0, 0);
    rbFinite = gtk_radio_button_new_with_label(NULL, "Finite");
    gtk_box_pack_start(GTK_BOX(vboxAcquireMode), rbFinite, 0, 0, 0);
    gtk_radio_button_join_group((GtkRadioButton*)rbFinite,
        (GtkRadioButton*)rbContinuous);

    vboxButtons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), vboxButtons);

    btnSelectLogFile = gtk_button_new_with_label ( "Select Log File ...");
    g_signal_connect(btnSelectLogFile, "clicked",
        G_CALLBACK(select_log_file_event_handler), csv_filename);
    gtk_box_pack_start(GTK_BOX(vboxButtons), btnSelectLogFile, 0, 0, 5);

    btnStart_Stop = gtk_button_new_with_label("Start");
    g_signal_connect(btnStart_Stop, "clicked",
        G_CALLBACK(start_stop_event_handler), NULL);
    gtk_box_pack_start(GTK_BOX(vboxButtons), btnStart_Stop, 0, 0, 5);

    btnQuit = gtk_button_new_with_label( "Quit");
    g_signal_connect(btnQuit, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(vboxButtons), btnQuit, TRUE, FALSE, 5);

    // FILE indicator
    gtk_box_pack_start(GTK_BOX(hboxFile), labelFile, TRUE, FALSE, 0);

    // Show the top level window and all of its controls
    gtk_widget_show_all(window);
}


// Display the CSV file name.
void show_file_name()
{
	if(labelFile)
		gtk_label_set_text(GTK_LABEL(labelFile), csv_filename);
}


// Find all of the HAT devices that are installed
// and open a connection to the first one.
int open_first_hat_device(uint8_t* hat_address)
{
    int hat_count = 0;
    struct HatInfo* hat_info_list = NULL;
    int retval = 0;

    // Get the number of MCC 172 devices so that we can determine
    // how much memory to allocate for the hat_info_list
    hat_count = hat_list(HAT_ID_MCC_172, NULL);

    if (hat_count > 0)
    {
        // Allocate memory for the hat_info_list
        hat_info_list = (struct HatInfo*)malloc(
            hat_count * sizeof(struct HatInfo));

        // Get the list of MCC 172 devices
        hat_list(HAT_ID_MCC_172, hat_info_list);
        // Choose the first one
        *hat_address = hat_info_list[0].address;

        // Open the hat device
        retval = mcc172_open(*hat_address);
        if (retval != RESULT_SUCCESS)
        {
            show_mcc172_error(retval);
        }
    }
    else
    {
        retval = NO_HAT_DEVICES_FOUND;
        show_mcc172_error(retval);
    }

    if (hat_info_list != NULL)
        free(hat_info_list);

    // Return the address of the first HAT device.
    return retval;
}
