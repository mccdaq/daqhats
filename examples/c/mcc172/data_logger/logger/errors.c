#include <pthread.h>
#include "logger.h"
#include "errors.h"

// Global Variables
pthread_mutex_t disp_error_mutex;
pthread_cond_t disp_error_cond;

// Create a dialog box and display the error message.
gboolean show_error(int* error_code_ptr)
{
    const char *error_msg;
    int error_code = *error_code_ptr;

    if (error_code > -100)
    {
        // DaqHat library error messages
        error_msg = hat_error_message(error_code);
    }
    else
    {
        // Logger application errors
        switch (error_code)
        {
            case NO_HAT_DEVICES_FOUND:
                error_msg = "No MCC Hat devices were found.";
                break;
            case HW_OVERRUN:
                error_msg = "Hardware overrun has occurred.";
                break;
            case BUFFER_OVERRUN:
                error_msg = "Buffer overrun has occurred.";
                break;
            case UNABLE_TO_OPEN_FILE:
                error_msg = "Unable to open the log file.";
                break;
            case MAXIMUM_FILE_SIZE_EXCEEDED:
                error_msg = "The maximum file size has been exceeded.";
                break;
            case THREAD_ERROR:
                error_msg = "Error creating worker thread.";
                break;
            case OPEN_TC_ERROR:
                error_msg = "Open thermocouple detected.";
                break;
            case OVERRANGE_TC_ERROR:
                error_msg = "Thermocouple voltage outside the valid range.";
                break;
            case COMMON_MODE_TC_ERROR:
                error_msg = "Thermocouple voltage outside the common-mode range.";
                break;
            case UNKNOWN_ERROR:
            default:
                // unknown error ... most likely an unhandled system error
                error_msg = "Unknown error.";
                break;
        }
    }

    pthread_mutex_lock(&disp_error_mutex);

    // Create a dialog box.
    GtkWidget* error_dialog = gtk_message_dialog_new ((GtkWindow*)window,
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, error_msg);

    // Set the window title
    gtk_window_set_title ((GtkWindow*)error_dialog, "Error");

    // Show the dialog box.
    gtk_dialog_run (GTK_DIALOG (error_dialog));

    // We are done with the dialog, so destroy it.
    gtk_widget_destroy (error_dialog);

    pthread_cond_signal(&disp_error_cond);
    pthread_mutex_unlock(&disp_error_mutex);

    return FALSE;
}


// This function is called from a background thread to
// show an error message in the main thread.
void show_error_in_main_thread(int error_code)
{
    // Sychronize the threads so using a signal
    pthread_mutex_lock(&disp_error_mutex);
    g_main_context_invoke(context, (GSourceFunc)show_error, &error_code);
    pthread_cond_wait(&disp_error_cond, &disp_error_mutex);
    pthread_mutex_unlock(&disp_error_mutex);
}
