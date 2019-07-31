#!/usr/bin/env python3
"""
    MCC 134 Control Panel

    Purpose:
        Display the MCC 134 temperatures

    Description:
        This example reads and displays the temperatures
"""
import tkinter
import tkinter.font
from daqhats import hat_list, mcc134, HatIDs, HatError, TcTypes

class ControlApp:
    """ Application class """
    # pylint: disable=too-many-instance-attributes, too-many-statements

    def __init__(self, master):
        self.master = master
        master.title("MCC 134 Control Panel")

        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None
        self.job = None

        self.tc_types = {
            "J": TcTypes.TYPE_J,
            "K": TcTypes.TYPE_K,
            "T": TcTypes.TYPE_T,
            "E": TcTypes.TYPE_E,
            "R": TcTypes.TYPE_R,
            "S": TcTypes.TYPE_S,
            "B": TcTypes.TYPE_B,
            "N": TcTypes.TYPE_N,
            "Disabled": TcTypes.DISABLED}
        self.tc_types_reversed = dict(map(reversed, self.tc_types.items()))

        # GUI Setup

        self.bold_font = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.top_frame = tkinter.LabelFrame(master, text="Select Device")
        self.top_frame.pack(expand=False, fill=tkinter.X)

        self.mid_frame = tkinter.LabelFrame(master, text="TC Types")
        self.mid_frame.pack(expand=False, fill=tkinter.X)

        self.bottom_frame = tkinter.LabelFrame(master, text="Temperature Inputs")
        self.bottom_frame.pack(expand=True, fill=tkinter.BOTH)

        # Create widgets

        self.dev_label = tkinter.Label(self.top_frame, text="MCC 134 address:")
        self.dev_label.grid(row=0, column=0)

        self.open_button = tkinter.Button(
            self.top_frame, text="Open", width=6, command=self.pressed_open_button)

        # Get list of MCC 134 devices for the device list widget
        self.addr_list = self.list_devices()

        if not self.addr_list:
            self.device_lister = tkinter.Label(self.top_frame, text="None found")
            self.open_button.config(state=tkinter.DISABLED)
        else:
            self.device_variable = tkinter.StringVar(self.top_frame)
            self.device_variable.set(self.addr_list[0])
            self.device_lister = tkinter.OptionMenu(
                self.top_frame, self.device_variable, *self.addr_list)

        self.device_lister.grid(row=0, column=1)
        self.open_button.grid(row=0, column=2)

        self.channel_labels = []
        self.tc_type_options = []
        self.temperatures = []
        self.tc_type_vars = []
        tc_choices = ("J", "K", "T", "E", "R", "S", "B", "N", "Disabled")
        for index in range(mcc134.info().NUM_AI_CHANNELS):
            # TC types and labels
            label = tkinter.Label(self.mid_frame, text="Ch {}".format(index))
            label.grid(row=index, column=0)

            self.tc_type_vars.append(tkinter.StringVar(self.mid_frame))
            self.tc_type_vars[index].set(tc_choices[0])
            self.tc_type_options.append(
                tkinter.OptionMenu(
                    self.mid_frame, self.tc_type_vars[index], *tc_choices,
                    command=lambda value, index=index: self.pressed_tc(value, index)))
            self.tc_type_options[index].grid(row=index, column=1)
            self.tc_type_options[index].grid_configure(sticky="E")

            # Labels
            self.channel_labels.append(tkinter.Label(
                self.bottom_frame, text="Ch {}".format(index), font=self.bold_font))
            self.channel_labels[index].grid(row=index, column=1)
            self.channel_labels[index].grid_configure(sticky="W")
            # Temperatures
            self.temperatures.append(tkinter.Label(
                self.bottom_frame, text="0.00", font=self.bold_font))
            self.temperatures[index].grid(row=index, column=3)
            self.temperatures[index].grid_configure(sticky="E")

            self.bottom_frame.grid_rowconfigure(index, weight=1)

        self.mid_frame.grid_columnconfigure(1, weight=1)

        self.bottom_frame.grid_columnconfigure(1, weight=1)
        self.bottom_frame.grid_columnconfigure(2, weight=1)

        self.bottom_frame.bind("<Configure>", self.resize_text)

        # Disable widgets until a device is opened
        self.disable_controls()

        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup

        icon = tkinter.PhotoImage(file='/usr/share/mcc/daqhats/icon.png')
        # pylint: disable=protected-access
        master.tk.call('wm', 'iconphoto', master._w, icon)

    def resize_text(self, _):
        """ Resize event """
        height = self.bottom_frame.winfo_height()
        new_size = -max(12, int(height / 7))
        #new_size = -max(12, int(event.height / 8))
        self.bold_font.configure(size=new_size)

    def pressed_tc(self, value, index):
        """ TC type change event """
        tc_type = value

        if tc_type == 'Disabled':
            self.channel_labels[index].config(state=tkinter.DISABLED)
            self.temperatures[index].config(state=tkinter.DISABLED)
        else:
            self.channel_labels[index].config(state=tkinter.NORMAL)
            self.temperatures[index].config(state=tkinter.NORMAL)

        # Set the TC type
        my_type = self.tc_types[tc_type]
        self.board.tc_type_write(index, my_type)

    def disable_controls(self):
        """ Disable controls when device closed """

        # Enable the address selector
        self.device_lister.config(state=tkinter.NORMAL)
        # Disable the board controls
        for child in self.mid_frame.winfo_children():
            child.config(state=tkinter.DISABLED)

        for child in self.bottom_frame.winfo_children():
            child.config(state=tkinter.DISABLED)

    def enable_controls(self):
        """ Enable controls when device opened """

        # Disable the address selector
        self.device_lister.config(state=tkinter.DISABLED)

        # Enable the board controls
        for child in self.mid_frame.winfo_children():
            child.config(state=tkinter.NORMAL)

        for child in self.bottom_frame.winfo_children():
            child.config(state=tkinter.NORMAL)

    def list_devices(self):
        """ List detected devices """
        self.dev_list = hat_list(HatIDs.MCC_134)
        addr_list = ["{}".format(dev.address) for dev in self.dev_list]
        return addr_list

    def open_device(self, address):
        """ Open selected device """
        try:
            self.board = mcc134(address)

        except HatError:
            return False
        else:
            return True

    def close_device(self):
        """ Close the device """
        self.board = None

    def update_inputs(self):
        """ Periodically read the inputs and update the display """
        if self.device_open:
            # Read the enabled channels
            for channel in range(mcc134.info().NUM_AI_CHANNELS):
                if self.tc_type_vars[channel].get() != 'Disabled':
                    value = self.board.t_in_read(channel)
                    if value == mcc134.OPEN_TC_VALUE:
                        text = "Open"
                    elif value == mcc134.OVERRANGE_TC_VALUE:
                        text = "Overrange"
                    elif value == mcc134.COMMON_MODE_TC_VALUE:
                        text = "Common mode error"
                    else:
                        text = "{:.2f}".format(value)
                    self.temperatures[channel].config(text=text)

        # call this every second
        self.job = self.master.after(1000, self.update_inputs)

    # Event handlers
    def pressed_open_button(self):
        """ Open button pressed event """
        if self.open_button.cget('text') == "Open":
            # Open the selected device
            address = int(self.device_variable.get())
            if self.open_device(address):
                self.device_open = True
                self.open_address = address

                self.enable_controls()

                # read the TC types
                for index in range(mcc134.info().NUM_AI_CHANNELS):
                    tc_type = self.board.tc_type_read(index)
                    self.tc_type_vars[index].set(self.tc_types_reversed[tc_type])
                    if tc_type == TcTypes.DISABLED:
                        self.channel_labels[index].config(state=tkinter.DISABLED)
                        self.temperatures[index].config(state=tkinter.DISABLED)
                    else:
                        self.channel_labels[index].config(state=tkinter.NORMAL)
                        self.temperatures[index].config(state=tkinter.NORMAL)

                # Periodically read the inputs and update controls
                self.update_inputs()

                self.open_button.config(text="Close")
            else:
                tkinter.messagebox.showerror("Error", "Could not open device.")
        else:
            if self.device_open:
                self.close_device()
                self.device_open = False
            self.open_button.config(text="Open")
            self.disable_controls()

    def close(self):
        """ Application close handler """
        # a background read may be in process, so wait for it to complete
        if self.job:
            self.master.after_cancel(self.job)
        self.board = None
        self.master.destroy()

def main():
    """ Application entry point """
    root = tkinter.Tk()
    _app = ControlApp(root)
    root.mainloop()

if __name__ == '__main__':
    main()
