#!/usr/bin/env python3
"""
    MCC 172 Control Panel

    Purpose:
        Display the MCC 172 input RMS voltages

    Description:
        This app reads and displays the input voltages.
"""
import tkinter
from tkinter import messagebox
import tkinter.font
import math
from daqhats import hat_list, mcc172, HatIDs

def calc_rms(data, channel, num_channels, num_samples_per_channel):
    """ Calculate RMS of a data buffer """
    value = 0.0
    for i in range(num_samples_per_channel):
        index = (i * num_channels) + channel
        value += (data[index] * data[index]) / num_samples_per_channel

    return math.sqrt(value)

class ControlApp:
    """ Control application class """

    # pylint: disable=too-many-instance-attributes
    
    def __init__(self, master):
        """ Initialize the class """
        self.master = master
        master.title("MCC 172 Control Panel")

        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None
        self.scan_run = False
        self.sample_rate = 0

        # GUI Setup

        self.BOLD_FONT = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.top_frame = tkinter.LabelFrame(master, text="Select Device")
        self.top_frame.pack(side=tkinter.TOP, expand=False, fill=tkinter.X)

        self.bottom_frame = tkinter.LabelFrame(master, text="Analog Inputs")
        self.bottom_frame.pack(side=tkinter.BOTTOM, expand=True, fill=tkinter.BOTH)

        # Create widgets

        self.dev_label = tkinter.Label(self.top_frame, text="MCC 172 address:")
        self.dev_label.grid(row=0, column=0)

        self.open_button = tkinter.Button(self.top_frame, text="Open", width=6,
                                          command=self.pressed_open_button)

        # Get list of MCC 172 devices for the device list widget
        self.addr_list = self.list_devices()

        if not self.addr_list:
            self.device_lister = tkinter.Label(self.top_frame, text="None found")
            self.open_button.config(state=tkinter.DISABLED)
        else:
            self.device_variable = tkinter.StringVar(self.top_frame)
            self.device_variable.set(self.addr_list[0])
            self.device_lister = tkinter.OptionMenu(self.top_frame,
                                                    self.device_variable,
                                                    *self.addr_list)

        self.device_lister.grid(row=0, column=1)
        self.open_button.grid(row=0, column=2)

        self.checkboxes = []
        self.check_values = []
        self.channel_labels = []
        self.voltages = []
        for index in range(mcc172.info().NUM_AI_CHANNELS):
            # Checkboxes
            self.check_values.append(tkinter.IntVar())
            self.checkboxes.append(tkinter.Checkbutton(
                self.bottom_frame,
                variable=self.check_values[index],
                command=lambda index=index: self.pressed_check(index)))
            self.checkboxes[index].grid(row=index, column=0)
            self.checkboxes[index].select()
            # Labels
            self.channel_labels.append(tkinter.Label(
                self.bottom_frame,
                text="Ch {}".format(index), font=self.BOLD_FONT))
            self.channel_labels[index].grid(row=index, column=1)
            self.channel_labels[index].grid_configure(sticky="W")
            # Voltages
            self.voltages.append(tkinter.Label(
                self.bottom_frame, text="0.000",
                font=self.BOLD_FONT))
            self.voltages[index].grid(row=index, column=2)
            self.voltages[index].grid_configure(sticky="E")

            self.bottom_frame.grid_rowconfigure(index, weight=1)

        self.bottom_frame.grid_columnconfigure(1, weight=1)
        self.bottom_frame.grid_columnconfigure(2, weight=1)

        self.bottom_frame.bind("<Configure>", self.resize_text)

        # Disable widgets until a device is opened
        self.disable_controls()

        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup

        icon = tkinter.PhotoImage(file='/usr/share/mcc/daqhats/icon.png')
        master.tk.call('wm', 'iconphoto', master._w, icon)

    def resize_text(self, event):
        """ Text resize event """
        new_size = -max(12, int(event.height / 12))
        self.BOLD_FONT.configure(size=new_size)

    def pressed_check(self, index):
        """ Check button pressed event """
        if self.check_values[index].get() == 0:
            self.channel_labels[index].config(state=tkinter.DISABLED)
            self.voltages[index].config(state=tkinter.DISABLED)
        else:
            self.channel_labels[index].config(state=tkinter.NORMAL)
            self.voltages[index].config(state=tkinter.NORMAL)

    def disable_controls(self):
        """ Disable controls when board not opened """
        # Enable the address selector
        self.device_lister.config(state=tkinter.NORMAL)
        # Disable the board controls
        for child in self.bottom_frame.winfo_children():
            child.config(state=tkinter.DISABLED)

    def enable_controls(self):
        """ Enable controls when board opened """
        # Disable the address selector
        self.device_lister.config(state=tkinter.DISABLED)
        # Enable the board controls
        for child in self.bottom_frame.winfo_children():
            child.config(state=tkinter.NORMAL)

    def list_devices(self):
        """ List attached devices """
        self.dev_list = hat_list(HatIDs.MCC_172)
        addr_list = ["{}".format(dev.address) for dev in self.dev_list]
        return addr_list

    def open_device(self, address):
        """ Open the specified device """
        try:
            self.board = mcc172(address)

            # Set the sampling clock rate
            self.sample_rate = 10240

            # Configure IEPE

            self.scan_run = False
        except:
            return False
        else:
            return True

    def close_device(self):
        """ Close the device """
        self.board = None

    def update_inputs(self):
        """ Periodically display the input values """
        if self.device_open:
            # Read a block of samples from the channel then calculate
            # RMS AC voltage (obtained during previous scan).
            if self.scan_run:
                num_channels = mcc172.info().NUM_AI_CHANNELS
                scan_results = self.board.a_in_scan_read(-1, 0)

                for channel in range(num_channels):
                    if self.check_values[channel].get() == 1:
                        value = calc_rms(scan_results.data,
                                         channel, num_channels,
                                         len(scan_results.data) / num_channels)

                        self.voltages[channel].config(
                            text="{:.3f}".format(value))

            # Start a new scan
            self.board.a_in_scan_start(3, self.sample_rate * 0.1)
            self.scan_run = True

            # schedule another update in 200 ms
            self.master.after(200, self.update_inputs)

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

                # Periodically read the inputs and update controls
                self.update_inputs()

                self.open_button.config(text="Close")
            else:
                messagebox.showerror("Error", "Could not open device.")
        else:
            if self.device_open:
                self.close_device()
                self.device_open = False
            self.open_button.config(text="Open")
            self.disable_controls()

    def close(self):
        """ Close the app """
        self.master.destroy()

def main():
    """ App entry point """
    root = tkinter.Tk()
    app = ControlApp(root)
    root.mainloop()

if __name__ == '__main__':
    main()
