#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "gtkdatabox_lines.h"
#include "gtkdatabox_ruler.h"
#include "gtkdatabox_grid.h"

#include "globals.h"
#include "log_file.h"
#include "errors.h"

GtkWidget *labelFile;

// function declarations
void initialize_graph_channel_info (void);
void show_file_name();
int open_first_hat_device(uint8_t* hat_address);

void allocate_channel_xy_arrays(int current_channel_mask,
    int numSamplesPerChannel);
int create_selected_channel_mask();
void set_enable_state_for_controls(gboolean state);

void refresh_graph(GtkWidget *box);
void copy_xy_arrays(int channel, int num_samples_to_copy);

void activate_event_handler(GtkApplication *app, gpointer user_data);
void select_log_file_event_handler(GtkWidget* widget, gpointer user_data);
void start_stop_event_handler(GtkWidget *widget, gpointer data);

