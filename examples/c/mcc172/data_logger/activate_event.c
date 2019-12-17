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
////////
////////    hboxNumSamples = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
////////    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxNumSamples);
////////
////////    label = gtk_label_new("Num Samples: ");
////////    gtk_box_pack_start(GTK_BOX(hboxNumSamples), label, 0, 0, 0);

    spinNumSamples = gtk_spin_button_new_with_range (10, 100000, 10);
    gtk_box_pack_start(GTK_BOX(hboxNumSamples), spinNumSamples, 0, 0, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinNumSamples), 500.);
////////
////////    hboxRate = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
////////    gtk_container_add(GTK_CONTAINER(vboxConfig), hboxRate);
////////
////////    label = gtk_label_new("                 Rate:  ");
////////    gtk_box_pack_start(GTK_BOX(hboxRate), label, 0, 0, 0);

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

    grid_color;
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
