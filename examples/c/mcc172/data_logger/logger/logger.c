#include <time.h>

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

    initialize_graph_channel_info();

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
    uint32_t numSamplesPerChannel)
{
    int i = 0;
    int chanMask = 0;
    int channel = 0;
    int num_channels = 0;

    // Delete the previous arrays for each of the channels (if they exist)
    for (i = 0; i < MAX_172_CHANNELS; i++)
    {
        if (graphChannelInfo[i].graph != NULL)
        {
            gtk_databox_graph_remove (GTK_DATABOX(dataBox),
                graphChannelInfo[i].graph);
            graphChannelInfo[i].graph= NULL;
        }

        if (graphChannelInfo[i].fft_graph != NULL)
        {
            gtk_databox_graph_remove (GTK_DATABOX(fftBox),
                graphChannelInfo[i].fft_graph);
            graphChannelInfo[i].fft_graph= NULL;
        }
    }

    // Create the new arrays for the current channels in the scan
    chanMask = current_channel_mask;
    channel = 0;
    for (i = 0; i < MAX_172_CHANNELS; i++)
    {
        // If this channel is in the scan, then allocate the arrays for the
        // indices (X) and data (Y)
        if (chanMask & 1)
        {
            graphChannelInfo[channel].X = g_new0 (gfloat, numSamplesPerChannel);
            graphChannelInfo[channel].Y = g_new0 (gfloat, numSamplesPerChannel);

            graphChannelInfo[channel].fft_X = g_new0 (gfloat, numSamplesPerChannel/2);
            graphChannelInfo[channel].fft_Y = g_new0 (gfloat, numSamplesPerChannel/2);

            num_channels++;
        }

        // Check next channel.
        channel++;
        chanMask >>= 1;
    }

    // Return the number of channels in the scan.
    return num_channels;
}


// Add each checked channel to the channel mask
int create_selected_channel_mask()
{
    gboolean checked_status = FALSE;
    int selected_channel_mask = 0;

    for (int i = 0; i < MAX_172_CHANNELS; i++)
    {
        // Is the channel checked?
        checked_status = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(chkChan[i]));

        // If checked, add the channel to the mask
        if (checked_status == TRUE)
        {
            selected_channel_mask += (int)pow(2,i);
        }
    }

    // return the channel mask
    return selected_channel_mask;
}


// Set te IEPE power configuration
void set_iepe_configuration()
{
    gboolean checked_status = FALSE;

    for (int i = 0; i < MAX_172_CHANNELS; i++)
    {
        // Is the channel checked?
        checked_status = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkIepe[i]));

        // If checked, add the channel to the mask
        if (checked_status == TRUE)
        {
            mcc172_iepe_config_write(address, i, checked_status);
        }
    }

    // return the channel mask
    return;
}


// Enable/disable the controls in the main window.
// Controls are disabled when the application is running
// and re-enabled when the application is stopped.
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
    gtk_widget_set_sensitive (spinNumSamples, state);

    // Set the state of the radio buttons
    gtk_widget_set_sensitive (rbFinite, state);
    gtk_widget_set_sensitive (rbContinuous, state);

    // Set the state of the buttons
    gtk_widget_set_sensitive (btnSelectLogFile, state);
    gtk_widget_set_sensitive (btnQuit, state);

}


int copy_hat_data_to_display_buffer(double* hat_read_buf, int samples_per_channel,
                                    double* display_buf,  int samples_per_channel_in_display_buf,
                                    int display_buf_size_in_samples, int num_channels)
{
    if (samples_per_channel_in_display_buf < display_buf_size_in_samples)
    {
        memcpy(&display_buf[samples_per_channel_in_display_buf* num_channels], hat_read_buf, samples_per_channel*sizeof(double) * num_channels);
        samples_per_channel_in_display_buf += samples_per_channel;
    }
    else if (samples_per_channel > samples_per_channel_in_display_buf)
    {
        memcpy(display_buf, &hat_read_buf[samples_per_channel*num_channels], (display_buf_size_in_samples - samples_per_channel)*sizeof(double) * num_channels);
    }
    else
    {
        memcpy(display_buf, &display_buf[samples_per_channel*num_channels], (display_buf_size_in_samples - samples_per_channel)*sizeof(double) * num_channels);
        memcpy(&display_buf[(display_buf_size_in_samples - samples_per_channel)*num_channels], hat_read_buf, samples_per_channel*sizeof(double) * num_channels);
    }

    if (samples_per_channel_in_display_buf > display_buf_size_in_samples)
        samples_per_channel_in_display_buf = display_buf_size_in_samples;

    return samples_per_channel_in_display_buf;
}


