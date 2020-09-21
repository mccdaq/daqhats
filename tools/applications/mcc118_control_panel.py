#!/usr/bin/env python3
"""
    MCC 118 Control Panel

    Purpose:
        Display the MCC 118 input voltages

    Description:
        This app reads and displays the input voltages.
"""
import tkinter
import tkinter.font
from daqhats import hat_list, mcc118, HatIDs, HatError

class ControlApp:
    """ Application class """
    # pylint: disable=too-many-instance-attributes

    def __init__(self, master):
        self.master = master
        master.title("MCC 118 Control Panel")

        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None

        # GUI Setup

        self.bold_font = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.top_frame = tkinter.LabelFrame(master, text="Select Device")
        self.top_frame.pack(side=tkinter.TOP, expand=False, fill=tkinter.X)

        self.bottom_frame = tkinter.LabelFrame(master, text="Analog Inputs")
        self.bottom_frame.pack(side=tkinter.BOTTOM, expand=True, fill=tkinter.BOTH)

        # Create widgets

        self.dev_label = tkinter.Label(self.top_frame, text="MCC 118 address:")
        self.dev_label.grid(row=0, column=0)

        self.open_button = tkinter.Button(
            self.top_frame, text="Open", width=6, command=self.pressed_open_button)

        # Get list of MCC 118 devices for the device list widget
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

        self.checkboxes = []
        self.check_values = []
        self.channel_labels = []
        self.voltages = []
        for index in range(mcc118.info().NUM_AI_CHANNELS):
            # Checkboxes
            self.check_values.append(tkinter.IntVar())
            self.checkboxes.append(
                tkinter.Checkbutton(
                    self.bottom_frame,
                    variable=self.check_values[index],
                    command=lambda index=index: self.pressed_check(index)))
            self.checkboxes[index].grid(row=index, column=0)
            self.checkboxes[index].select()
            # Labels
            self.channel_labels.append(tkinter.Label(
                self.bottom_frame, text="Ch {}".format(index), font=self.bold_font))
            self.channel_labels[index].grid(row=index, column=1)
            self.channel_labels[index].grid_configure(sticky="W")
            # Voltages
            self.voltages.append(tkinter.Label(
                self.bottom_frame, text="0.000", font=self.bold_font))
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
        # pylint: disable=protected-access
        master.tk.call('wm', 'iconphoto', master._w, icon)

    def resize_text(self, _):
        """ Window resize event """
        height = self.bottom_frame.winfo_height()
        new_size = -max(12, int(height / 13))
        self.bold_font.configure(size=new_size)

    def pressed_check(self, index):
        """ Check button pressed event """
        if self.check_values[index].get() == 0:
            self.channel_labels[index].config(state=tkinter.DISABLED)
            self.voltages[index].config(state=tkinter.DISABLED)
        else:
            self.channel_labels[index].config(state=tkinter.NORMAL)
            self.voltages[index].config(state=tkinter.NORMAL)

    def disable_controls(self):
        """ Disable controls when board closed """
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
        # Reset the channels to enabled
        for index in range(mcc118.info().NUM_AI_CHANNELS):
            self.check_values[index].set(1)

    def list_devices(self):
        """ List detected devices """
        self.dev_list = hat_list(HatIDs.MCC_118)
        addr_list = ["{}".format(dev.address) for dev in self.dev_list]
        return addr_list

    def open_device(self, address):
        """ Open the selected device """
        try:
            self.board = mcc118(address)
        except HatError:
            return False
        else:
            return True

    def close_device(self):
        """ Close the device """
        self.board = None

    def update_inputs(self):
        """ Periodically read inputs and display the values """
        if self.device_open:
            for channel in range(mcc118.info().NUM_AI_CHANNELS):
                if self.check_values[channel].get() == 1:
                    value = self.board.a_in_read(channel)
                    self.voltages[channel].config(text="{:.3f}".format(value))

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

                self.open_button.config(text="Close")

                # Periodically read the inputs and update controls
                self.update_inputs()
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
        self.master.destroy()

def main():
    """ Application entry point """
    root = tkinter.Tk()
    _app = ControlApp(root)
    root.mainloop()

if __name__ == '__main__':
    main()
