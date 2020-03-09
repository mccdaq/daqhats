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
#include <gtkdatabox.h>
#include <gtkdatabox_util.h>
#include <glib.h>

#include "daqhats/daqhats.h"
#include "kiss_fft/kiss_fftr.h"

#define MAX_172_CHANNELS 2


typedef struct graph_channel_info
{
    GtkDataboxGraph*    graph;
    GtkDataboxGraph*    fft_graph;
    GdkRGBA*            color;
    uint                channelNumber;
    uint                readBufStartIndex;
    gfloat*             X;
    gfloat*             Y;
    gfloat*             fft_X;
    gfloat*             fft_Y;
} GraphChannelInfo;

EXTERN GtkWidget *window;

EXTERN GtkWidget *dataBox;
EXTERN GtkWidget *fftBox;
EXTERN GtkWidget *dataTable;
EXTERN GtkWidget *fftTable;

EXTERN GtkWidget *rbContinuous, *rbFinite;
EXTERN GtkWidget *spinRate;
EXTERN GtkWidget *comboBoxFftSize;
EXTERN GtkWidget *btnSelectLogFile;
EXTERN GtkWidget *btnQuit;
EXTERN GtkWidget *chkChan[MAX_172_CHANNELS];
EXTERN GtkWidget *chkIepe[MAX_172_CHANNELS];
EXTERN GtkWidget *btnStart_Stop;

EXTERN GraphChannelInfo graphChannelInfo[MAX_172_CHANNELS];
EXTERN GdkRGBA legendColor[MAX_172_CHANNELS];

EXTERN uint8_t address;
EXTERN uint8_t channel_mask;

EXTERN gboolean done;
EXTERN gboolean continuous;

EXTERN int iFftSize;
EXTERN double iRatePerChannel;

EXTERN pthread_t threadh;
EXTERN struct thread_info *tinfo;

EXTERN char application_name[512];
EXTERN char csv_filename[512];

EXTERN GMutex data_mutex;
EXTERN GMainContext *context;

EXTERN pthread_mutex_t graph_init_mutex;
EXTERN pthread_cond_t graph_init_cond;

EXTERN int error_code;
EXTERN char error_message[256];

EXTERN char dbg_string[1000];
EXTERN char dbg_string0[1000];
EXTERN char dbg_string1[1000];

#endif // GLOBALS_H_INCLUDED