// Copy the data for the specified channel from the interleaved
// HAT buffer to the array for the specified channel.
void copy_data_to_xy_arrays(double* hat_read_buf, int read_buf_start_index,
    int channel, int stride, int buffer_size_samples, gboolean first_block)
{
    // Get the arrays for this channel
    gfloat* X = graphChannelInfo[channel].X;
    gfloat* Y = graphChannelInfo[channel].Y;

    int ii = 0;

    // For the first block, set the indices and data.
    // For all other blocks, just set the data
    if (first_block)
    {
        // Set indices and data
        for (int i = read_buf_start_index; i < buffer_size_samples; i+=stride)
        {
            X[ii] = (gfloat)ii;
            Y[ii] = (gfloat)hat_read_buf[i];

            ii++;
        }
    }
    else
    {
		// Set data only
		for (int i = read_buf_start_index; i < buffer_size_samples; i+=stride)
        {
            Y[ii] = (gfloat)hat_read_buf[i];

            ii++;
        }
    }
}


// Copy the data for the specified channel from the interleaved
// HAT buffer to the array for the specified channel.
void copy_fft_data_to_xy_arrays(double* spectrum, double rate_per_channel,
    int channel, int samples_per_channel, gboolean first_block)
{
    // Get the arrays for this channel
    gfloat* fft_X = graphChannelInfo[channel].fft_X;
    gfloat* fft_Y = graphChannelInfo[channel].fft_Y;

    int ii = 0;
    double f_i = 0;

    // For the first block, set the indices and data.
    // For all other blocks, just set the data
    if (first_block)
    {
        // Set indices and data
         for (int i = 0; i < samples_per_channel/2; i++)
        {
            fft_X[ii] = (gfloat)f_i;
            fft_Y[ii] = (gfloat)spectrum[i];

            f_i += rate_per_channel / samples_per_channel;

            ii++;
        }
    }
    else
    {
		// Set data onlyi
		for (int i = 0; i < samples_per_channel/2; i++)
        {
            fft_Y[ii] = (gfloat)spectrum[i];

            ii++;
        }
    }
}


// Refresh the graph with the new data.
gboolean refresh_graph()
{
    // set a mutex to prevent the data from changing while it is being plotted
    g_mutex_lock (&data_mutex);

    // Tell the graph to update
    gtk_widget_queue_draw(dataBox);
    gtk_widget_queue_draw(fftBox);

    // release the mutex
    g_mutex_unlock (&data_mutex);

    return FALSE;
}


