#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtkdatabox.h>
#include <gtkdatabox_util.h>
#include <gtkdatabox_lines.h>

#include "log_file.h"
#include "errors.h"
#include "logger.h"

#define MAX_CHANNELS 4  // MCC134 Channel Count

// Global Variables
GtkWidget *window;
GMainContext *context;
uint8_t g_hat_addr = 0;
uint8_t g_chan_mask;
int g_num_samples = 50;
double g_zoom_level = 1.0;
double g_sample_rate = 1.0;
uint32_t g_sample_count = 0;
gboolean g_done = TRUE;
gboolean g_continuous = TRUE;
const char *colors[8] = {"#DD3222", "#3482CB", "#75B54A", "#9966ff",
                         "#FFC000", "#FF6A00", "#808080", "#6E1911"};
GtkWidget *labelFile, *dataBox, *rbContinuous, *rbFinite, *spinRate,
          *spinNumSamples, *btnSelectLogFile, *chkChan[MAX_CHANNELS],
          *btnStart_Stop, *comboRateUnits, *comboTcType[MAX_CHANNELS];
GMutex data_mutex;
pthread_t threadh;
guint g_timeout;
pthread_mutex_t allocate_arrays_mutex;
pthread_cond_t allocate_arrays_cond;
pthread_mutex_t timer_mutex;
pthread_cond_t timer_cond;
typedef struct graph_channel_info
{
    GtkDataboxGraph*    graph;
    GdkRGBA*            color;
    uint                channelNumber;
    gfloat*             X;
    gfloat*             Y;
    gint                buffSize;
} GraphChannelInfo;
GraphChannelInfo graphChannelInfo[MAX_CHANNELS];

// Function Prototypes
static int open_first_hat_device(uint8_t* hat_address);
static void app_activate_handler(GtkApplication *app, gpointer user_data);
static gboolean stop_scan(void);


int main(void)
{
    int retval = 0;
    int i = 0;
    GtkApplication *app;
    GdkRGBA legendColor[MAX_CHANNELS];

    // Set the default filename.
    getcwd(csv_filename, sizeof(csv_filename));
    strcat(csv_filename, "/LogFiles/data.csv");

    // Initialize channel info array
    for (i=0; i< MAX_CHANNELS; i++)
    {
        gdk_rgba_parse (&legendColor[i], colors[i]);
        graphChannelInfo[i].color = &legendColor[i];
        graphChannelInfo[i].channelNumber = i;
    }

    // Create the application structure and set the activate event handler.
    app = gtk_application_new("mcc134.dataLogger", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(app_activate_handler), NULL);

    // Start running the GTK appliction.
    g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    // Find the hat devices and open the first one.
    retval = open_first_hat_device(&g_hat_addr);

    if (retval == 0)
    {
        context = g_main_context_default();

        // Start the GTK message loop.
        gtk_main();

        // Close the device.
        mcc134_close(g_hat_addr);
    }

    return 0;
}

// Allocate arrays for the indices and data for each channel in the scan.
static gboolean allocate_channel_xy_arrays(int *channel)
{
    int chan = *channel;
    int buff_size = 0;

    pthread_mutex_lock(&allocate_arrays_mutex);

    buff_size = g_sample_count < g_num_samples? g_sample_count : g_num_samples;

    // Delete the previous array for each the channel (if it exists)
    if (graphChannelInfo[chan].graph != NULL)
    {
        gtk_databox_graph_remove (GTK_DATABOX(dataBox),
            GTK_DATABOX_GRAPH(graphChannelInfo[chan].graph));
        graphChannelInfo[chan].graph= NULL;
    }

    // Free any existing data arrays
    if(graphChannelInfo[chan].X != NULL)
    {
        g_free(graphChannelInfo[chan].X);
        graphChannelInfo[chan].X = NULL;
    }
    if(graphChannelInfo[chan].Y != NULL)
    {
        g_free(graphChannelInfo[chan].Y);
        graphChannelInfo[chan].Y = NULL;
    }

    // Allocate arrays for graph data and initialize to zero
    graphChannelInfo[chan].Y = g_new0(gfloat, buff_size);
    graphChannelInfo[chan].X = g_new0(gfloat, buff_size);
    graphChannelInfo[chan].buffSize = buff_size;

    if(buff_size > 0)
    {
        graphChannelInfo[chan].graph = gtk_databox_lines_new
            ((guint)buff_size, graphChannelInfo[chan].X,
            graphChannelInfo[chan].Y,
            graphChannelInfo[chan].color, 1);
        gtk_databox_graph_add(GTK_DATABOX (dataBox),
            GTK_DATABOX_GRAPH(graphChannelInfo[chan].graph));
    }

    pthread_cond_signal(&allocate_arrays_cond);
    pthread_mutex_unlock(&allocate_arrays_mutex);

    return FALSE;
}

