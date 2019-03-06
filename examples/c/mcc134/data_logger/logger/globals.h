#ifndef GLOBALS_H_INCLUDED
#define GLOBALS_H_INCLUDED


// GLOBALS should be defined in the file globals.c so that the global
// variables are declared in that file.  All other .c files should not
// define GLOBALS so that the varaibles are handled as extern.
#ifdef GLOBALS
#define EXTERN
#else
#define EXTERN extern
#endif


#include <gtk/gtk.h>
#include "gtkdatabox.h"
#include <glib.h>

#include <daqhats/daqhats.h>

#define MAX_134_TEMP_CHANNELS 4
#define MAX_134_CJC_CHANNELS 4
////////#define MAX_134_CHANNELS MAX_134_TEMP_CHANNELS + MAX_134_CJC_CHANNELS


typedef struct graph_channel_info
{
    GtkDataboxGraph*    graph;
    GdkRGBA*            color;
    uint                channelNumber;
    uint                readBufStartIndex;
    gfloat*             X;
    gfloat*             Y;
} GraphChannelInfo;

EXTERN GtkWidget *window;

EXTERN GtkWidget *box;
EXTERN GtkWidget *table;

EXTERN GtkWidget *spinRate;
EXTERN GtkWidget *spinNumSamples;
EXTERN GtkWidget *btnSelectLogFile;
EXTERN GtkWidget *btnQuit;
EXTERN GtkWidget *btnStart_Stop;

EXTERN GtkWidget *chkChan[MAX_134_TEMP_CHANNELS];
EXTERN GtkWidget *comboTcType[MAX_134_CJC_CHANNELS];
EXTERN GtkWidget *comboReadIntervalUnits;

EXTERN GraphChannelInfo graphChannelInfo[MAX_134_TEMP_CHANNELS];
EXTERN GdkRGBA legendColor[MAX_134_TEMP_CHANNELS];

EXTERN uint8_t address;
EXTERN uint8_t channel_mask;

EXTERN gboolean done;

EXTERN int numSamplesToDisplay;
EXTERN double ratePerChannel;
EXTERN int rateUnits;

EXTERN char application_name[512];
EXTERN char csv_filename[512];

EXTERN GMainContext *context;

EXTERN char error_message[256];
EXTERN GMainContext *error_context;

EXTERN double data_buffer[MAX_134_TEMP_CHANNELS];
EXTERN int selected_tc_type[MAX_134_TEMP_CHANNELS];
EXTERN int selected_rate_units;

EXTERN GtkDataboxGraph* grid_x;

EXTERN int total_samples_read;

#endif // GLOBALS_H_INCLUDED