// Wait for the scan to complete, read the data,
// write it to a CSV file, and plot it in the graph.
void analog_in_finite ()
{
    int chan_mask = 0;
    int channel = 0;
 	uint32_t samples_read_per_channel = 0;
    uint32_t buffer_size_samples = 0;
    int retval = 0;
    uint16_t read_status = 0;

    // Allocate arrays for the indices and data
    // for each channel in the scan.
    int num_channels = allocate_channel_xy_arrays(channel_mask,
        iNumSamplesPerChannel);

    // Set the timeout
    double scan_timeout = num_channels *
		iNumSamplesPerChannel / iRatePerChannel * 10;

    // Setup the sample buffer.
    double hat_read_buf[iNumSamplesPerChannel*num_channels];
    buffer_size_samples = iNumSamplesPerChannel*num_channels;

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

		return;
	}

    // Wait for the scan to start running.
    do
    {
        retval = mcc172_a_in_scan_status(address, &read_status,
            &samples_read_per_channel);
    }
    while ((retval == RESULT_SUCCESS) &&
           ((read_status & STATUS_RUNNING) != STATUS_RUNNING));

    if (retval == RESULT_SUCCESS)

    {
        // Read all the samples requested
        retval = mcc172_a_in_scan_read(address, &read_status,
            iNumSamplesPerChannel, scan_timeout, hat_read_buf,
            buffer_size_samples, &samples_read_per_channel);
        if (retval != RESULT_SUCCESS)
        {
            show_mcc172_error(retval);
        }
        else if (read_status & STATUS_HW_OVERRUN)
        {
            show_mcc172_error(HW_OVERRUN);
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            show_mcc172_error(BUFFER_OVERRUN);
        }
    }

    double* spectrum = (double*)malloc(sizeof(double) * (iNumSamplesPerChannel/2 + 1));

    if (retval == RESULT_SUCCESS)
    {
        // Write the data to the CSV file.
        retval = write_log_file(log_file_ptr, hat_read_buf,
            iNumSamplesPerChannel, num_channels);
        if (retval < 0)
        {
            switch (retval)
            {
            case -1:
                show_mcc172_error(MAXIMUM_FILE_SIZE_EXCEEDED);
                break;

            default:
                show_mcc172_error(UNKNOWN_ERROR);
                break;
            }
        }
    }

    // Parse the data and display it on the graph.
    // Convert the 1D interleaved data into a 2D array
    chan_mask = channel_mask;
    channel = 0;
    int read_buf_index = 0;
    int index = 0;

    // Display the data for each channel.
    while (chan_mask > 0)
    {
        // Is this channel part of the scan?
        if (chan_mask & 1)
        {
            // Get the data for this channel                // Calculate and display the FFT.
            calculate_real_fft(hat_read_buf, iNumSamplesPerChannel, num_channels, index++,
                mcc172_info()->AI_MAX_RANGE, spectrum, TRUE);


            copy_data_to_xy_arrays(hat_read_buf, read_buf_index, channel,
                num_channels, buffer_size_samples, TRUE);

            // Graph the data
            gfloat* X = graphChannelInfo[channel].X;
            gfloat* Y = graphChannelInfo[channel].Y;

            graphChannelInfo[channel].graph =
                gtk_databox_lines_new ((guint)iNumSamplesPerChannel, X, Y,
                graphChannelInfo[channel].color, 2);

            gtk_databox_graph_add(GTK_DATABOX (dataBox),
                GTK_DATABOX_GRAPH(graphChannelInfo[channel].graph));

            gtk_databox_set_total_limits(GTK_DATABOX (dataBox), 0.0,
                (gfloat)iNumSamplesPerChannel, 10.0, -10.0);

            copy_fft_data_to_xy_arrays(spectrum, iRatePerChannel,
                channel, iNumSamplesPerChannel, TRUE);

            gfloat* fft_X = graphChannelInfo[channel].fft_X;
            gfloat* fft_Y = graphChannelInfo[channel].fft_Y;

            graphChannelInfo[channel].fft_graph = gtk_databox_lines_new
                ((guint)iNumSamplesPerChannel/2, fft_X, fft_Y,
                graphChannelInfo[channel].color, 2);

            gtk_databox_graph_add(GTK_DATABOX (fftBox),
                GTK_DATABOX_GRAPH(graphChannelInfo[channel].fft_graph));

            gtk_databox_set_total_limits(GTK_DATABOX (fftBox), 0.0,
                (gfloat)iNumSamplesPerChannel/2, 0.0, -150.0);

            read_buf_index++;
        }

        // Next channel in the mask
        channel++;
        chan_mask >>= 1;
     }

    // Re-enable all of the controls in the min window.
    set_enable_state_for_controls(TRUE);

    // Stop the scan completes
    retval = mcc172_a_in_scan_stop(address);
    if (retval != RESULT_SUCCESS)
    {
        show_mcc172_error(retval);
    }

    // Clean up after the scan completes
    retval = mcc172_a_in_scan_cleanup(address);
    if (retval != RESULT_SUCCESS)
    {
        show_mcc172_error(retval);
    }

    gtk_button_set_label(GTK_BUTTON(btnStart_Stop), "Start");
}

