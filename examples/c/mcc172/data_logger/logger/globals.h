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

#include "daqhats/daqhats.h"

#define MAX_118_CHANNELS 8


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

EXTERN GtkWidget *rbContinuous, *rbFinite;
EXTERN GtkWidget *spinRate;
EXTERN GtkWidget *spinNumSamples;
EXTERN GtkWidget *btnSelectLogFile;
EXTERN GtkWidget *btnQuit;
EXTERN GtkWidget *chkChan[MAX_118_CHANNELS];
EXTERN GtkWidget *btnStart_Stop;

EXTERN GraphChannelInfo graphChannelInfo[MAX_118_CHANNELS];
EXTERN GdkRGBA legendColor[MAX_118_CHANNELS];

EXTERN uint8_t address;
EXTERN uint8_t channel_mask;

EXTERN double scan_timeout;
EXTERN gboolean done;

EXTERN int iNumSamplesPerChannel;
EXTERN double iRatePerChannel;

EXTERN pthread_t threadh;
EXTERN struct thread_info *tinfo;

EXTERN char application_name[512];
EXTERN char csv_filename[512];

EXTERN GMutex data_mutex;
EXTERN GMainContext *context;

EXTERN char error_message[256];
EXTERN GMainContext *error_context;

#endif // GLOBALS_H_INCLUDED
