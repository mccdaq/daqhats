#include <string.h>
#include <math.h>

#include "logger.h"


int main(void)
{
    int retval = 0;
    GtkApplication *app;

    // Set the application name.
    strcpy(application_name, "MCC 134 Data Logger");

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
        error_context = g_main_context_default();

        // Start the GTK message loop.
        gtk_main();

        // Close the device.
        mcc134_close(address);
    }

    //Exit app...
    return 0;
}


// Allocate arrays for the indices and data for each channel in the scan.
void allocate_channel_xy_arrays(int channel, int numSamplesPerChannel)
{
    // Create the new arrays for the current channels in the scan
    graphChannelInfo[channel].X = g_new0 (gfloat, numSamplesPerChannel);

    graphChannelInfo[channel].Y = g_new0 (gfloat, numSamplesPerChannel);
}


// Enable/disable the controls in the main window.
// Controls are disabled when the application is running
// and re-enabled when the application is stopped.
void set_enable_state_for_controls(gboolean state)
{
    gboolean checked_status = FALSE;
    int i;
    
    // Set the state of the check boxes and CJC combo boxex
    for (i = 0; i < MAX_134_TEMP_CHANNELS; i++)
    {
        gtk_widget_set_sensitive (chkChan[i], state);

        if (state == FALSE)
        {
            // If the channel is being disabled, then disable its CJC combo box.
            gtk_widget_set_sensitive (comboTcType[i], FALSE);
        }
        else
        {
            // Channel check boxes are being enabled.  If the channel
            // is checked, enable the CJC combo box; otherwise, disable
            // the CJC combo box
            checked_status = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chkChan[i]));
            if (checked_status == TRUE)
                gtk_widget_set_sensitive (comboTcType[i], TRUE);
            else
                gtk_widget_set_sensitive (comboTcType[i],FALSE);
        }

    }

    // Set the state of the text boxes
    gtk_widget_set_sensitive (spinRate, state);
    gtk_widget_set_sensitive (comboReadIntervalUnits, state);
    gtk_widget_set_sensitive (spinNumSamples, state);

    // Set the state of the buttons
    gtk_widget_set_sensitive (btnSelectLogFile, state);
    gtk_widget_set_sensitive (btnQuit, state);

}


// Copy X anf Y data to the left by one sample to make room at the end
// of the arrays the the next sample.
void copy_xy_arrays(int channel, int num_samples_to_copy)
{
    // Get the array pointers.
    gfloat* X = graphChannelInfo[channel].X;
    gfloat* Y = graphChannelInfo[channel].Y;

    int i;
    // Copy each sample and index to the previous sample location.
    for (i = 0; i < num_samples_to_copy; i++)
    {
        X[i] = X[i+1];
        Y[i] = Y[i+1];
    }
}


// Refresh the graph with the new data.
void refresh_graph(GtkWidget *box)
{
    gfloat min_x, max_x, min_y, max_y;

    // Check if the Stop button has been pressed.
    if (done == TRUE)
        return;

    // We need at least 2 samples to be able to draw a line in the graph.
    if (total_samples_read < 2)
        return;

    // Get the min and max values for the X anf Y arrays.
    gtk_databox_calculate_extrema(GTK_DATABOX(box), &min_x, &max_x, &min_y, &max_y);

    int num_grid_lines = total_samples_read - 2;
    int num_ticks = max_x - min_x + 1;
    num_ticks = (num_ticks < 9) ? num_ticks : 9;

    if (num_grid_lines > 0)
    {
        // Remove the old grid from the graph.
        gtk_databox_graph_remove(GTK_DATABOX (box), grid_x);

        // Clip the number of grid lines
        if (num_grid_lines > 9)
            num_grid_lines = 9;

        // Create a new grid with the current number of grid lines and add
        // it to the graph.
        GdkRGBA grid_color;
        grid_color.red = 0;
        grid_color.green = 0;
        grid_color.blue = 0;
        grid_color.alpha = 0.3;
        grid_x = (GtkDataboxGraph*)gtk_databox_grid_new(7, num_grid_lines, &grid_color, 1);
        gtk_databox_graph_add(GTK_DATABOX (box), GTK_DATABOX_GRAPH(grid_x));

        // Set the number of ticks in the ruler for the X axis.
        GtkDataboxRuler* rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(box));
        gtk_databox_ruler_set_manual_tick_cnt(rulerX, num_ticks);

        // Set the first tick value.
        gfloat tv = min_x;

        // Calculate the increment between tick values by dividing
        // the X axis range by the number of ticks.
        gfloat inc = (max_x - min_x + 1) / num_ticks;

        // Calculate the values for the tick marks.
        gfloat* tick_values = g_new0(gfloat, num_ticks);
        int i;
        for (i=0; i<num_ticks; i++)
        {
            tick_values[i] = tv;
            tv += inc;
        }

        // St the ruler with the new tick marks.
        gtk_databox_ruler_set_manual_ticks(rulerX, tick_values);
    }

    // Set the X and Y limits for auto scaling.
    gtk_databox_set_total_limits(GTK_DATABOX (box), min_x, max_x,
        max_y+0.1, min_y-0.1);

    // Tell the graph to update
    gtk_widget_queue_draw(box);
}


