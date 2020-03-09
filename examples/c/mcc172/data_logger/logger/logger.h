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
#include "fft.h"

GtkWidget *labelFile;

// function declarations
void show_file_name();
int open_first_hat_device(uint8_t* hat_address);

int allocate_channel_xy_arrays(uint8_t current_channel_mask,
    uint32_t fft_size);
int create_selected_channel_mask();
void set_enable_state_for_controls(gboolean state);

gboolean refresh_graph(int* start_sample);
void copy_data_to_xy_arrays(double* hat_read_buf, int read_buf_start_index,
    int channel, int stride, int buffer_size_samples, uint32_t start_sample);

static void * read_and_display_data ();

void activate_event_handler(GtkApplication *app, gpointer user_data);
void select_log_file_event_handler(GtkWidget* widget, gpointer user_data);
void start_stop_event_handler(GtkWidget *widget, gpointer data);
gboolean stop_acquisition(void);

