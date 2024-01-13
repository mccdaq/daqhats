#include <gtk/gtk.h>
#include <stdlib.h>

/// \brief Callback to end the program
void end_program(GtkWidget* wid, gpointer ptr)
{
   gtk_main_quit();
}

/// \brief Displays a message dialog with the specified title and message
void show_message_dialog(const char* title, const char* message, gpointer parent)
{
   GtkWidget* dlg = gtk_message_dialog_new(GTK_WINDOW(parent),
      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
      GTK_MESSAGE_OTHER,
      GTK_BUTTONS_OK,
      "%s",
      message);
   gtk_window_set_title(GTK_WINDOW(dlg), title);
   gtk_dialog_run(GTK_DIALOG(dlg));
   gtk_widget_destroy(dlg);
}

/// \brief Runs a shell command and returns the output as a single string.
///        The string must be freed by the caller.
/// \param command - the command to run
/// \param msg - returns the command output
/// \return 0 on success, nonzero on failure
int get_command_output(const char* command, char** msg)
{
   FILE* fp;
   char* buffer;
   const size_t max_length = 10240;
   
   if (NULL == msg)
   {
      return -1;
   }
   *msg = NULL;
   
   buffer = (char*)malloc(max_length);
   if (NULL == buffer)
   {
      return -2;
   }
   
   fp = popen(command, "r");
   if (NULL == fp)
   {
      free(buffer);
      return -3;
   }
   
   size_t read_length = 0;
   char* ptr = buffer;
   while (NULL != fgets(ptr, (max_length - read_length), fp))
   {
      size_t length = strlen(ptr);
      read_length += length;
      ptr+= length;
   }
   
   int status = WEXITSTATUS(pclose(fp));
   
   if (0 == read_length)
   {
      free(buffer);
      buffer = NULL;
   }
   *msg = buffer;
   return status;
}

/// \brief Callback for the List Devices button
void pressed_list_button(GtkWidget* wid, gpointer ptr)
{
   // List devices button pressed
   char* msg = NULL;
   int result = get_command_output("daqhats_list_boards", &msg);
   if (0 == result)
   {
      show_message_dialog("List Devices", msg, ptr);
   }
   else
   {
      show_message_dialog("List Devices", "Error running command", ptr);
   }
   if (NULL != msg)
   {
      free(msg);
   }
}

/// \brief Callback for the Read EEPROMs button
void pressed_read_button(GtkWidget* wid, gpointer ptr)
{
   // Read EEPROMs button pressed
   // daqhats_read_eeproms must be run as root
   char* msg = NULL;
   int result = get_command_output("pkexec daqhats_read_eeproms 2>/dev/null", &msg);
   if (127 == result)
   {
      // command not found, so try gksudo
      if (NULL != msg)
      {
         free(msg);
         msg = NULL;
      }
      result = get_command_output("gksudo daqhats_read_eeproms 2>/dev/null", &msg);
   }

   switch (result)
   {
   case 0:     // success
      show_message_dialog("Read EEPROMs", msg, ptr);
      break;
   case 126:   // user canceled
      break;
   case 127:   // command not found
   default:
      show_message_dialog("Read EEPROMs", "Error running command", ptr);
      break;
   }
   if (NULL != msg)
   {
      free(msg);
   }
}

/// \brief Callback for the MCC 118 button
void pressed_118_button(GtkWidget* wid, gpointer ptr)
{
   system("/usr/share/mcc/daqhats/mcc118_control_panel &");
}

/// \brief Callback for the MCC 128 button
void pressed_128_button(GtkWidget* wid, gpointer ptr)
{
   system("/usr/share/mcc/daqhats/mcc128_control_panel &");
}

/// \brief Callback for the MCC 134 button
void pressed_134_button(GtkWidget* wid, gpointer ptr)
{
   system("/usr/share/mcc/daqhats/mcc134_control_panel &");
}

/// \brief Callback for the MCC 152 button
void pressed_152_button(GtkWidget* wid, gpointer ptr)
{
   system("/usr/share/mcc/daqhats/mcc152_control_panel &");
}

