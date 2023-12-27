#include <daqhats/daqhats.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdlib.h>

const char* const tc_types[] =
{
   "J",
   "K",
   "T",
   "E",
   "R",
   "S",
   "B",
   "N",
   "Disabled"
};

const int num_tc_types = sizeof(tc_types)/sizeof(const char*);

GtkWidget* main_window = NULL;
GtkWidget* device_address_combo = NULL;
GtkWidget* digital_frame = NULL;
GtkWidget* analog_frame = NULL;
GtkWidget** tc_type_combos = NULL;
GtkWidget** channel_labels = NULL;
GtkWidget** voltage_labels = NULL;
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
      mcc134_close(address);
   }
   gtk_main_quit();
}

/// @brief Callback for TC type changed
/// @param wid - the combo box that changed
/// @param ptr - the channel index passed as a gpointer
void changed_tc_type_combo(GtkComboBox* wid, gpointer ptr)
{
   uint8_t type = TC_DISABLED;
   uint8_t index = (uint8_t)GPOINTER_TO_UINT(ptr);

   gint value = gtk_combo_box_get_active(wid);
   if (value <= TC_TYPE_N)
   {
      type = (uint8_t)value;
      gtk_widget_set_sensitive(channel_labels[index], TRUE);
      gtk_widget_set_sensitive(voltage_labels[index], TRUE);
   }
   else
   {
      // disabled
      gtk_widget_set_sensitive(channel_labels[index], FALSE);
      gtk_widget_set_sensitive(voltage_labels[index], FALSE);
   }
   mcc134_tc_type_write(address, index, type);
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

/// @brief Timer callback to update the temperature readings
/// @param ptr - not used
/// @return true to continue the timer, false to stop
int update_inputs(void* ptr)
{
   double value = 0.0;
   char text[24];

   // read and display the input voltages
   if (!device_open)
   {
      return false;
   }

   for (int i = 0; i < mcc134_info()->NUM_AI_CHANNELS; i++)
   {
      if (gtk_combo_box_get_active(GTK_COMBO_BOX(tc_type_combos[i])) != (TC_TYPE_N + 1))
      {
         // update enabled channels
         if (mcc134_t_in_read(address, i, &value) == RESULT_SUCCESS)
         {
            if (value == OPEN_TC_VALUE)
            {
               sprintf(text, "Open");
            }
            else if (value == OVERRANGE_TC_VALUE)
            {
               sprintf(text, "Overrange");
            }
            else if (value == COMMON_MODE_TC_VALUE)
            {
               sprintf(text, "Common mode error");
            }
            else
            {
               sprintf(text, "%.2f", value);
            }
            gtk_label_set_text(GTK_LABEL(voltage_labels[i]), text);
         }
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
         if (mcc134_open(address) == RESULT_SUCCESS)
         {
            device_open = true;
            enable_controls(true);
            gtk_button_set_label(GTK_BUTTON(wid), "Close");

            // Read the current TC type settings
            uint8_t type = TC_DISABLED;
            for (int i = 0; i < mcc134_info()->NUM_AI_CHANNELS; i++)
            {
               mcc134_tc_type_read(address, i, &type);
               if (type == TC_DISABLED)
               {
                  gtk_combo_box_set_active(GTK_COMBO_BOX(tc_type_combos[i]), TC_TYPE_N + 1);
               }
               else
               {
                  gtk_combo_box_set_active(GTK_COMBO_BOX(tc_type_combos[i]), i);
               }
            }

            update_inputs(NULL);
            update_thread = g_timeout_add(500, update_inputs, NULL);
         }
      }
   }
   else
   {
      // Close the device
      g_source_remove(update_thread);
      mcc134_close(address);
      device_open = false;
      enable_controls(false);
      gtk_button_set_label(GTK_BUTTON(wid), "Open");
   }
}

/// @brief Set up the GUI 
void setup_controls(void)
{
   main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(main_window), "MCC 134 Control Panel");
   g_signal_connect(main_window, "delete_event", G_CALLBACK(end_program), NULL);

   // create a grid for the frames
   GtkWidget* grid = gtk_grid_new();
   gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
   gtk_container_add(GTK_CONTAINER(main_window), grid);
   gtk_container_set_border_width(GTK_CONTAINER(main_window), 2);

   // create and organize frames
   GtkWidget* top_frame = gtk_frame_new("Select device");
   gtk_frame_set_shadow_type(GTK_FRAME(top_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), top_frame, 0, 0, 1, 1);

   GtkWidget* top_grid = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(top_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(top_grid, 2);
   gtk_widget_set_margin_bottom(top_grid, 2);
   gtk_widget_set_margin_left(top_grid, 2);
   gtk_widget_set_margin_right(top_grid, 2);
   gtk_container_add(GTK_CONTAINER(top_frame), top_grid);

   // create the select frame widgets
   GtkWidget* lbl = gtk_label_new("MCC 134 address: ");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(top_grid), lbl, 0, 0, 1, 1);
   
   // get the list of MCC 134 devices
   struct HatInfo* dev_list = NULL;
   int count = hat_list(HAT_ID_MCC_134, NULL);
   if (count > 0)
   {
      dev_list = (struct HatInfo*)malloc(count * sizeof(struct HatInfo));
      count = hat_list(HAT_ID_MCC_134, dev_list);

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
      
   // Create the config frame   
   digital_frame = gtk_frame_new("TC Types");
   gtk_frame_set_shadow_type(GTK_FRAME(digital_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), digital_frame, 0, 1, 1, 1);

   GtkWidget* config_grid = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(config_grid), 2);
   gtk_grid_set_column_spacing(GTK_GRID(config_grid), 2);
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(config_grid, 2);
   gtk_widget_set_margin_bottom(config_grid, 2);
   gtk_widget_set_margin_left(config_grid, 2);
   gtk_widget_set_margin_right(config_grid, 2);
   gtk_container_add(GTK_CONTAINER(digital_frame), config_grid);

   // create the config frame widgets
   tc_type_combos = malloc(mcc134_info()->NUM_AI_CHANNELS * sizeof(GtkWidget*));
   for (int i = 0; i < mcc134_info()->NUM_AI_CHANNELS; i++)
   {
      char text[8];
      sprintf(text, "Ch %d", i);
      lbl = gtk_label_new(text);
      gtk_widget_set_hexpand(lbl, TRUE);
      gtk_grid_attach(GTK_GRID(config_grid), lbl, 0, i, 1, 1);

      tc_type_combos[i] = gtk_combo_box_text_new();
      gtk_widget_set_hexpand(tc_type_combos[i], TRUE);
      gtk_grid_attach(GTK_GRID(config_grid), tc_type_combos[i], 1, i, 1, 1);
      for (int j = 0; j < num_tc_types; j++)
      {
         gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tc_type_combos[i]), tc_types[j]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(tc_type_combos[i]), 0);
      g_signal_connect(tc_type_combos[i], "changed", G_CALLBACK(changed_tc_type_combo), GINT_TO_POINTER(i));
   }

   // create the analog frame / widgets
   analog_frame = gtk_frame_new("Temperature Inputs");
   gtk_frame_set_shadow_type(GTK_FRAME(analog_frame), GTK_SHADOW_OUT);
   gtk_grid_attach(GTK_GRID(grid), analog_frame, 0, 2, 1, 1);
      
   // Use a grid for the widgets
   GtkWidget* analog_grid = gtk_grid_new();
   // set spacing around the outside of the grid
   gtk_widget_set_margin_top(analog_grid, 2);
   gtk_widget_set_margin_bottom(analog_grid, 2);
   gtk_widget_set_margin_left(analog_grid, 2);
   gtk_widget_set_margin_right(analog_grid, 2);
   gtk_container_add(GTK_CONTAINER(analog_frame), analog_grid);

   lbl = gtk_label_new(NULL);
   gtk_label_set_markup(GTK_LABEL(lbl), "<b>Channel</b>");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_widget_set_vexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(analog_grid), lbl, 0, 0, 1, 1);
   lbl = gtk_label_new(NULL);
   gtk_label_set_markup(GTK_LABEL(lbl), "<b>Temperature, C</b>");
   gtk_widget_set_hexpand(lbl, TRUE);
   gtk_widget_set_vexpand(lbl, TRUE);
   gtk_grid_attach(GTK_GRID(analog_grid), lbl, 1, 0, 1, 1);

   channel_labels = malloc(mcc134_info()->NUM_AI_CHANNELS * sizeof(GtkWidget*));
   voltage_labels = malloc(mcc134_info()->NUM_AI_CHANNELS * sizeof(GtkWidget*));
   for (int index = 0; index < mcc134_info()->NUM_AI_CHANNELS; index++)
   {
      // Channels
      char text[8];
      sprintf(text, "Ch %d", index);
      channel_labels[index] = gtk_label_new(text);
      gtk_widget_set_hexpand(channel_labels[index], TRUE);
      gtk_widget_set_vexpand(channel_labels[index], TRUE);
      gtk_widget_set_halign(channel_labels[index], GTK_ALIGN_CENTER);
      gtk_grid_attach(GTK_GRID(analog_grid), channel_labels[index], 0, index + 1, 1, 1);

      // Voltages
      voltage_labels[index] = gtk_label_new("0.00");
      gtk_widget_set_hexpand(voltage_labels[index], TRUE);
      gtk_widget_set_vexpand(voltage_labels[index], TRUE);
      gtk_widget_set_halign(voltage_labels[index], GTK_ALIGN_CENTER);
      gtk_grid_attach(GTK_GRID(analog_grid), voltage_labels[index], 1, index + 1, 1, 1);
   }
}

/// @brief Clean up after destroying the GUI
void cleanup_controls(void)
{
   free(tc_type_combos);
   free(channel_labels);
   free(voltage_labels);
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