// Add each checked channel to the channel mask
static int create_selected_channel_mask()
{
    gboolean checked = FALSE;
    int selected_channel_mask = 0;
    int i = 0;

    for (i = 0; i < MAX_CHANNELS; i++)
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

// Enable/disable the controls in the main window.
// Controls are disabled when the acquisition is running
// and re-enabled when the acquisition is stopped.
static void set_enable_state_for_controls(gboolean state)
{
    int i = 0;

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        gtk_widget_set_sensitive (chkChan[i], state);
        gtk_widget_set_sensitive(comboTcType[i], state);
    }
    gtk_widget_set_sensitive (spinRate, state);
    gtk_widget_set_sensitive (comboRateUnits, state);
    gtk_widget_set_sensitive (spinNumSamples, state);
    gtk_widget_set_sensitive (btnSelectLogFile, state);
}

// Copy data from the hat read buffer to the display buffer
static void copy_hat_data_to_display_buffer(double* hat_read_buf,
                                            int samples_per_chan_read,
                                            double* display_buf, int num_chans)
{
    size_t copy_size = 0;
    int samples_to_keep = 0;
    int start_idx = 0;
    int samples_per_chan_displayed = g_sample_count >= g_num_samples ?
                                     g_num_samples : g_sample_count;

    if (samples_per_chan_read > 0) {
        // There are samples to be copied
        if((samples_per_chan_displayed + samples_per_chan_read)
           <= g_num_samples)
        {
            // The display buffer is not full yet
            copy_size = samples_per_chan_read * num_chans * sizeof(double);
            memcpy(&display_buf[samples_per_chan_displayed * num_chans],
                   hat_read_buf, copy_size);
            g_sample_count += samples_per_chan_read;
        }
        else {
            // The display buffer is full and the values must first be shifted.
            samples_to_keep = g_num_samples - samples_per_chan_read;
            copy_size = samples_to_keep * num_chans * sizeof(double);
            start_idx = ((samples_per_chan_displayed - samples_to_keep)
                         * num_chans);
            memcpy(display_buf, &display_buf[start_idx], copy_size);
            samples_per_chan_displayed = samples_to_keep;

            copy_size = samples_per_chan_read * num_chans * sizeof(double);
            memcpy(&display_buf[samples_per_chan_displayed * num_chans],
                   hat_read_buf, copy_size);
            g_sample_count += samples_per_chan_read;
        }
    }

    return;
}

// Copy the data for the specified channel from the interleaved
// HAT buffer to the array for the specified channel.
static void copy_data_to_xy_arrays(double* display_buf,
                                   int read_buf_start_index, int channel,
                                   int stride, uint32_t start_sample)
{
    uint32_t sample = start_sample;
    uint32_t data_array_idx = 0;
    int i = read_buf_start_index;

    if(graphChannelInfo[channel].buffSize < g_num_samples)
    {
        pthread_mutex_lock(&allocate_arrays_mutex);
        g_main_context_invoke(context, (GSourceFunc)allocate_channel_xy_arrays,
                              &channel);
        pthread_cond_wait(&allocate_arrays_cond, &allocate_arrays_mutex);
        pthread_mutex_unlock(&allocate_arrays_mutex);
    }

    // Set indices and data
    for (sample = start_sample; sample < g_sample_count; sample++)
    {
        graphChannelInfo[channel].X[data_array_idx] = (gfloat)sample;
        graphChannelInfo[channel].Y[data_array_idx] = (gfloat)display_buf[i];
        data_array_idx++;
        i+=stride;
    }
}

