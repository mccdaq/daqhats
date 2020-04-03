#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <gtk/gtk.h>
#include <glib.h>

#include "daqhats/daqhats.h"

#define MAX_172_CHANNELS 2
#define READ_ALL_AVAILABLE  -1

// Global Variables
GtkWidget *window;
GMainContext *context;
uint8_t g_chan_mask;

#endif // LOGGER_H_INCLUDED
