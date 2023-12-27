#include <daqhats/daqhats.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdlib.h>

GtkWidget* main_window = NULL;
GtkWidget* device_address_combo = NULL;
GtkWidget* digital_frame = NULL;
GtkWidget* analog_frame = NULL;
GtkWidget** dio_dir_buttons = NULL;
GtkWidget** dio_input_labels = NULL;
GtkWidget** dio_output_buttons = NULL;
GtkWidget** analog_output_scales = NULL;
guint update_thread;
int address = 0;
bool device_open = false;

/// @brief Callback to end the program
/// @param wid - the widget (not used)
/// @param ptr - user data (not used)
void end_program(GtkWidget* wid, gpointer ptr)
{
   if (device_open)
   {
      g_source_remove(update_thread);
      mcc152_close(address);
   }
   gtk_main_quit();
}

/// @brief Enable or disable the controls
/// @param enable - true to enable, false to disable
void enable_controls(bool enable)
{
   if (device_address_combo != NULL)
   {
      // The address combo is disabled when the other controls are enabled and vice versa.
      gtk_widget_set_sensitive(device_address_combo, !enable);
   }

   gtk_widget_set_sensitive(digital_frame, enable);
   gtk_widget_set_sensitive(analog_frame, enable);
}

/// @brief Timer callback to update the DIO inputs
/// @param ptr - not used
/// @return true to continue the timer, false to stop
int update_inputs(void* ptr)
{
   uint8_t value = 0;
   char text[4];

   // read and display the DIO input values
   if (!device_open)
   {
      return false;
   }

   for (int i = 0; i < mcc152_info()->NUM_DIO_CHANNELS; i++)
   {
      if (mcc152_dio_input_read_bit(address, i, &value) == RESULT_SUCCESS)
      {
         sprintf(text, "%d", value);
         gtk_label_set_text(GTK_LABEL(dio_input_labels[i]), text);
      }
   }
   return true;
}

/// @brief Callback for open button clicked
/// @param wid - the button that was clicked
/// @param ptr - user data (not used)
void clicked_open_button(GtkWidget* wid, gpointer ptr)
{
   const char* label = gtk_button_get_label(GTK_BUTTON(wid));
   if (strcmp(label, "Open") == 0)
   {
      // Open the selected device
      if (device_address_combo != NULL)
      {
         char* text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(device_address_combo));
         sscanf(text, "%d", &address);
         if (mcc152_open(address) == RESULT_SUCCESS)
         {
            device_open = true;
            enable_controls(true);
            gtk_button_set_label(GTK_BUTTON(wid), "Close");

            // get the DIO direction and output states and update the controls
            for (int i = 0; i < mcc152_info()->NUM_DIO_CHANNELS; i++)
            {
               uint8_t value = 0;
               if (mcc152_dio_config_read_bit(address, i, DIO_DIRECTION, &value) == RESULT_SUCCESS)
               {
                  if (value == 0)
                  {
                     gtk_button_set_label(GTK_BUTTON(dio_dir_buttons[i]), "Output");
                     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dio_dir_buttons[i]), TRUE);
                  }
                  else
                  {
                     gtk_button_set_label(GTK_BUTTON(dio_dir_buttons[i]), "Input");
                     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dio_dir_buttons[i]), FALSE);
                  }
               }

               if (mcc152_dio_output_read_bit(address, i, &value) == RESULT_SUCCESS)
               {
                  if (value == 0)
                  {
                     gtk_button_set_label(GTK_BUTTON(dio_output_buttons[i]), "0");
                     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dio_output_buttons[i]), TRUE);
                  }
                  else
                  {
                     gtk_button_set_label(GTK_BUTTON(dio_output_buttons[i]), "1");
                     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dio_output_buttons[i]), FALSE);
                  }
               }
            }

            // set the analog output values to 0
            for (int i = 0; i < mcc152_info()->NUM_AO_CHANNELS; i++)
            {
               gtk_range_set_value(GTK_RANGE(analog_output_scales[i]), 0.0);
            }

            update_inputs(NULL);
            update_thread = g_timeout_add(200, update_inputs, NULL);
         }
      }
   }
   else
   {
      // Close the device
      g_source_remove(update_thread);
      mcc152_close(address);
      device_open = false;
      enable_controls(false);
      gtk_button_set_label(GTK_BUTTON(wid), "Open");
   }      
}