// Refresh the graph with the new data.
static gboolean refresh_graph()
{
    int start_sample = 0;
    int i = 0;
    uint8_t chanMask = g_chan_mask;
    int channel = 0;
    gboolean first_chan = TRUE;
    gdouble yMin = 0.0;
    gdouble yMax = 100.0;
    gdouble yMid = 50.0;
    gdouble yRange = 100.0;
    gfloat start, end;

    g_mutex_lock (&data_mutex);

    start_sample = g_sample_count >= g_num_samples ?
            (g_sample_count - g_num_samples) : 0;

    // Set the new limits on the time domain graph
    start = (gfloat)start_sample;
    end = (gfloat)(start_sample + g_num_samples - 1);

    // Auto-scale Y-Axis
    if(g_sample_count > 0)
    {
        while (chanMask > 0)
        {
            if (chanMask & 1)
            {
                if(first_chan)
                {
                    yMin = graphChannelInfo[channel].Y[0];
                    yMax = graphChannelInfo[channel].Y[0];
                    first_chan = FALSE;
                }
                for(i=0; i<graphChannelInfo[channel].buffSize; i++)
                {
                    if(graphChannelInfo[channel].Y[i] < yMin)
                    {
                        yMin = graphChannelInfo[channel].Y[i];
                    }
                    if(graphChannelInfo[channel].Y[i] > yMax)
                    {
                        yMax = graphChannelInfo[channel].Y[i];
                    }
                }
            }
            channel++;
            chanMask >>= 1;
        }
        if(yMin == yMax)
        {
            yRange = 0.1;
        }
        else
        {
            yRange = (yMax - yMin) / 0.8;
        }
        yMid = (yMax + yMin) / 2.0;
        yRange *= g_zoom_level;
        yMin = yMid - (yRange / 2.0);
        yMax = yMid + (yRange / 2.0);

        gtk_databox_set_total_limits(GTK_DATABOX (dataBox), start, end, yMax, yMin);

        // Re-draw the graphs
        gtk_widget_queue_draw(dataBox);
    }

    // release the mutex
    g_mutex_unlock (&data_mutex);

    return FALSE;
}

