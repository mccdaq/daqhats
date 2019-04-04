#include "globals.h"
#include "errors.h"


// Get the error message for the specified error code.
const char* get_mcc118_error_message(int error_code)
{
    const char* msg;

    if (error_code > -100)
    {
        // DaqHat library error messages
        msg = hat_error_message(error_code);
    }
    else
    {
        // Logger application errors
        switch (error_code)
        {
            case NO_HAT_DEVICES_FOUND:
                msg = (const char*)"No MCC 118 Hat devices were found.";
                break;

            case HW_OVERRUN:
                msg = (const char*)"Hardware overrun has occurred.";
                break;

            case BUFFER_OVERRUN:
                msg = (const char*)"Buffer overrun has occurred.";
                break;

            case UNABLE_TO_OPEN_FILE:
                msg = (const char*)"Unable to open the log file.";
                break;

            case MAXIMUM_FILE_SIZE_EXCEEDED:
                msg = (const char*)"The maximum file size of 2GB has been "
                    "exceeded.";
                break;

            // unknown error ... most likely an unhandled system error
            case UNKNOWN_ERROR:
            default:
                msg = (const char*)"Unknown error.";
                break;
        }
    }

   return msg;
}


// Create a dialog box and display the error message.
gboolean show_error(const char* errmsg)
{
    // Create a dialog box.
    GtkWidget* error_dialog = gtk_message_dialog_new ((GtkWindow*)window,
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, errmsg);

    // Set the window title
    gtk_window_set_title ((GtkWindow*)error_dialog, "Error");

    // Show the dialog box.
    gtk_dialog_run (GTK_DIALOG (error_dialog));

    // We are done with the dialog, so destroy it.
    gtk_widget_destroy (error_dialog);

    return FALSE;
}


// Get the error message for the specified error code, and display it in a
// dialog box.
//
// This function is called from a background thread using the function
// g_main_context_invoke(), which causes the function to execute in the main
// thread.
void show_mcc118_error_main_thread(gpointer error_code_ptr)
{
    int error_code = *(int*)error_code_ptr;
    show_mcc118_error(error_code);
}


// Get the error message for the specified error code, and display it in a
// dialog box.
void show_mcc118_error(int error_code)
{
    // Get the error message.
    const char* errmsg = get_mcc118_error_message(error_code);

 	// display the rror message in a a dialog box.
   show_error(errmsg);
}