/// @brief A DIO direction button was toggled
/// @param wid - the button that was toggled
/// @param ptr - the DIO index
void toggled_dio_dir(GtkWidget*wid, gpointer ptr)
{
   unsigned int index = GPOINTER_TO_UINT(ptr);

   const char* label = gtk_button_get_label(GTK_BUTTON(wid));
   bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wid));
   if (active && (strcmp(label, "Output") != 0))
   {
      // set to output
      mcc152_dio_config_write_bit(address, index, DIO_DIRECTION, 0);
      gtk_button_set_label(GTK_BUTTON(wid), "Output");
   }
   else if (!active && (strcmp(label, "Input") != 0))
   {
      // set to input
      mcc152_dio_config_write_bit(address, index, DIO_DIRECTION, 1);
      gtk_button_set_label(GTK_BUTTON(wid), "Input");
   }
}

/// @brief A DIO output state button was toggled
/// @param wid - the button that was toggled
/// @param ptr - the DIO index
void toggled_dio_output(GtkWidget*wid, gpointer ptr)
{
   unsigned int index = GPOINTER_TO_UINT(ptr);

   const char* label = gtk_button_get_label(GTK_BUTTON(wid));
   bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wid));
   if (active && (strcmp(label, "0") != 0))
   {
      // set to 0
      mcc152_dio_output_write_bit(address, index, 0);
      gtk_button_set_label(GTK_BUTTON(wid), "0");
   }
   else if (!active && (strcmp(label, "1") != 0))
   {
      // set to 1
      mcc152_dio_output_write_bit(address, index, 1);
      gtk_button_set_label(GTK_BUTTON(wid), "1");
   }
}

/// @brief An analog output slider value changed
/// @param wid - the slider that changed
/// @param ptr - the channel index
void changed_analog_output(GtkWidget*wid, gpointer ptr)
{
   unsigned int index = GPOINTER_TO_UINT(ptr);
   double value = gtk_range_get_value(GTK_RANGE(wid));

   mcc152_a_out_write(address, index, 0, value);
}