// While the scan is running, read the data, write it to a
// CSV file, and plot it in the graph.
// This function runs as a background thread
// for the duration of the scan.
static void * analog_in_continuous ()
{
	////////int error_code;
    int chanMask = 0;
    int channel = 0;
    uint32_t samples_read_per_channel = 0;
    uint32_t buffer_size_samples = 0;
    int retval = 0;
    uint16_t read_status;

    // Allocate arrays for the indices and data
    // for each channel in the scan.
    int num_channels = allocate_channel_xy_arrays(channel_mask,
        iNumSamplesPerChannel);

    // Set the timeout
    double scan_timeout = num_channels *
		iNumSamplesPerChannel / iRatePerChannel * 10;

    // Setup the sample buffer.
    buffer_size_samples = iNumSamplesPerChannel*num_channels;
    double hat_read_buf[2*(int)iRatePerChannel*num_channels];

    double display_buf[buffer_size_samples];
    int samples_in_display_buf = 0;

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
                (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);

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
                    (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);
            }

            retval = mcc172_a_in_scan_cleanup(address);
            if (retval != RESULT_SUCCESS)
            {
                // Error dialog must be displayed on the main thread.
                error_code = retval;
                g_main_context_invoke(context,
                    (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);
            }

            done = TRUE;
            pthread_join (threadh, NULL);

            gtk_button_set_label (GTK_BUTTON(btnStart_Stop), "Start");
            return NULL;
        }
    } while ((retval == RESULT_SUCCESS) &&
             ((read_status & STATUS_RUNNING) != STATUS_RUNNING));

    double* spectrum = (double*)malloc(sizeof(double) * (iNumSamplesPerChannel/2 + 1));

    // Loop to read data continuously
    gboolean first_block = TRUE;
    while  (done == FALSE)
    {
        // Read the data from the device
        samples_read_per_channel = 0;

        retval = mcc172_a_in_scan_read(address, &read_status,
            READ_ALL_AVAILABLE, scan_timeout, hat_read_buf,
            buffer_size_samples, &samples_read_per_channel);


        if (retval != RESULT_SUCCESS)
        {
            // Error dialog must be displayed on the main thread.
            error_code = retval;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);
            break;
        }
        else if (read_status & STATUS_HW_OVERRUN)
        {
            // Error dialog must be displayed on the main thread.
            error_code = HW_OVERRUN;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);
            break;
        }
        else if (read_status & STATUS_BUFFER_OVERRUN)
        {
            // Error dialog must be displayed on the main thread.
            error_code = BUFFER_OVERRUN;
            g_main_context_invoke(context,
                (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);
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
                (GSourceFunc)show_mcc172_error_main_thread, (gpointer)&error_code);

            // Call the Start?Stop event handler to reset the UI
            start_stop_event_handler(btnStart_Stop, NULL);

            done = TRUE;

            break;
        }

        samples_in_display_buf = copy_hat_data_to_display_buffer(hat_read_buf, samples_read_per_channel,
                                        display_buf, samples_in_display_buf,
                                        iNumSamplesPerChannel, num_channels);

        if (samples_in_display_buf >= iNumSamplesPerChannel)
        {
            // Set a mutex to prevent the data from changing while we plot it
            g_mutex_lock (&data_mutex);

            chanMask = channel_mask;
            channel = 0;
            int read_buf_index = 0;
            int index = 0;

            // While there are channels to plot.
            while (chanMask > 0)
            {
                // If this channel is included in the acquisition, plot its data.
                if (chanMask & 1)
                {
                    // Calculate and display the FFT.
                    calculate_real_fft(display_buf, iNumSamplesPerChannel, num_channels, index++,
                        mcc172_info()->AI_MAX_RANGE, spectrum, first_block);

                    if (first_block)
                    {
                        // If this is the first block we need to set the indices and
                        // the data.
                        copy_data_to_xy_arrays(display_buf, read_buf_index,
                            channel, num_channels, buffer_size_samples, first_block);

                        gfloat* X = graphChannelInfo[channel].X;
                        gfloat* Y = graphChannelInfo[channel].Y;

                        graphChannelInfo[channel].graph = gtk_databox_lines_new
                            ((guint)iNumSamplesPerChannel, X, Y,
                            graphChannelInfo[channel].color, 2);

                        gtk_databox_graph_add(GTK_DATABOX (dataBox),
                            GTK_DATABOX_GRAPH(graphChannelInfo[channel].graph));

                        gtk_databox_set_total_limits(GTK_DATABOX (dataBox), 0.0,
                            (gfloat)iNumSamplesPerChannel, 10.0, -10.0);

                        copy_fft_data_to_xy_arrays(spectrum, iRatePerChannel,
                            channel, iNumSamplesPerChannel, first_block);

                        gfloat* fft_X = graphChannelInfo[channel].fft_X;
                        gfloat* fft_Y = graphChannelInfo[channel].fft_Y;

                        graphChannelInfo[channel].fft_graph = gtk_databox_lines_new
                            ((guint)iNumSamplesPerChannel/2, fft_X, fft_Y,
                            graphChannelInfo[channel].color, 2);

                        gtk_databox_graph_add(GTK_DATABOX (fftBox),
                            GTK_DATABOX_GRAPH(graphChannelInfo[channel].fft_graph));

                        gtk_databox_set_total_limits(GTK_DATABOX (fftBox), 0.0,
                            (gfloat)iNumSamplesPerChannel/2, 0.0, -150.0);
                    }
                    else
                    {

                        // If this is not the first block, just update the data.
                        copy_data_to_xy_arrays(display_buf, read_buf_index,
                            channel, num_channels, buffer_size_samples, first_block);

                        copy_fft_data_to_xy_arrays(spectrum, iRatePerChannel,
                            channel, iNumSamplesPerChannel, first_block);
                    }
                    // Set the index to start at the first
                    // sample of the next channel.
                    read_buf_index++;
                }
                channel++;
                chanMask >>= 1;
            }

            samples_in_display_buf = 0;

            // Done with the data so fill the buffer with zeros.
            memset(hat_read_buf, 0, buffer_size_samples * sizeof(double));

            first_block = FALSE;

            // Set the condition to cause the display
            // update function to update the display.
            g_main_context_invoke(context, (GSourceFunc)refresh_graph, NULL);

            // Release the mutex
            g_mutex_unlock(&data_mutex);

            // Allow idle time for the display to update
            usleep(1);
        }
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

