#!/usr/bin/env python3
"""
    MCC 134 Control Panel 

    Purpose:
        Display the MCC 134 temperatures

    Description:
        This example reads and displays the temperatures
"""
from daqhats import hat_list, mcc134, HatIDs, HatError, TcTypes
from tkinter import *
from tkinter import messagebox
import tkinter.font

class ControlApp:
    
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

        self.BOLD_FONT = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.top_frame = LabelFrame(master, text="Select Device")
        self.top_frame.pack(expand=False, fill=X)

        self.mid_frame = LabelFrame(master, text="TC Types")
        self.mid_frame.pack(expand=False, fill=X)
        
        self.bottom_frame = LabelFrame(master, text="Temperature Inputs")
        self.bottom_frame.pack(expand=True, fill=BOTH)
        
        # Create widgets

        self.dev_label = Label(self.top_frame, text="MCC 134 address:")
        self.dev_label.grid(row=0, column=0)

        self.open_button = Button(self.top_frame, text="Open", width=6, command=self.pressedOpenButton)

        # Get list of MCC 134 devices for the device list widget
        self.addr_list = self.listDevices()

        if len(self.addr_list) == 0:
            self.device_lister = Label(self.top_frame, text="None found")
            self.open_button.config(state=DISABLED)
        else:
            self.device_variable = StringVar(self.top_frame)
            self.device_variable.set(self.addr_list[0])
            self.device_lister = OptionMenu(self.top_frame, self.device_variable, *self.addr_list)
            
        self.device_lister.grid(row=0, column=1)
        self.open_button.grid(row=0, column=2)

        self.channel_labels = []
        self.tc_type_options = []
        self.temperatures = []
        self.tc_type_vars = []
        tc_choices = ("J", "K", "T", "E", "R", "S", "B", "N", "Disabled")
        for index in range(mcc134.info().NUM_AI_CHANNELS):
            # TC types and labels
            label = Label(self.mid_frame, text="Ch {}".format(index))
            label.grid(row=index, column=0)
            
            self.tc_type_vars.append(StringVar(self.mid_frame))
            self.tc_type_vars[index].set(tc_choices[0])
            self.tc_type_options.append(OptionMenu(self.mid_frame, self.tc_type_vars[index], *tc_choices,
                                                   command=lambda value, index=index:self.pressedTC(value, index)))
            self.tc_type_options[index].grid(row=index, column=1)
            self.tc_type_options[index].grid_configure(sticky="E")

            # Labels
            self.channel_labels.append(Label(self.bottom_frame, text="Ch {}".format(index), font=self.BOLD_FONT))
            self.channel_labels[index].grid(row=index, column=1)
            self.channel_labels[index].grid_configure(sticky="W")
            # Temperatures
            self.temperatures.append(Label(self.bottom_frame, text="0.00", font=self.BOLD_FONT))
            self.temperatures[index].grid(row=index, column=3)
            self.temperatures[index].grid_configure(sticky="E")

            self.bottom_frame.grid_rowconfigure(index, weight=1)

        self.mid_frame.grid_columnconfigure(1, weight=1)
        
        self.bottom_frame.grid_columnconfigure(1, weight=1)
        self.bottom_frame.grid_columnconfigure(2, weight=1)

        self.bottom_frame.bind("<Configure>", self.resizeText)

        # Disable widgets until a device is opened
        self.disableControls()
        
        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup

        icon = PhotoImage(file='/usr/share/mcc/daqhats/icon.png')
        master.tk.call('wm', 'iconphoto', master._w, icon)

    def resizeText(self, event):
        new_size = -max(12, int(event.height / 8))
        self.BOLD_FONT.configure(size=new_size)
        
    def pressedTC(self, value, index):
        tc_type = value
        
        if tc_type == 'Disabled':
            self.channel_labels[index].config(state=DISABLED)
            self.temperatures[index].config(state=DISABLED)
        else:
            self.channel_labels[index].config(state=NORMAL)
            self.temperatures[index].config(state=NORMAL)

        # Set the TC type
        my_type = self.tc_types[tc_type]
        self.board.tc_type_write(index, my_type)
        
    def disableControls(self):
        # Enable the address selector
        self.device_lister.config(state=NORMAL)
        # Disable the board controls
        for child in self.mid_frame.winfo_children():
            child.config(state=DISABLED)
            
        for child in self.bottom_frame.winfo_children():
            child.config(state=DISABLED)

    def enableControls(self):
        # Disable the address selector
        self.device_lister.config(state=DISABLED)
        
        # Enable the board controls
        for child in self.mid_frame.winfo_children():
            child.config(state=NORMAL)
            
        for child in self.bottom_frame.winfo_children():
            child.config(state=NORMAL)

    def listDevices(self):
        self.dev_list = hat_list(HatIDs.MCC_134)
        addr_list = ["{}".format(dev.address) for dev in self.dev_list]
        return addr_list

    def openDevice(self, address):
        try:
            self.board = mcc134(address)
            
        except:
            raise
            return False
        else:
            return True

    def closeDevice(self):
        self.board = None
     
    def updateInputs(self):
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
        self.job = self.master.after(1000, self.updateInputs)

    # Event handlers
    def pressedOpenButton(self):
        if self.open_button.cget('text') == "Open":
            # Open the selected device
            address = int(self.device_variable.get())
            if self.openDevice(address):
                self.device_open = True
                self.open_address = address

                self.enableControls()
                
                # read the TC types
                for index in range(mcc134.info().NUM_AI_CHANNELS):
                    tc_type = self.board.tc_type_read(index)
                    self.tc_type_vars[index].set(self.tc_types_reversed[tc_type])
                    if tc_type == TcTypes.DISABLED:
                        self.channel_labels[index].config(state=DISABLED)
                        self.temperatures[index].config(state=DISABLED)
                    else:
                        self.channel_labels[index].config(state=NORMAL)
                        self.temperatures[index].config(state=NORMAL)
                
                # Periodically read the inputs and update controls
                self.updateInputs()
                
                self.open_button.config(text="Close")
            else:
                messagebox.showerror("Error", "Could not open device.")
        else:
            if self.device_open:
                self.closeDevice()
                self.device_open = False
            self.open_button.config(text="Open")
            self.disableControls()

    def close(self):
        # a background read may be in process, so wait for it to complete
        if self.job:
            self.master.after_cancel(self.job)
        self.board = None
        self.master.destroy()

root = Tk()
app = ControlApp(root)
root.mainloop()