/// \brief Callback for the MCC 172 button
void pressed_172_button(GtkWidget* wid, gpointer ptr)
{
   system("/usr/share/mcc/daqhats/mcc172_control_panel &");
}


/// \brief Main function
int main (int argc, char *argv[])
{
   // init the main window
   gtk_init(&argc, &argv);
   GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(win), "MCC DAQ HAT Manager");
   g_signal_connect(win, "delete_event", G_CALLBACK(end_program), NULL);

   // create a grid for the frames
   GtkWidget* grid = gtk_grid_new();
   gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
   gtk_container_add(GTK_CONTAINER(win), grid);
   gtk_container_set_border_width(GTK_CONTAINER(win), 2);
   
   // create and organize frames
   GtkWidget* main_frame = gtk_frame_new("Manage devices");
   gtk_frame_set_shadow_type(GTK_FRAME(main_frame), GTK_SHADOW_OUT);
   gtk_widget_set_valign(main_frame, GTK_ALIGN_START);
   gtk_grid_attach(GTK_GRID(grid), main_frame, 0, 0, 1, 1);

   GtkWidget* main_grid = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(main_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(main_grid, 2);
   gtk_widget_set_margin_bottom(main_grid, 2);
   gtk_widget_set_margin_left(main_grid, 2);
   gtk_widget_set_margin_right(main_grid, 2);
   gtk_container_add(GTK_CONTAINER(main_frame), main_grid);

   GtkWidget* device_frame = gtk_frame_new("Control Apps");
   gtk_frame_set_shadow_type(GTK_FRAME(device_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), device_frame, 1, 0, 1, 1);

   GtkWidget* device_grid = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(device_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(device_grid, 2);
   gtk_widget_set_margin_bottom(device_grid, 2);
   gtk_widget_set_margin_left(device_grid, 2);
   gtk_widget_set_margin_right(device_grid, 2);
   gtk_container_add(GTK_CONTAINER(device_frame), device_grid);

   // create buttons
   GtkWidget* list_devices_button = gtk_button_new_with_label("List devices");
   g_signal_connect(list_devices_button, "clicked", G_CALLBACK(pressed_list_button), win);
   gtk_grid_attach(GTK_GRID(main_grid), list_devices_button, 0, 0, 1, 1);

   GtkWidget* read_eeprom_button = gtk_button_new_with_label("Read EEPROMs");
   g_signal_connect(read_eeprom_button, "clicked", G_CALLBACK(pressed_read_button), win);
   gtk_grid_attach(GTK_GRID(main_grid), read_eeprom_button, 0, 1, 1, 1);

   GtkWidget* mcc118_button = gtk_button_new_with_label("MCC 118 App");
   g_signal_connect(mcc118_button, "clicked", G_CALLBACK(pressed_118_button), win);
   gtk_grid_attach(GTK_GRID(device_grid), mcc118_button, 0, 0, 1, 1);
   
   GtkWidget* mcc128_button = gtk_button_new_with_label("MCC 128 App");
   g_signal_connect(mcc128_button, "clicked", G_CALLBACK(pressed_128_button), win);
   gtk_grid_attach(GTK_GRID(device_grid), mcc128_button, 0, 1, 1, 1);
   
   GtkWidget* mcc134_button = gtk_button_new_with_label("MCC 134 App");
   g_signal_connect(mcc134_button, "clicked", G_CALLBACK(pressed_134_button), win);
   gtk_grid_attach(GTK_GRID(device_grid), mcc134_button, 0, 2, 1, 1);
   
   GtkWidget* mcc152_button = gtk_button_new_with_label("MCC 152 App");
   g_signal_connect(mcc152_button, "clicked", G_CALLBACK(pressed_152_button), win);
   gtk_grid_attach(GTK_GRID(device_grid), mcc152_button, 0, 3, 1, 1);
   
   GtkWidget* mcc172_button = gtk_button_new_with_label("MCC 172 App");
   g_signal_connect(mcc172_button, "clicked", G_CALLBACK(pressed_172_button), win);
   gtk_grid_attach(GTK_GRID(device_grid), mcc172_button, 0, 4, 1, 1);
   
   gtk_widget_show_all(win);
   gtk_main();
   return 0;
}