// Event handler for the Start/Stop button.
//
// If starting, change the button text to "Stop" and parse
// the UI settings before starting the acquisition.
// If stopping, change the button text
// to "Start" and stop the acquisition.
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

        iNumSamplesPerChannel = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(spinNumSamples));

        iRatePerChannel =  gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(spinRate));

        set_iepe_configuration();

        mcc172_a_in_clock_config_write(address, SOURCE_LOCAL, iRatePerChannel);
        mcc172_a_in_clock_config_read(address, &clock_source, &actual_rate_per_channel, &synced);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinRate), actual_rate_per_channel);

        // Set the continuous option based on the UI setting.
        gboolean bContinuous =  gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(rbContinuous));
        if (bContinuous == TRUE)
        {
            options |= OPTS_CONTINUOUS;
        }

        // Start the analog scan.g_main_context_default
        retval = mcc172_a_in_scan_start(address, channel_mask,
            10 * MAX_172_CHANNELS * iRatePerChannel, options);
        if (retval != RESULT_SUCCESS)
        {
            show_mcc172_error(retval);

            gtk_button_set_label(GTK_BUTTON(btnStart_Stop), "Start");
            done = TRUE;
            return;
        }

        if ((options & OPTS_CONTINUOUS) == OPTS_CONTINUOUS)
        {
            // If continuous scan, start a thread
            // to read the data from the device
            if (pthread_create(&threadh, NULL, &analog_in_continuous, &tinfo) != 0)
            {
                printf("error creating thread..\n");
            }
        }
        else
        {
            // Its a finite scan, so call function to wait for all of the
            // samples to be acquired
            analog_in_finite();
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
    show_file_name();////////        // channel 2

}


///////////////////////////////////////////////////////////////////////////////
//
// The following functions are used to initialize the user interface.  These
// functions include displaying the user interface, setting g_main_context_defaultchannel colors,
// and selecting the first available MCC 172 HAT device.
//
///////////////////////////////////////////////////////////////////////////////


//   Assign a legend color and channel number for each channel on the device.
void initialize_graph_channel_info (void)
{
    for (int i = 0; i < MAX_172_CHANNELS; i++)
    {
        switch(i)
        {
        // channel 0
        case 0:
        default:
            legendColor[i].red = 1;
            legendColor[i].green = 0;
            legendColor[i].blue = 0;
            legendColor[i].alpha = 1;

            graphChannelInfo[i].color = &legendColor[i];
            graphChannelInfo[i].channelNumber = i;
            break;
        // channel 1
        case 1:
            legendColor[i].red = 0;
            legendColor[i].green = 1;
            legendColor[i].blue = 0;
            legendColor[i].alpha = 1;

            graphChannelInfo[i].color = &legendColor[i];
            graphChannelInfo[i].channelNumber = i;
           break;
        }
    }
}