/// @brief Set up the GUI 
void setup_controls(void)
{
   main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(main_window), "MCC 152 Control Panel");
   g_signal_connect(main_window, "delete_event", G_CALLBACK(end_program), NULL);

   // create a grid for the frames
   GtkWidget* grid = gtk_grid_new();
   gtk_container_add(GTK_CONTAINER(main_window), grid);
   gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
   gtk_container_set_border_width(GTK_CONTAINER(main_window), 2);

   // create and organize frames
   GtkWidget* top_frame = gtk_frame_new("Select device");
   gtk_frame_set_shadow_type(GTK_FRAME(top_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), top_frame, 0, 0, 2, 1);

   GtkWidget* top_grid = gtk_grid_new();
   gtk_grid_set_column_spacing(GTK_GRID(top_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(top_grid, 2);
   gtk_widget_set_margin_bottom(top_grid, 2);
   gtk_widget_set_margin_left(top_grid, 2);
   gtk_widget_set_margin_right(top_grid, 2);
   gtk_container_add(GTK_CONTAINER(top_frame), top_grid);

   // create the top frame widgets
   GtkWidget* lbl = gtk_label_new("MCC 152 address: ");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(top_grid), lbl, 0, 0, 1, 1);
   
   // get the list of MCC 152 devices
   struct HatInfo* dev_list = NULL;
   int count = hat_list(HAT_ID_MCC_152, NULL);
   if (count > 0)
   {
      dev_list = (struct HatInfo*)malloc(count * sizeof(struct HatInfo));
      count = hat_list(HAT_ID_MCC_152, dev_list);

      device_address_combo = gtk_combo_box_text_new();
      gtk_widget_set_hexpand(device_address_combo, TRUE);
      gtk_grid_attach(GTK_GRID(top_grid), device_address_combo, 1, 0, 1, 1);
      char addr_str[4];
      for (int i = 0; i < count; i++)
      {
         sprintf(addr_str, "%d", dev_list[i].address);
         gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(device_address_combo), addr_str);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(device_address_combo), 0);
      free(dev_list);
   }
   else
   {
      lbl = gtk_label_new("None found");
      gtk_widget_set_hexpand(lbl, TRUE);
      gtk_grid_attach(GTK_GRID(top_grid), lbl, 1, 0, 1, 1);
   }
   
   GtkWidget* open_button = gtk_button_new_with_label("Open");
   gtk_widget_set_hexpand(open_button, TRUE);
   gtk_grid_attach(GTK_GRID(top_grid), open_button, 2, 0, 1, 1);
   g_signal_connect(open_button, "clicked", G_CALLBACK(clicked_open_button), NULL);
   
   if (count == 0)
   {
      gtk_widget_set_sensitive(open_button, FALSE);
   }

   // Create the digital frame   
   digital_frame = gtk_frame_new("Digital I/O");
   gtk_frame_set_shadow_type(GTK_FRAME(digital_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), digital_frame, 0, 1, 1, 1);

   GtkWidget* digital_grid = gtk_grid_new();
   gtk_grid_set_column_spacing(GTK_GRID(digital_grid), 5);
   gtk_grid_set_row_spacing(GTK_GRID(digital_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(digital_grid, 2);
   gtk_widget_set_margin_bottom(digital_grid, 2);
   gtk_widget_set_margin_left(digital_grid, 2);
   gtk_widget_set_margin_right(digital_grid, 2);
   gtk_container_add(GTK_CONTAINER(digital_frame), digital_grid);

   // create the digital frame widgets
   lbl = gtk_label_new(NULL);
   gtk_label_set_markup(GTK_LABEL(lbl), "<b>DIO #</b>");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_widget_set_vexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(digital_grid), lbl, 0, 0, 1, 1);
   lbl = gtk_label_new(NULL);
   gtk_label_set_markup(GTK_LABEL(lbl), "<b>Direction</b>");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_widget_set_vexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(digital_grid), lbl, 1, 0, 1, 1);
   lbl = gtk_label_new(NULL);
   gtk_label_set_markup(GTK_LABEL(lbl), "<b>Input State</b>");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_widget_set_vexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(digital_grid), lbl, 2, 0, 1, 1);
   lbl = gtk_label_new(NULL);
   gtk_label_set_markup(GTK_LABEL(lbl), "<b>Output State</b>");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_widget_set_vexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(digital_grid), lbl, 3, 0, 1, 1);

   dio_dir_buttons = malloc(mcc152_info()->NUM_DIO_CHANNELS * sizeof(GtkWidget*));
   dio_input_labels = malloc(mcc152_info()->NUM_DIO_CHANNELS * sizeof(GtkWidget*));
   dio_output_buttons = malloc(mcc152_info()->NUM_DIO_CHANNELS * sizeof(GtkWidget*));

   for (int i = 0; i < mcc152_info()->NUM_DIO_CHANNELS; i++)
   {
      char text[4];

      // Channel #
      sprintf(text, "%d", i);
      lbl = gtk_label_new(text);
      gtk_widget_set_hexpand(lbl, TRUE);
      gtk_widget_set_vexpand(lbl, TRUE);
      gtk_grid_attach(GTK_GRID(digital_grid), lbl, 0, i + 1, 1, 1);

      // Direction button
      dio_dir_buttons[i] = gtk_toggle_button_new_with_label("Output");
      //gtk_widget_set_halign(dio_dir_buttons[i], GTK_ALIGN_CENTER);
      gtk_widget_set_valign(dio_dir_buttons[i], GTK_ALIGN_CENTER);
      g_signal_connect(dio_dir_buttons[i], "toggled", G_CALLBACK(toggled_dio_dir), GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(digital_grid), dio_dir_buttons[i], 1, i + 1, 1, 1);

      // Input state label
      dio_input_labels[i] = gtk_label_new("1");
      gtk_widget_set_hexpand(dio_input_labels[i], TRUE);
      gtk_widget_set_vexpand(dio_input_labels[i], TRUE);
      gtk_grid_attach(GTK_GRID(digital_grid), dio_input_labels[i], 2, i + 1, 1, 1);

      // Output state button
      dio_output_buttons[i] = gtk_toggle_button_new_with_label("1");
      gtk_widget_set_halign(dio_output_buttons[i], GTK_ALIGN_CENTER);
      gtk_widget_set_valign(dio_output_buttons[i], GTK_ALIGN_CENTER);
      g_signal_connect(dio_output_buttons[i], "toggled", G_CALLBACK(toggled_dio_output), GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(digital_grid), dio_output_buttons[i], 3, i + 1, 1, 1);
   }

   // create the analog frame / widgets
   analog_frame = gtk_frame_new("Analog Outputs");
   gtk_frame_set_shadow_type(GTK_FRAME(analog_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), analog_frame, 1, 1, 1, 1);
      
   // Use a grid for the widgets
   GtkWidget* analog_grid = gtk_grid_new();
   gtk_grid_set_column_spacing(GTK_GRID(analog_grid), 5);
   gtk_grid_set_row_spacing(GTK_GRID(analog_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(analog_grid, 2);
   gtk_widget_set_margin_bottom(analog_grid, 2);
   gtk_widget_set_margin_left(analog_grid, 2);
   gtk_widget_set_margin_right(analog_grid, 2);
   gtk_container_add(GTK_CONTAINER(analog_frame), analog_grid);

   analog_output_scales = malloc(mcc152_info()->NUM_AO_CHANNELS * sizeof(GtkWidget*));

   for (int i = 0; i < mcc152_info()->NUM_AO_CHANNELS; i++)
   {
      char text[20];

      // Channel label
      lbl = gtk_label_new(NULL);
      sprintf(text, "<b>Channel %d</b>", i);
      gtk_label_set_markup(GTK_LABEL(lbl), text);
      gtk_widget_set_hexpand(lbl, TRUE);
      gtk_grid_attach(GTK_GRID(analog_grid), lbl, i, 0, 1, 1);

      // Output voltage scale
      analog_output_scales[i] = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL,
         mcc152_info()->AO_MIN_VOLTAGE, mcc152_info()->AO_MAX_VOLTAGE, 0.1);
      gtk_range_set_inverted(GTK_RANGE(analog_output_scales[i]), TRUE);
      gtk_scale_set_has_origin(GTK_SCALE(analog_output_scales[i]), TRUE);
      gtk_scale_set_value_pos(GTK_SCALE(analog_output_scales[i]), GTK_POS_BOTTOM);
      gtk_scale_add_mark(GTK_SCALE(analog_output_scales[i]), 1.0, GTK_POS_RIGHT, "1.0");
      gtk_scale_add_mark(GTK_SCALE(analog_output_scales[i]), 2.0, GTK_POS_RIGHT, "2.0");
      gtk_scale_add_mark(GTK_SCALE(analog_output_scales[i]), 3.0, GTK_POS_RIGHT, "3.0");
      gtk_scale_add_mark(GTK_SCALE(analog_output_scales[i]), 4.0, GTK_POS_RIGHT, "4.0");
      gtk_scale_add_mark(GTK_SCALE(analog_output_scales[i]), 5.0, GTK_POS_RIGHT, "5.0");
      gtk_widget_set_hexpand(analog_output_scales[i], TRUE);
      gtk_widget_set_vexpand(analog_output_scales[i], TRUE);
      g_signal_connect(analog_output_scales[i], "value-changed", G_CALLBACK(changed_analog_output), GINT_TO_POINTER(i));
      gtk_grid_attach(GTK_GRID(analog_grid), analog_output_scales[i], i, 1, 1, 1);
   }
}

/// @brief Clean up after destroying the GUI
void cleanup_controls(void)
{
   free(dio_dir_buttons);
   free(dio_input_labels);
   free(dio_output_buttons);
   free(analog_output_scales);
}

/// @brief Main function
int main (int argc, char *argv[])
{
   // init the main window
   gtk_init(&argc, &argv);

   setup_controls();
   enable_controls(false);

   gtk_widget_show_all(main_window);
   gtk_main();

   cleanup_controls();
   return 0;
}