// While the scan is running, read the data, write it to a
// CSV file, and plot it in the graph.
// This function runs as a background thread for the duration of the scan.
static void* read_and_display_data ()
{
    int chanMask = g_chan_mask;
    int channel = 0;
    int retval = 0;
    int start_sample = 0;
    int num_channels = 0;
    int fail_count = 0;
    int read_buf_index = 0;
    uint32_t display_buf_size_samples;
    double temp_val = 0.0;
    double hat_read_buf[MAX_CHANNELS] = {0};
    double *display_buf = NULL;

    // Reset the sample count
    g_sample_count = 0;

    // Get the channel count
    chanMask = g_chan_mask;
    while (chanMask > 0)
    {
        if (chanMask & 1)
        {
            num_channels++;
        }
        graphChannelInfo[channel].buffSize = 0;
        channel++;
        chanMask >>= 1;
    }

    // Write channel numbers to file header
    retval = init_log_file(log_file_ptr, g_chan_mask, MAX_CHANNELS);
    if (retval < 0)
    {
        g_done = TRUE;
        set_enable_state_for_controls(TRUE);
        return NULL;
    }

    display_buf_size_samples = g_num_samples * num_channels;
    display_buf = (double*)malloc(display_buf_size_samples * sizeof(double));
    memset(display_buf, 0, display_buf_size_samples * sizeof(double));

    // Loop to read data continuously
    while (g_done != TRUE)
    {
        chanMask = g_chan_mask;
        channel = 0;
        fail_count = 0;
        read_buf_index = 0;

        // While there are channels to plot.
        while (chanMask > 0)
        {
            // If this channel is included in the acquisition,
            // read a value and add it to the plot.
            if (chanMask & 1)
            {
                retval = mcc134_t_in_read(g_hat_addr, channel, &temp_val);
                if(retval == RESULT_SUCCESS)
                {
                    if(temp_val == OPEN_TC_VALUE)
                    {
                        retval = OPEN_TC_ERROR;
                    }
                    else if(temp_val == OVERRANGE_TC_VALUE)
                    {
                        retval = OVERRANGE_TC_ERROR;
                    }
                    else if(temp_val == COMMON_MODE_TC_VALUE)
                    {
                        retval = COMMON_MODE_TC_ERROR;
                    }
                }

                if(retval != RESULT_SUCCESS)
                {
                    show_error_in_main_thread(retval);
                    // Call the Start/Stop event handler to reset the UI
                    g_main_context_invoke(context, (GSourceFunc)stop_scan,
                                          NULL);
                    fail_count++;
                }

                hat_read_buf[read_buf_index++] = temp_val;
            }
            channel++;
            chanMask >>= 1;
        }

        if(fail_count == 0)
        {
            // Write the data to a log file as CSV data
            retval = write_log_file(log_file_ptr, hat_read_buf, 1,
                                    num_channels);
            if (retval < 0)
            {
                // Call the Start/Stop event handler to reset the UI
                g_main_context_invoke(context, (GSourceFunc)stop_scan, NULL);
            }

            copy_hat_data_to_display_buffer(hat_read_buf, 1, display_buf,
                                            num_channels);

            // Set a mutex to prevent the data from changing while we plot it
            g_mutex_lock (&data_mutex);

            chanMask = g_chan_mask;
            channel = 0;
            read_buf_index = 0;

            start_sample = g_sample_count >= g_num_samples ?
                            (g_sample_count - g_num_samples) : 0;
            // While there are channels to plot.
            while (chanMask > 0)
            {
                // If this channel is included in the acquisition,
                // plot its data.
                if (chanMask & 1)
                {
                    copy_data_to_xy_arrays(display_buf, read_buf_index++,
                        channel, num_channels, start_sample);
                }
                channel++;
                chanMask >>= 1;
            }

            // Update the display.
            g_main_context_invoke(context, (GSourceFunc)refresh_graph, NULL);

            // Release the mutex
            g_mutex_unlock(&data_mutex);
        }

        // Wait for the timer signal
        pthread_mutex_lock(&timer_mutex);
        pthread_cond_wait(&timer_cond, &timer_mutex);
        pthread_mutex_unlock(&timer_mutex);
    }

    free(display_buf);
    return NULL;
}

// This is a timer function to signal to the worker thread to get a new input
// value at the specified time interval.
static gboolean read_timer (gpointer data)
{
    // Send the timer signal to the worker thread
    pthread_mutex_unlock(&timer_mutex);
    pthread_cond_signal(&timer_cond);
    pthread_mutex_unlock(&timer_mutex);
    if(g_done)
    {
        return FALSE;
    }
    return TRUE;
}