// Event handler that is called when the application is launched to create
// the main window and its controls.
void activate_event_handler(GtkApplication *app, gpointer user_data)
{
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, "theme.css", NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                               GTK_STYLE_PROVIDER(cssProvider),
                               GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *hboxMain, *vboxMain;
    GtkWidget *hboxFile;
    GtkWidget *vboxConfig, *vboxSampleRateConfig, *vboxAcquireMode, *vboxButtons, *vboxGraph, *label;
    GtkWidget *hboxChannel, *hboxIepe, *hboxNumSamples, *hboxRate;
    GtkWidget *vboxChannel, *vboxIepe, *vboxLegend;
    GtkDataboxGraph *dataGraph, *fftGraph;
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

    hboxNumSamples = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxSampleRateConfig), hboxNumSamples);

    label = gtk_label_new("Num Samples: ");
    gtk_box_pack_start(GTK_BOX(hboxNumSamples), label, 0, 0, 0);

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

    spinNumSamples = gtk_spin_button_new_with_range (10, 100000, 10);
    gtk_box_pack_start(GTK_BOX(hboxNumSamples), spinNumSamples, 0, 0, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinNumSamples), 500.);

    // FILE indicator
    hboxFile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxMain), hboxFile);
    labelFile = gtk_label_new(csv_filename);


    spinRate = gtk_spin_button_new_with_range (10, 100000, 10);
    gtk_box_pack_start(GTK_BOX(hboxRate), spinRate, 0, 0, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinRate), 1000.);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER(hboxMain), separator);

    vboxGraph = gtk_box_new(GTK_ORIENTATION_VERTICAL, 50);
    gtk_container_add(GTK_CONTAINER(hboxMain), vboxGraph);

    // add the data graph
    gtk_databox_create_box_with_scrollbars_and_rulers_positioned (&dataBox, &dataTable,
        FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
    gtk_box_pack_start(GTK_BOX(vboxGraph), dataTable, TRUE, TRUE, 0);

    GtkDataboxRuler* rulerY = gtk_databox_get_ruler_y(GTK_DATABOX(dataBox));
    gtk_databox_ruler_set_text_orientation(rulerY, GTK_ORIENTATION_HORIZONTAL);

    GtkDataboxRuler* rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(dataBox));
    gchar* formatX = "%%6.0lf";
    gtk_databox_ruler_set_linear_label_format(rulerX, formatX);
    gtk_databox_ruler_set_draw_subticks(rulerX, FALSE);

    gtk_databox_ruler_set_range(rulerY, 10.0, -10.0, 0.0);
    gtk_databox_ruler_set_range(rulerX, 0.0, 500.0, 0.0);

    GdkRGBA grid_color;
    grid_color.red = 0;
    grid_color.green = 0;
    grid_color.blue = 0;
    grid_color.alpha = 0.3;
    dataGraph = (GtkDataboxGraph*)gtk_databox_grid_new(7, 9, &grid_color, 1);
    gtk_databox_graph_add(GTK_DATABOX (dataBox), GTK_DATABOX_GRAPH(dataGraph));

    // add the FFT graph
    gtk_databox_create_box_with_scrollbars_and_rulers_positioned (&fftBox, &fftTable,
        FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
    gtk_box_pack_start(GTK_BOX(vboxGraph), fftTable, TRUE, TRUE, 0);

    rulerY = gtk_databox_get_ruler_y(GTK_DATABOX(fftBox));
    gtk_databox_ruler_set_text_orientation(rulerY, GTK_ORIENTATION_HORIZONTAL);

    rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(fftBox));
    formatX = "%%6.0lf";
    gtk_databox_ruler_set_linear_label_format(rulerX, formatX);
    gtk_databox_ruler_set_draw_subticks(rulerX, FALSE);

    gtk_databox_ruler_set_range(rulerY, 10.0, -10.0, 0.0);
    gtk_databox_ruler_set_range(rulerX, 0.0, 500.0, 0.0);

    grid_color.red = 0;
    grid_color.green = 0;
    grid_color.blue = 0;
    grid_color.alpha = 0.3;
    fftGraph = (GtkDataboxGraph*)gtk_databox_grid_new(7, 9, &grid_color, 1);
    gtk_databox_graph_add(GTK_DATABOX (fftBox), GTK_DATABOX_GRAPH(fftGraph));

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

        // Get the list of MCC 172s.
        hat_list(HAT_ID_MCC_172, hat_info_list);

        // This application will use the first device (i.e. hat_info_list[0])
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
