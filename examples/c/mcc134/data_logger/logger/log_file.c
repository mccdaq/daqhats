#include <string.h>
#include <glib.h>
#include "log_file.h"
#include "errors.h"

// Global variables
FILE* log_file_ptr;
char csv_filename[512];

// Function Prototypes
static void get_path_and_filename(char* full_path, char* path, char* filename);
static void check_log_file_error (int status);


// Get the error message for the specified error code.
static void get_path_and_filename(char* full_path, char* path, char* filename)
{
    char* p;
    int path_len = 0;

    // Get the pointer to the last occurance of '/'.  This is the
    // pointer to the end of the path part of the full path.
    p = strrchr(full_path, '/');

    // get the length of the path
    path_len = p - full_path + 1;

    // copy the path part of the full path
    strncpy(path, full_path, path_len);

    // copy the file name part of the full path
    strcpy(filename, full_path+path_len);

    return;
}

// Show the OpenFile dialog to allow the name of the log file to be chosen.
char* choose_log_file(GtkWidget *parent_window, char* default_path)
{
    struct stat st;
    char path[512] = {'\0'};
    char filename[256] = {'\0'};
    char* new_filename;
    GtkWidget *dialog;
    gint res;

    // Get the path and filename.
    get_path_and_filename(default_path, path, filename);

    // Create the path if it does not already exist.
    if (stat(path, &st) == -1)
    {
        mkdir(path, 0700);
    }

    // Create the OpenFile dialog.
    dialog = gtk_file_chooser_dialog_new ("Select Log File",
                                          (GtkWindow*)parent_window,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          ("_Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          ("_OK"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    // Set the initial directory
    gtk_file_chooser_set_current_folder ((GtkFileChooser*)dialog, path);

    // Set the initial file name.
    gtk_file_chooser_set_current_name((GtkFileChooser*)dialog, filename);

    // Show the dialog.
    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        // get the selected file name path
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
        new_filename = gtk_file_chooser_get_filename (chooser);
    }
    else
    {
        // Use the initial path if the selection has been canceled
        new_filename = default_path;
    }

    // Destroy the dialog.
    gtk_widget_destroy (dialog);

    // Return the path to the selected file.
    return new_filename;
}


// Open the specified file for writing.
FILE* open_log_file (char* path)
{
    struct stat st;
    char directory[512] = {'\0'};
    char filename[256] = {'\0'};;

    // Get the path and file name.
    get_path_and_filename(path, directory, filename);

    // Create the path if it does not already exist.
    if (stat(directory, &st) == -1)
    {
        mkdir(directory, 0700);
    }

    // Open the log file for writing.
    log_file_ptr = fopen(path, "w");

    // Return the file pointer.
    return log_file_ptr;
}

int init_log_file(FILE* log_file_ptr, uint8_t chanMask, int max_channels)
{
    int i = 0;
    int channel = 0;
    int write_status = 1;
    int num_channels = 0;

    channel = 0;

	if (write_status <= 0)
	{
		// exit if an error occurred.
		return write_status;
	}

    for (i = 0; i < max_channels; i++)
    {
        // If this channel is in the scan,
        // print the channel number
        if (chanMask & 1)
        {
            //print channel
            write_status = fprintf(log_file_ptr, "Chan %d, ", i);
            if (write_status <= 0)
            {
                // Break if an error occurred.
                break;
            }
            num_channels++;
        }

        // Check next channel.
        channel++;
        chanMask >>= 1;
    }
    write_status = fprintf(log_file_ptr, "\n");

    // Check error status and display error message
    check_log_file_error(write_status);

    return write_status;
}


// Convert the numeric data to ASCII values, seperated by commas (CSV), and
// write the data using the specified file pointer.
int write_log_file(FILE* log_file_ptr, double* read_buf, int samplesPerChannel,
    int numberOfChannels)
{
    int i = 0;
    int j = 0;
    int write_status = 0;

    char str[1000];
    char buf[256];
    int scan_start_index = 0;

    strcpy(str, "");

    // Write the data to the file.
    for (i = 0; i < samplesPerChannel; i++)
    {
        // Initialize the string to be written to the file.
        strcpy(str, "");

        // Write a sample for each channel in the scan.
        for (j = 0; j < numberOfChannels; j++)
        {
            // Convert the data sample to ASCII
            sprintf(buf,"%2.6lf,", read_buf[scan_start_index+j]);

            // Add the data sample to the string to be written.
            strcat(str, buf);
        }

        // Write the ASCII scan data to the file.
        write_status = fprintf(log_file_ptr, "%s\n", str);
        if (write_status <= 0)
        {
            // Break if an error occurred.
            break;
        }

        // Get the index to the start of the next scan.
        scan_start_index += numberOfChannels;
    }

    // Flush the file to ensure all data is written.
    fflush(log_file_ptr);

    // Check error status and display error message
    check_log_file_error(write_status);

    // Return the error code.
    return write_status;
}

static void check_log_file_error (int status)
{
    if(status == -1)
    {
        show_error_in_main_thread(MAXIMUM_FILE_SIZE_EXCEEDED);
    }
    else if (status < 0)
    {
        show_error_in_main_thread(UNKNOWN_ERROR);
    }
    return;
}