// Event handler for the Start/Stop button.
// If starting, change the button text to "Stop" and start the acquisition.
// If stopping, change the button text to "Start" and stop the acquisition.
static void start_stop_event_handler(GtkWidget *widget, gpointer data)
{
    int retval = 0;
    int unit_index = 0;
    int tc_type_index = 0;
    int i = 0;
    const gchar* StartStopBtnLbl;
    struct thread_info *tinfo;

    StartStopBtnLbl = gtk_button_get_label(GTK_BUTTON(widget));

    if (strcmp(StartStopBtnLbl , "Start") == 0)
    {
        set_enable_state_for_controls(FALSE);

        // Set variables based on the UI settings.
        g_chan_mask = create_selected_channel_mask();

        // Change the label on the start button to "Stop".
        gtk_button_set_label(GTK_BUTTON(widget), "Stop");

        g_done = FALSE;

        g_num_samples = gtk_spin_button_get_value(
                            GTK_SPIN_BUTTON(spinNumSamples));
        g_sample_rate = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spinRate));
        // Get the units for how often to read a channel.
        unit_index = gtk_combo_box_get_active (GTK_COMBO_BOX(comboRateUnits));

        // Calculate the number of seconds between reads of a channel.
        switch (unit_index)
        {
            // Seconds
            default:
            case 0:
                break;
            // Minutes
            case 1:
                g_sample_rate *= 60;
                break;
            // Hours
            case 2:
                g_sample_rate *= 60 * 60;
                break;
            // Days
            case 3:
                g_sample_rate *= 60 * 60 * 24;
                break;
        }

        // Set the TC type for each channel
        for(i=0; i<MAX_CHANNELS; i++)
        {
            tc_type_index = gtk_combo_box_get_active(
                                GTK_COMBO_BOX(comboTcType[i]));
            retval = mcc134_tc_type_write(g_hat_addr, i, tc_type_index);
            if(retval != RESULT_SUCCESS)
            {
                show_error(&retval);
                g_done = TRUE;
                set_enable_state_for_controls(TRUE);
                return;
            }
        }

        // Open the log file.
        log_file_ptr = open_log_file(csv_filename);
        if (log_file_ptr == NULL)
        {
            retval = UNABLE_TO_OPEN_FILE;
            show_error(&retval);
            g_done = TRUE;
            set_enable_state_for_controls(TRUE);
            return;
        }

        // Start a thread to read the data from the device
        retval = pthread_create(&threadh, NULL, &read_and_display_data,
                                &tinfo);
        if(retval != RESULT_SUCCESS)
        {
            retval = THREAD_ERROR;
            show_error(&retval);
            gtk_button_set_label(GTK_BUTTON(btnStart_Stop), "Start");
            set_enable_state_for_controls(TRUE);
            g_done = TRUE;
        }
        else
        {
            // Start the timer
            g_timeout = g_timeout_add_seconds((guint)g_sample_rate,
                                              read_timer, NULL);
        }
    }
    else
    {
        // Set the done flag, stop the timer and
        // wait for the worker thread to complete.
        g_done = TRUE;
        g_source_remove(g_timeout);
        pthread_mutex_unlock(&timer_mutex);
        pthread_cond_signal(&timer_cond);
        pthread_mutex_unlock(&timer_mutex);
        pthread_join(threadh, NULL);

        set_enable_state_for_controls(TRUE);
        // Change the label on the stop button to "Start"
        gtk_button_set_label(GTK_BUTTON(widget), "Start");
    }

    return;
}

// A function to stop the acquisisiont that can be invoked
// from the worker thread
static gboolean stop_scan()
{
    // Simulate a stop button press
    start_stop_event_handler(btnStart_Stop, NULL);

    return FALSE;
}

// Event handler for the Select Log File button.
// Displays a file select dialog to choose the log file to be opened.
// The file name will be shown in footer of the main window.
static void select_log_file_event_handler(GtkWidget* widget, char* user_data)
{
    // Select the log file.
    strcpy(csv_filename, choose_log_file(window, user_data));
    // Display the CSV log file name.
    gtk_label_set_text(GTK_LABEL(labelFile), csv_filename);
}

// Event handler for the time domain plot Y Zoom + button
static void zoom_in_handler(GtkWidget* widget, gpointer user_dat)
{
    g_zoom_level *= 0.8;
    refresh_graph();
}

// Event handler for the time domain plot Y Zoom - button
static void zoom_out_handler(GtkWidget* widget, gpointer user_dat)
{
    g_zoom_level /= 0.8;
    refresh_graph();
}

