#!/usr/bin/env python3
"""
    MCC 152 Control Panel

    Purpose:
        Control the MCC 152

    Description:
        This program allows viewing MCC 152 input states and controlling outputs.
"""
import tkinter
import tkinter.font
from daqhats import hat_list, mcc152, HatIDs, HatError, DIOConfigItem

class ControlApp:
    """ Application class """
    # pylint: disable=too-many-instance-attributes, too-many-statements

    def __init__(self, master):
        self.master = master
        master.title("MCC 152 Control Panel")

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

        self.left_frame = tkinter.LabelFrame(master, text="Digital I/O")
        self.left_frame.pack(side=tkinter.LEFT, expand=True, fill=tkinter.BOTH)

        self.right_frame = tkinter.LabelFrame(master, text="Analog Output")
        self.right_frame.pack(side=tkinter.RIGHT, fill=tkinter.Y)

        # Create widgets

        self.dev_label = tkinter.Label(self.top_frame, text="MCC 152 address:")
        self.dev_label.grid(row=0, column=0)

        self.open_button = tkinter.Button(
            self.top_frame, text="Open", width=6, command=self.pressed_open_button)

        # Get list of MCC 152 devices for the device list widget
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

        self.col_labels = []
        self.col_labels.append(tkinter.Label(
            self.left_frame, text="DIO #", font=self.bold_font, padx=3))
        self.col_labels.append(tkinter.Label(
            self.left_frame, text="Direction", font=self.bold_font, padx=3))
        self.col_labels.append(tkinter.Label(
            self.left_frame, text="Input State", font=self.bold_font, padx=3))
        self.col_labels.append(tkinter.Label(
            self.left_frame, text="Output State", font=self.bold_font, padx=3))
        self.col_labels[0].grid(row=0, column=0)
        self.col_labels[1].grid(row=0, column=1)
        self.col_labels[2].grid(row=0, column=2)
        self.col_labels[3].grid(row=0, column=3)
        self.left_frame.grid_columnconfigure(0, weight=1)
        self.left_frame.grid_columnconfigure(1, weight=1)
        self.left_frame.grid_columnconfigure(2, weight=1)
        self.left_frame.grid_columnconfigure(3, weight=1)

        self.dio_labels = []
        self.dirs = []
        self.in_states = []
        self.out_states = []
        for index in range(mcc152.info().NUM_DIO_CHANNELS):
            # Labels
            self.dio_labels.append(tkinter.Label(self.left_frame, text="{}".format(index)))
            self.dio_labels[index].grid(row=index+1, column=0)

            self.dirs.append(tkinter.Button(
                self.left_frame, text="Input", width=6,
                command=lambda index=index: self.pressed_dir_button(index)))
            self.dirs[index].grid(row=index+1, column=1)

            self.in_states.append(tkinter.Label(self.left_frame, text="1"))
            self.in_states[index].grid(row=index+1, column=2)

            self.out_states.append(tkinter.Button(
                self.left_frame, text="1",
                command=lambda index=index: self.pressed_output_button(index)))
            self.out_states[index].grid(row=index+1, column=3)

            self.left_frame.grid_rowconfigure(index+1, weight=1)

        self.aout_sliders = []
        for index in range(mcc152.info().NUM_AO_CHANNELS):
            self.aout_sliders.append(tkinter.Scale(
                self.right_frame, from_=5.0, to=0.0, resolution=0.1,
                label="Ch {}".format(index),
                command=lambda value, index=index: self.set_aout_value(index, value)))
            self.aout_sliders[index].grid(row=0, column=index)
            self.aout_sliders[index].grid_configure(sticky="nsew")

        self.right_frame.grid_rowconfigure(0, weight=1)
        self.right_frame.grid_columnconfigure(0, weight=1)
        self.right_frame.grid_columnconfigure(1, weight=1)

        # Disable widgets until a device is opened
        self.disable_controls()

        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup

        icon = tkinter.PhotoImage(file='/usr/share/mcc/daqhats/icon.png')
        # pylint: disable=protected-access
        master.tk.call('wm', 'iconphoto', master._w, icon)

    def disable_controls(self):
        """ Disable controls when device closed """

        # Enable the address selector
        self.device_lister.config(state=tkinter.NORMAL)
        # Disable the board controls
        for child in self.left_frame.winfo_children():
            child.config(state=tkinter.DISABLED)
        for child in self.right_frame.winfo_children():
            child.config(state=tkinter.DISABLED)

    def enable_controls(self):
        """ Enable controls when device opened """

        # Disable the address selector
        self.device_lister.config(state=tkinter.DISABLED)
        # Enable the board controls
        for child in self.left_frame.winfo_children():
            child.config(state=tkinter.NORMAL)
        for child in self.right_frame.winfo_children():
            child.config(state=tkinter.NORMAL)

    def list_devices(self):
        """ List detected devices"""
        self.dev_list = hat_list(HatIDs.MCC_152)
        addr_list = ["{}".format(dev.address) for dev in self.dev_list]
        return addr_list

    def open_device(self, address):
        """ Open selected device """
        try:
            self.board = mcc152(address)
        except HatError:
            return False
        else:
            return True

    def close_device(self):
        """ Close device """
        self.board = None

    def update_inputs(self):
        """ Periodically read the inputs and update the display """
        if self.device_open:
            in_tuple = self.board.dio_input_read_tuple()
            for index in range(mcc152.info().NUM_DIO_CHANNELS):
                if in_tuple[index] == 0:
                    self.in_states[index].config(text='0')
                else:
                    self.in_states[index].config(text='1')

            # schedule another update in 200 ms
            self.master.after(200, self.update_inputs)

    def update_controls_from_device(self):
        """ Display the current device state """
        # Read the current DIO state
        dir_tuple = self.board.dio_config_read_tuple(DIOConfigItem.DIRECTION)
        out_tuple = self.board.dio_output_read_tuple()

        for index in range(mcc152.info().NUM_DIO_CHANNELS):
            if dir_tuple[index] == 0:
                self.dirs[index].config(text='Output', relief=tkinter.SUNKEN)
            else:
                self.dirs[index].config(text='Input', relief=tkinter.RAISED)

            if out_tuple[index] == 0:
                self.out_states[index].config(text='0', relief=tkinter.SUNKEN)
            else:
                self.out_states[index].config(text='1', relief=tkinter.RAISED)

        # Analog output state is unknown so set it to 0.0
        for channel in range(mcc152.info().NUM_AO_CHANNELS):
            self.board.a_out_write(channel, 0.0)
            self.aout_sliders[channel].set(0.0)

    # Event handlers
    def pressed_open_button(self):
        """ Open button event """
        if self.open_button.cget('text') == "Open":
            # Open the selected device
            address = int(self.device_variable.get())
            if self.open_device(address):
                self.device_open = True
                self.open_address = address

                self.enable_controls()
                self.update_controls_from_device()

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

    def pressed_dir_button(self, index):
        """ DIO direction button event """
        if self.dirs[index].cget('text') == 'Input':
            self.board.dio_config_write_bit(index, DIOConfigItem.DIRECTION, 0)
            self.dirs[index].config(text='Output', relief=tkinter.SUNKEN)
        else:
            self.board.dio_config_write_bit(index, DIOConfigItem.DIRECTION, 1)
            self.dirs[index].config(text='Input', relief=tkinter.RAISED)

    def pressed_output_button(self, index):
        """ DIO output button event """
        if self.out_states[index].cget('text') == '0':
            self.board.dio_output_write_bit(index, 1)
            self.out_states[index].config(text='1', relief=tkinter.RAISED)
        else:
            self.board.dio_output_write_bit(index, 0)
            self.out_states[index].config(text='0', relief=tkinter.SUNKEN)

    def set_aout_value(self, index, value):
        """ Analog output value change event """
        self.board.a_out_write(index, float(value))

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