// Timer function to read data asynchronously from the MCC134.  It reads
// a value for each channel, stores the values to a CSV file, and the plots
// the values in the graph.
static gboolean read_data(gpointer data)
{
    uint8_t chan_mask = channel_mask;
    int channel = 0;
    int number_of_channels = 0;
    int buffer_index = 0;
    int retval = 0;

    // Read the data for each channel that is enabled.
    while (chan_mask != 0)
    {
        if (chan_mask & 1)
        {
            retval = mcc134_t_in_read(address, channel, &data_buffer[buffer_index++]);
            number_of_channels++;
        }

        channel++;
        chan_mask >>= 1;
    }

    // Increment the number of samples read for each channel.
    total_samples_read++;

    // Write the data to a log file as CSV data.
    retval = write_log_file(log_file_ptr, data_buffer, 1, number_of_channels);
    if (retval < 0)
    {
        int error_code;
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
            (GSourceFunc)show_mcc134_error_main_thread, &error_code);

        // Call the Start?Stop event handler to reset the UI
        start_stop_event_handler(btnStart_Stop, NULL);

        done = TRUE;
    }


    chan_mask = channel_mask;
    channel = 0;
    int read_buf_index = 0;

    // While there are channels to plot.
    while (chan_mask > 0)
    {
        // If this channel is included in the acquisition, plot its data.
        if (chan_mask & 1)
        {
            // If the display is not full, then add a sample to the end of the
            // array; otherwise, move each samle left 1 spot and add the new
            // sample at the end of the array.
            if (total_samples_read <= numSamplesToDisplay)
            {
                // Display is not full, so just add a sample to the end.

                // Remove the old plot for the current channel.
                if (graphChannelInfo[channel].graph != NULL)
                {
                    gtk_databox_graph_remove(GTK_DATABOX (box),
                        graphChannelInfo[channel].graph);
                }

                // Get the X and Y arrays.
                gfloat* X = graphChannelInfo[channel].X;
                gfloat* Y = graphChannelInfo[channel].Y;

                // Add a sample to the end.
                X[total_samples_read-1] = total_samples_read - 1;
                Y[total_samples_read-1] = (gfloat)data_buffer[read_buf_index];

                // Create a new plot for the samples.
                graphChannelInfo[channel].graph = gtk_databox_lines_new
                    ((guint)total_samples_read, X, Y,
                    graphChannelInfo[channel].color, 2);

                // Add the plot for the current channel to the graph.
                gtk_databox_graph_add(GTK_DATABOX (box),
                    GTK_DATABOX_GRAPH(graphChannelInfo[channel].graph));
            }
            else
            {
                // Display is full, move each sample to the left by one position anofd
                // add a new sample to the end of the array.

                // Since the size of the plots has not changed (ie the display is full),
                // reuse the plots from the previous interation.

                // Move sample left by 1 position.
                copy_xy_arrays(channel, numSamplesToDisplay-1);

                // Get the X and Y arrays.
                gfloat* X = graphChannelInfo[channel].X;
                gfloat* Y = graphChannelInfo[channel].Y;

                // Add the current sample and index to the end of the arrays.
                X[numSamplesToDisplay-1] = total_samples_read - 1;
                Y[numSamplesToDisplay-1] = (gfloat)data_buffer[read_buf_index];
            }

            // Increment the index to the sample for the next channel.
            read_buf_index++;
        }
        channel++;
        chan_mask >>= 1;
    }

    // Update the display.
    refresh_graph(box);

    // return FALSE to have the timer call this function again, or
    // return TRUE to stop calling this function
    if (done == FALSE)
        return TRUE;
    else
        return FALSE;
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
    uint32_t read_interval_seconds = 0;

    total_samples_read = 0;

    if (strcmp(StartStopBtnLbl , "Start") == 0)
    {
        done =  FALSE;

        // Open the log file.
        log_file_ptr = open_log_file(csv_filename);

        if (log_file_ptr == NULL)
        {
            show_mcc134_error(UNABLE_TO_OPEN_FILE);
            done = TRUE;
            return;
        }

        // Write channel numbers to file header
        init_log_file(log_file_ptr, channel_mask);

        // Disable the controls while acquiring data.
        set_enable_state_for_controls(FALSE);

        // Change the label on the start button to "Stop".
        gtk_button_set_label(GTK_BUTTON(widget), "Stop");

        // Get the number of samles to be displayed.
        numSamplesToDisplay = gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(spinNumSamples));
        int i;
        for (i = 0; i < MAX_134_TEMP_CHANNELS; i++)
        {
            if (graphChannelInfo[i].graph != NULL)
            {
                gtk_databox_graph_remove (GTK_DATABOX(box),
                    graphChannelInfo[i].graph);
                graphChannelInfo[i].graph = NULL;
            }
        }

        // Allocate arrays for the indices and data
        // for each channel in the scan.

        int channel = 0;
        int chan_mask = channel_mask;
        while (chan_mask != 0)
        {
            if (chan_mask & 1)
            {
                allocate_channel_xy_arrays(channel, numSamplesToDisplay);
            }

            channel++;
            chan_mask >>= 1;
        }

        // Get how often to read a channel.
        ratePerChannel =  gtk_spin_button_get_value_as_int(
            GTK_SPIN_BUTTON(spinRate));

        // Get the units for how often to read a channel.
        rateUnits = gtk_combo_box_get_active (GTK_COMBO_BOX(comboReadIntervalUnits));

        // Calculate the number of seconds between reads of a channel.
        switch (rateUnits)
        {
            // Seconds
            case 0:
                read_interval_seconds = (uint32_t)ratePerChannel;
                break;

            // Minutes
            case 1:
                read_interval_seconds = (uint32_t)ratePerChannel * 60;
                break;

            // Hours
            case 2:
                read_interval_seconds = (uint32_t)ratePerChannel * 60 * 60;
                break;

            // Days
            case 3:
                read_interval_seconds = (uint32_t)ratePerChannel * 60 * 60 * 24;
                break;
        }

        context = g_main_context_default();

        // Set the TC type for each active channel.
        chan_mask = channel_mask;
        channel = 0;
        while (chan_mask != 0)
        {
            // Enable the channel for reading by setting read_dataits TC type.
            if (chan_mask & 1)
            {
                mcc134_tc_type_write(address, channel, selected_tc_type[channel]);
            }

            channel++;;
            chan_mask >>= 1;
        }

        // Read the first sample immediately
        read_data(NULL);

        // Start the timer thread to read the data.
        g_timeout_add_seconds (read_interval_seconds, read_data, NULL);
    }
    else
    {
        set_enable_state_for_controls(TRUE);

        // Change the label on the stop button to "Start".current
        gtk_button_set_label(GTK_BUTTON(widget), "Start");

        done = TRUE;
    }
}