// Event handler that is called when the application is launched to create
// the main window and its controls.
static void app_activate_handler(GtkApplication *app, gpointer user_data)
{
    GtkCssProvider* cssProvider;
    GtkWidget *hboxMain, *vboxMain, *hboxFile, *vboxConfig, *vboxSampleRate,
              *vboxNumSamples, *vboxGraph, *label, *vboxTcType,
              *hboxChannel, *hboxRate1, *hboxRate2, *hboxNumSamples1,
              *hboxNumSamples2, *hboxLogFile, *hboxZoom, *vboxChannel,
              *vboxLegend, *acqSeparator, *logFileSeparator, *chanSeparator,
              *endSeparator, *dispSeparator, *dataTable, *btnZoomInY,
              *btnZoomOutY, *separator;
    GtkWidget *legend[MAX_CHANNELS];
    GtkDataboxRuler *rulerY, *rulerX;
    GdkRGBA background_color;
    PangoAttrList *titleAttrs;
    PangoAttribute *bold;
    GtkStyleContext *styleContext;
    int i = 0;
    int type_idx = 0;
    // TC Types must be in the same order as the daqhats library
    char* tcType[] = {"J", "K", "T", "E", "R", "S", "B", "N"};
    char* rateUnits[] = {"Sec", "Min", "Hour", "Day"};
    char chanName[20];
    char legendName[20];
    char chan_css[100] = "";
    char css_str[1024] = "#startStop.circular {border-color: #3B5998; "
                         "background-color: #3B5998;}\n";

    // Set CSS styling for the legend
    for(i=0; i<MAX_CHANNELS; i++)
    {
        sprintf(chan_css, "#Chan%d block.filled {background-color: %s; "
            "border-color: %s;}\n", i, colors[i], colors[i]);
        strcat(css_str, chan_css);
    }

    // Set CSS styling for channel legend
    cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER (cssProvider), css_str,
                                    -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                               GTK_STYLE_PROVIDER(cssProvider),
                               GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Create attribute list for section headings
    titleAttrs = pango_attr_list_new ();
    bold = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
    pango_attr_list_insert (titleAttrs, bold);

    // Create the top level gtk window.
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_set_size_request(window, 900, 700);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    // Create the GDK resources for the main window
    gtk_widget_realize(window);

    // Connect the event handler to the "delete_event" event
    g_signal_connect(window, "delete_event", G_CALLBACK (gtk_main_quit), NULL);

    vboxMain = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_container_add(GTK_CONTAINER(window), vboxMain);

    hboxMain = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(vboxMain), hboxMain);

    vboxConfig = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(hboxMain), vboxConfig);

    /******** Actions Section ********/
    // Start/Stop Button
    btnStart_Stop = gtk_button_new_with_label("Start");
    g_signal_connect(btnStart_Stop, "clicked",
        G_CALLBACK(start_stop_event_handler), NULL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), btnStart_Stop, FALSE, FALSE, 0);
    gtk_widget_set_name(GTK_WIDGET(btnStart_Stop), "startStop");
    styleContext = gtk_widget_get_style_context(GTK_WIDGET(btnStart_Stop));
    gtk_style_context_add_class(styleContext, "circular");

    /******** Display Settings ********/
    dispSeparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), dispSeparator, FALSE, FALSE, 0);
    label = gtk_label_new("Display Settings");
    gtk_label_set_attributes (GTK_LABEL(label), titleAttrs);
    gtk_box_pack_start(GTK_BOX(vboxConfig), label, FALSE, FALSE, 0);
    // Time Domain Zoom Buttons
    hboxZoom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxZoom);

    label = gtk_label_new("Zoom Y:");
    gtk_box_pack_start(GTK_BOX(hboxZoom), label, FALSE, FALSE, 0);

    btnZoomOutY = gtk_button_new_with_label ( "-");
    gtk_box_pack_start(GTK_BOX(hboxZoom), btnZoomOutY, TRUE, FALSE, 3);
    styleContext = gtk_widget_get_style_context(GTK_WIDGET(btnZoomOutY));
    gtk_style_context_add_class(styleContext, "circular");
    g_signal_connect(btnZoomOutY, "clicked", G_CALLBACK(zoom_out_handler),
                     NULL);

    btnZoomInY = gtk_button_new_with_label ( "+");
    gtk_box_pack_start(GTK_BOX(hboxZoom), btnZoomInY, TRUE, FALSE, 0);
    styleContext = gtk_widget_get_style_context(GTK_WIDGET(btnZoomInY));
    gtk_style_context_add_class(styleContext, "circular");
    g_signal_connect(btnZoomInY, "clicked", G_CALLBACK(zoom_in_handler), NULL);

    /******** Channel Settings ********/
    chanSeparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), chanSeparator, FALSE, FALSE, 0);
    label = gtk_label_new("Channel Settings");
    gtk_label_set_attributes (GTK_LABEL(label), titleAttrs);
    gtk_box_pack_start(GTK_BOX(vboxConfig), label, FALSE, FALSE, 0);
    hboxChannel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxChannel);
    // Channel Select
    vboxChannel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxChannel);
    label = gtk_label_new("Chan Select:");
    gtk_box_pack_start(GTK_BOX(vboxChannel), label, FALSE, FALSE, 0);
    vboxTcType=gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxTcType);
    label = gtk_label_new("TC Type:");
    gtk_box_pack_start(GTK_BOX(vboxTcType), label, FALSE, FALSE, 0);
    vboxLegend=gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxChannel), vboxLegend);
    label = gtk_label_new("");  // Needed for spacing
    gtk_box_pack_start(GTK_BOX(vboxLegend), label, FALSE, FALSE, 0);
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        sprintf(chanName, "Channel %d", i);
        chkChan[i] = gtk_check_button_new_with_label(chanName);
        gtk_box_pack_start(GTK_BOX(vboxChannel), chkChan[i], TRUE, FALSE, 0);

        legend[i] = gtk_level_bar_new_for_interval(0.0, 100.0);
        gtk_level_bar_set_value(GTK_LEVEL_BAR(legend[i]), 100.0);
        gtk_box_pack_start(GTK_BOX(vboxLegend), legend[i], TRUE, FALSE, 0);
        sprintf(legendName, "Chan%d", i);
        gtk_widget_set_name(legend[i], legendName);
        comboTcType[i] = gtk_combo_box_text_new ();

        for (type_idx = 0; type_idx < 8; type_idx++)
        {
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboTcType[i]), 0,
                                      tcType[type_idx]);
        }
        gtk_box_pack_start(GTK_BOX(vboxTcType), comboTcType[i], FALSE, 0, 0);
        gtk_combo_box_set_active (GTK_COMBO_BOX(comboTcType[i]), 0);
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkChan[0]), TRUE);

    /******** Acquisition Settings ********/
    acqSeparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), acqSeparator, FALSE, FALSE, 0);
    label = gtk_label_new("Acquisition Settings");
    gtk_label_set_attributes (GTK_LABEL(label), titleAttrs);
    gtk_box_pack_start(GTK_BOX(vboxConfig), label, FALSE, FALSE, 0);
    // Sample Rate
    vboxSampleRate = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), vboxSampleRate);
    hboxRate1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vboxSampleRate), hboxRate1);
    label = gtk_label_new("Read Every:");
    gtk_box_pack_start(GTK_BOX(hboxRate1), label, FALSE, FALSE, 0);
    hboxRate2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vboxSampleRate), hboxRate2);
    spinRate = gtk_spin_button_new_with_range (1, 100000, 1);
    gtk_box_pack_start(GTK_BOX(hboxRate2), spinRate, FALSE, FALSE, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinRate), 1.);
    comboRateUnits = gtk_combo_box_text_new ();
    gtk_box_pack_start(GTK_BOX(hboxRate2), comboRateUnits, 0, 0, 0);
    for (i = 0; i < 4; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboRateUnits), 0,
                                  rateUnits[i]);

    gtk_combo_box_set_active(GTK_COMBO_BOX(comboRateUnits), 0);
    // Number of Samples
    vboxNumSamples = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), vboxNumSamples);
    hboxNumSamples1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vboxNumSamples), hboxNumSamples1);
    label = gtk_label_new("Samples To Display:");
    gtk_box_pack_start(GTK_BOX(hboxNumSamples1), label, FALSE, FALSE, 0);
    hboxNumSamples2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vboxNumSamples), hboxNumSamples2);
    spinNumSamples = gtk_spin_button_new_with_range (10, 1000, 1);
    gtk_box_pack_start(GTK_BOX(hboxNumSamples2), spinNumSamples, FALSE, FALSE,
                       0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinNumSamples), 50.);

    /******** Log File Settings ********/
    logFileSeparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), logFileSeparator, FALSE, FALSE, 0);
    label = gtk_label_new("Log File Settings");
    gtk_label_set_attributes (GTK_LABEL(label), titleAttrs);
    gtk_box_pack_start(GTK_BOX(vboxConfig), label, FALSE, FALSE, 0);
    hboxLogFile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxLogFile);
    btnSelectLogFile = gtk_button_new_with_label ( "Select Log File ...");
    g_signal_connect(btnSelectLogFile, "clicked",
        G_CALLBACK(select_log_file_event_handler), csv_filename);
    gtk_box_pack_start(GTK_BOX(hboxLogFile), btnSelectLogFile, FALSE, FALSE, 0);
    styleContext = gtk_widget_get_style_context(GTK_WIDGET(btnSelectLogFile));
    gtk_style_context_add_class(styleContext, "circular");

    /******** Graphs ********/
    // Separators from the configuration controls
    endSeparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vboxConfig), endSeparator, FALSE, FALSE, 0);
    separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_container_add(GTK_CONTAINER(hboxMain), separator);

    // Add the time domain graph
    vboxGraph = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(hboxMain), vboxGraph);
    label = gtk_label_new("Temperature (°C)");
    gtk_label_set_attributes (GTK_LABEL(label), titleAttrs);
    gtk_box_pack_start(GTK_BOX(vboxGraph), label, FALSE, FALSE, 0);
    gtk_databox_create_box_with_scrollbars_and_rulers_positioned (&dataBox,
        &dataTable, FALSE, FALSE, TRUE, TRUE, FALSE, TRUE);
    gtk_box_pack_start(GTK_BOX(vboxGraph), dataTable, TRUE, TRUE, 10);
    rulerY = gtk_databox_get_ruler_y(GTK_DATABOX(dataBox));
    gtk_databox_ruler_set_text_orientation(rulerY, GTK_ORIENTATION_HORIZONTAL);
    gtk_databox_ruler_set_max_length(rulerY, 7);
    rulerX = gtk_databox_get_ruler_x(GTK_DATABOX(dataBox));
    gtk_databox_ruler_set_max_length(rulerX, 9);
    gtk_databox_ruler_set_linear_label_format(rulerX, "%%.0f");
    // Set the default limits
    gtk_databox_ruler_set_range(rulerY, 100.0, 0.0, 0.0);
    gtk_databox_ruler_set_range(rulerX, 0.0, 50.0, 0.0);
    gtk_databox_ruler_set_draw_subticks(rulerX, FALSE);

    // Set the background color for the graphs
    gdk_rgba_parse (&background_color, "#d9d9d9");
    pgtk_widget_override_background_color(dataBox, GTK_STATE_FLAG_NORMAL,
                                          &background_color);

    /******** Log file name display ********/
    hboxFile = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(vboxMain), hboxFile);
    labelFile = gtk_label_new(csv_filename);
    gtk_box_pack_start(GTK_BOX(hboxFile), labelFile, TRUE, FALSE, 0);

    // Show the top level window and all of its controls
    gtk_widget_show_all(window);
}

// Find all of the MCC134 HAT devices that are installed
// and open a connection to the first one.
static int open_first_hat_device(uint8_t* hat_address)
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

        // Get the list of MCC 134 devices
        hat_list(HAT_ID_MCC_134, hat_info_list);
        // Choose the first one
        *hat_address = hat_info_list[0].address;

        // Open the hat device
        retval = mcc134_open(*hat_address);
        if(retval != RESULT_SUCCESS)
        {
            show_error(&retval);
        }
    }
    else
    {
        retval = NO_HAT_DEVICES_FOUND;
        show_error(&retval);
    }

    if (hat_info_list != NULL)
        free(hat_info_list);

    // Return the address of the first HAT device.
    return retval;
}
