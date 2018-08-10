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

int allocate_channel_xy_arrays(uint8_t current_channel_mask,
    uint32_t numSamplesPerChannel);
int create_selected_channel_mask();
void set_enable_state_for_controls(gboolean state);

gboolean refresh_graph(GtkWidget *box);
void copy_data_to_xy_arrays(double* hat_read_buf, int read_buf_start_index,
    int channel, int stride, int buffer_size_samples, gboolean first_block);

void analog_in_finite ();
static void * analog_in_continuous ();

void activate_event_handler(GtkApplication *app, gpointer user_data);
void select_log_file_event_handler(GtkWidget* widget, gpointer user_data);
void start_stop_event_handler(GtkWidget *widget, gpointer data);