// Event handler for the Channel check box.
//
// Enable/disable controls associated with the channel.
void channel_button_clicked(GtkWidget* widget, gpointer user_data)
{
    int i = (int)user_data;
    int checked = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (checked == TRUE)
    {
        // Add the channel to the mask and enable the channel's TC control.
        channel_mask += (int)pow(2,i);
        gtk_widget_set_sensitive (comboTcType[i], TRUE);
    }
    else
    {
        // Remove the channel from the mask and disable the channel's TC control.
        channel_mask -= (int)pow(2,i);
        gtk_widget_set_sensitive (comboTcType[i], FALSE);
    }
}


// Event handler for the Thermocoupled Type Combo Box.
//
// Stores the TC type in the TC Type array.
void tc_type_changed(GtkWidget* widget, gpointer user_data)
{
    int channel = (int)user_data;
    int selection = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    selected_tc_type[channel] = selection;
}


// Event handler for the Rate Units Combo Box.
//
// Stores the seleced rate units.
void rate_units_changed(GtkWidget* widget, gpointer user_data)
{
    int selection = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    selected_rate_units = selection;
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


///////////////////////////////////////////////////////////////////////////////
//
// The following functions are used to initialize the user interface.  These
// functions include displaying the user interface, setting channel colors,
// and selecting the first available MCC 134 HAT device.
//
///////////////////////////////////////////////////////////////////////////////


//   Assign a legend color and channel number for each channel on the device.
void initialize_graph_channel_info (void)
{
    int i;
    for (i = 0; i < MAX_134_TEMP_CHANNELS; i++)
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
        // channel 2
        case 2:
            legendColor[i].red = 0;
            legendColor[i].green = 0;
            legendColor[i].blue = 1;
            legendColor[i].alpha = 1;

            graphChannelInfo[i].color = &legendColor[i];
            graphChannelInfo[i].channelNumber = i;
           break;
        // channel 3
        case 3:
            legendColor[i].red = 1;
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
    GtkWidget *vboxConfig;

    GtkWidget *rateLabel, *numSamplesLabel;
    GtkWidget *hboxChannel;
    GtkWidget *vboxChannel, *vboxChannelLegend;
////////    GtkWidget *vboxCJC, *vboxCJCLegend;
    GtkWidget *hboxRate, *hboxNumSamples;

    GtkWidget *hboxTcType, *vboxTcType;
////////    GtkDataboxGraph* graph;
////////    int i = 0;

    char* tcType[] = {"J", "K", "T", "E", "R", "S", "B", "N"};
    char* rateUnits[] = {"Sec", "Min", "Hour", "Day"};

    // Create the top level gtk window.
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window, 900, 500);
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

    hboxChannel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxChannel);

    vboxChannel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxChannel);

    vboxChannelLegend = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxChannelLegend);

