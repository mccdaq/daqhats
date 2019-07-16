#!/usr/bin/env python3
"""
    MCC 152 Control Panel 

    Purpose:
        Control the MCC 152

    Description:
        This program allows viewing MCC 152 input states and controlling outputs.
"""
from daqhats import hat_list, mcc152, HatIDs, HatError, DIOConfigItem
from tkinter import *
from tkinter import messagebox
#import tkMessageBox
import tkinter.font

class ControlApp:
    
    def __init__(self, master):
        self.master = master
        master.title("MCC 152 Control Panel")
    
        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None

        # GUI Setup

        self.BOLD_FONT = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.top_frame = LabelFrame(master, text="Select Device")
        self.top_frame.pack(side=TOP, expand=False, fill=X)

        self.left_frame = LabelFrame(master, text="Digital I/O")
        self.left_frame.pack(side=LEFT, expand=True, fill=BOTH)

        self.right_frame = LabelFrame(master, text="Analog Output")
        self.right_frame.pack(side=RIGHT, fill=Y)

        # Create widgets

        self.dev_label = Label(self.top_frame, text="MCC 152 address:")
        self.dev_label.grid(row=0, column=0)

        self.open_button = Button(self.top_frame, text="Open", width=6, command=self.pressedOpenButton)

        # Get list of MCC 152 devices for the device list widget
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

        self.col_labels = []
        self.col_labels.append(Label(self.left_frame, text="DIO #", font=self.BOLD_FONT, padx=3))
        self.col_labels.append(Label(self.left_frame, text="Direction", font=self.BOLD_FONT, padx=3))
        self.col_labels.append(Label(self.left_frame, text="Input State", font=self.BOLD_FONT, padx=3))
        self.col_labels.append(Label(self.left_frame, text="Output State", font=self.BOLD_FONT, padx=3))
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
            self.dio_labels.append(Label(self.left_frame, text="{}".format(index)))
            self.dio_labels[index].grid(row=index+1, column=0)
            
            self.dirs.append(Button(self.left_frame, text="Input", width=6, command=lambda index=index: self.pressedDirButton(index)))
            self.dirs[index].grid(row=index+1, column=1)

            self.in_states.append(Label(self.left_frame, text="1"))
            self.in_states[index].grid(row=index+1, column=2)

            self.out_states.append(Button(self.left_frame, text="1", command=lambda index=index: self.pressedOutputButton(index)))
            self.out_states[index].grid(row=index+1, column=3)

            self.left_frame.grid_rowconfigure(index+1, weight=1)

        self.aout_sliders = []
        for index in range(mcc152.info().NUM_AO_CHANNELS):
            self.aout_sliders.append(Scale(self.right_frame, from_=5.0, to=0.0, resolution=0.1, label="Ch {}".format(index), command=lambda value, index=index: self.setAoutValue(index, value)))
            self.aout_sliders[index].grid(row=0, column=index)
            self.aout_sliders[index].grid_configure(sticky="nsew")
                
        self.right_frame.grid_rowconfigure(0, weight=1)
        self.right_frame.grid_columnconfigure(0, weight=1)
        self.right_frame.grid_columnconfigure(1, weight=1)

        # Disable widgets until a device is opened
        self.disableControls()
        
        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup
        
        icon = PhotoImage(file='/usr/share/mcc/daqhats/icon.png')
        master.tk.call('wm', 'iconphoto', master._w, icon)

    def disableControls(self):
        # Enable the address selector
        self.device_lister.config(state=NORMAL)
        # Disable the board controls
        for child in self.left_frame.winfo_children():
            child.config(state=DISABLED)
        for child in self.right_frame.winfo_children():
            child.config(state=DISABLED)

    def enableControls(self):
        # Disable the address selector
        self.device_lister.config(state=DISABLED)
        # Enable the board controls
        for child in self.left_frame.winfo_children():
            child.config(state=NORMAL)
        for child in self.right_frame.winfo_children():
            child.config(state=NORMAL)

    def listDevices(self):
        self.dev_list = hat_list(HatIDs.MCC_152)
        addr_list = ["{}".format(dev.address) for dev in self.dev_list]
        return addr_list

    def openDevice(self, address):
        try:
            self.board = mcc152(address)
        except:
            return False
        else:
            return True

    def closeDevice(self):
        self.board = None
        
    def updateInputs(self):
        if self.device_open:
            in_tuple = self.board.dio_input_read_tuple()
            for index in range(mcc152.info().NUM_DIO_CHANNELS):
                if in_tuple[index] == 0:
                    self.in_states[index].config(text='0')
                else:
                    self.in_states[index].config(text='1')
                    
            # schedule another update in 200 ms
            self.master.after(200, self.updateInputs)
                

    def updateControlsFromDevice(self):
        # Read the current DIO state
        dir_tuple = self.board.dio_config_read_tuple(DIOConfigItem.DIRECTION)
        out_tuple = self.board.dio_output_read_tuple()
        
        for index in range(mcc152.info().NUM_DIO_CHANNELS):
            if dir_tuple[index] == 0:
                self.dirs[index].config(text='Output', relief=SUNKEN)
            else:
                self.dirs[index].config(text='Input', relief=RAISED)
                
            if out_tuple[index] == 0:
                self.out_states[index].config(text='0', relief=SUNKEN)
            else:
                self.out_states[index].config(text='1', relief=RAISED)
                
        # Analog output state is unknown so set it to 0.0
        for channel in range(mcc152.info().NUM_AO_CHANNELS):
            self.board.a_out_write(channel, 0.0)
            self.aout_sliders[channel].set(0.0)
        
    # Event handlers
    def pressedOpenButton(self):
        if self.open_button.cget('text') == "Open":
            # Open the selected device
            address = int(self.device_variable.get())
            if self.openDevice(address):
                self.device_open = True
                self.open_address = address

                self.enableControls()
                self.updateControlsFromDevice()
                
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

    def pressedDirButton(self, index):
        if self.dirs[index].cget('text') == 'Input':
            self.board.dio_config_write_bit(index, DIOConfigItem.DIRECTION, 0)
            self.dirs[index].config(text='Output', relief=SUNKEN)
        else:
            self.board.dio_config_write_bit(index, DIOConfigItem.DIRECTION, 1)
            self.dirs[index].config(text='Input', relief=RAISED)

    def pressedOutputButton(self, index):
        if self.out_states[index].cget('text') == '0':
            self.board.dio_output_write_bit(index, 1)
            self.out_states[index].config(text='1', relief=RAISED)
        else:
            self.board.dio_output_write_bit(index, 0)
            self.out_states[index].config(text='0', relief=SUNKEN)
        
    def setAoutValue(self, index, value):
        self.board.a_out_write(index, float(value))
        
    def close(self):
        self.master.destroy()


root = Tk()
app = ControlApp(root)
root.mainloop()