////////    vboxCJC = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
////////    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxCJC);
////////
////////    vboxCJCLegend = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
////////    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxCJCLegend);

    hboxTcType = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxTcType);

    vboxTcType = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxTcType);

    char* chanBase = "Channel ";
    char* labelName = "Chan";
////////    i = 0;
    char chanNumBuf[5];
    char labelNumBuf[5];
    char chanNameBuffer[20];
    char labelNameBuffer[20];
    GtkWidget *legend[MAX_134_TEMP_CHANNELS];

    // Add the temperature channel check boxes.
    int i;
    for (i = 0; i < MAX_134_TEMP_CHANNELS; i++)
    {
        strcpy(chanNameBuffer, chanBase);
        strcpy(labelNameBuffer, labelName);
        sprintf(chanNumBuf, "%d", i);
        sprintf(labelNumBuf, "%d", i);
        strcat(chanNameBuffer, chanNumBuf);
        strcat(labelNameBuffer, labelNumBuf);

        chkChan[i] = gtk_check_button_new_with_label(chanNameBuffer);
        gtk_box_pack_start(GTK_BOX(vboxChannel), chkChan[i], TRUE, 0, 0);

        g_signal_connect(GTK_TOGGLE_BUTTON(chkChan[i]), "clicked", G_CALLBACK(channel_button_clicked), (gpointer)i);

        legend[i] = gtk_label_new("  ");
        gtk_box_pack_start(GTK_BOX(vboxChannelLegend), legend[i], TRUE, 0, 0);
        gtk_widget_set_name(legend[i], labelNameBuffer);
    }


    // Add the thermocouple type combo boxes.
    for (i = 0; i < MAX_134_CJC_CHANNELS; i++)
    {
        comboTcType[i] = gtk_combo_box_text_new ();
        int j;
        for (j = 0; j < 8; j++)
        {
            gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(comboTcType[i]), 0, tcType[j]);
        }
        gtk_box_pack_start(GTK_BOX(vboxTcType), comboTcType[i], TRUE, 0, 0);

        g_signal_connect(GTK_COMBO_BOX_TEXT(comboTcType[i]), "changed", G_CALLBACK(tc_type_changed), (gpointer)i);

        gtk_combo_box_set_active (GTK_COMBO_BOX(comboTcType[i]), 0);
        selected_tc_type[i] = gtk_combo_box_get_active (GTK_COMBO_BOX(comboTcType[i]));
        gtk_widget_set_sensitive (comboTcType[i], FALSE);
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkChan[0]), TRUE);


    hboxNumSamples = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxNumSamples);

    numSamplesLabel = gtk_label_new("Samples To Display:    ");
    gtk_box_pack_start(GTK_BOX(hboxNumSamples), numSamplesLabel, 0, 0, 0);

    spinNumSamples = gtk_spin_button_new_with_range (10, 1000, 10);
    gtk_box_pack_start(GTK_BOX(hboxNumSamples), spinNumSamples, 0, 0, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinNumSamples), 50.);

    hboxRate = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxRate);

    // FILE indicator
    hboxFile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxMain), hboxFile);
    labelFile = gtk_label_new(csv_filename);

    rateLabel = gtk_label_new("Read every:");
    gtk_box_pack_start(GTK_BOX(hboxRate), rateLabel, 0, 0, 0);

    spinRate = gtk_spin_button_new_with_range (1, 1000, 1);
    gtk_box_pack_start(GTK_BOX(hboxRate), spinRate, 0, 0, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinRate), 1.);

    comboReadIntervalUnits = gtk_combo_box_text_new ();
    gtk_box_pack_start(GTK_BOX(hboxRate), comboReadIntervalUnits, 0, 0, 0);
    for (i = 0; i < 4; i++)
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT(comboReadIntervalUnits), 0, rateUnits[i]);

    gtk_combo_box_set_active (GTK_COMBO_BOX(comboReadIntervalUnits), 0);
    g_signal_connect(GTK_COMBO_BOX(comboReadIntervalUnits), "changed", G_CALLBACK(rate_units_changed), NULL);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER(hboxMain), separator);
    g_signal_connect(window, "delete_event", G_CALLBACK (gtk_main_quit), NULL);

    gtk_databox_create_box_with_scrollbars_and_rulers_positioned (&box, &table,
        FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
    gtk_box_pack_start(GTK_BOX(hboxMain), table, TRUE, TRUE, 0);

    GtkDataboxRuler* rulerY = gtk_databox_get_ruler_y(GTK_DATABOX(box));
    gtk_databox_ruler_set_text_orientation(rulerY, GTK_ORIENTATION_HORIZONTAL);

    GtkDataboxRuler* rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(box));
    gchar* formatX = "%%6.0lf";
    gtk_databox_ruler_set_linear_label_format(rulerX, formatX);
    gtk_databox_ruler_set_draw_subticks(rulerX, FALSE);

    gtk_databox_ruler_set_range(rulerY, 10.0, -10.0, 0.0);
    gtk_databox_ruler_set_range(rulerX, 0.0, 50.0, 0.0);

    GdkRGBA grid_color;
    grid_color.red = 0;
    grid_color.green = 0;
    grid_color.blue = 0;
    grid_color.alpha = 0.3;
    grid_x = (GtkDataboxGraph*)gtk_databox_grid_new(7, 9, &grid_color, 1);
    gtk_databox_graph_add(GTK_DATABOX (box), GTK_DATABOX_GRAPH(grid_x));

    btnSelectLogFile = gtk_button_new_with_label ( "Select Log File ...");
    g_signal_connect(btnSelectLogFile, "clicked",
        G_CALLBACK(select_log_file_event_handler), csv_filename);
    gtk_box_pack_start(GTK_BOX(vboxConfig), btnSelectLogFile, 0, 0, 5);

    btnStart_Stop = gtk_button_new_with_label("Start");
    g_signal_connect(btnStart_Stop, "clicked",
        G_CALLBACK(start_stop_event_handler), NULL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), btnStart_Stop, 0, 0, 5);

    btnQuit = gtk_button_new_with_label( "Quit");
    g_signal_connect(btnQuit, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), btnQuit, TRUE, FALSE, 0);

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

    // Get the number of MCC 134 devices so that we can determine
    // how much memory to allocate for the hat_info_list
    hat_count = hat_list(HAT_ID_MCC_134, NULL);

    if (hat_count > 0)
    {
        // Allocate memory for the hat_info_list
        hat_info_list = (struct HatInfo*)malloc(
            hat_count * sizeof(struct HatInfo));

        // Get the list of MCC 134s.
        hat_list(HAT_ID_MCC_134, hat_info_list);

        // This application will use the first device (i.e. hat_info_list[0])
        *hat_address = hat_info_list[0].address;

        // Open the hat device
        retval = mcc134_open(*hat_address);
        if (retval != RESULT_SUCCESS)
        {
            show_mcc134_error(retval);
        }
    }
    else
    {
        retval = NO_HAT_DEVICES_FOUND;
        show_mcc134_error(retval);
    }

    if (hat_info_list != NULL)
        free(hat_info_list);

    // Return the address of the first HAT device.
    return retval;
}
