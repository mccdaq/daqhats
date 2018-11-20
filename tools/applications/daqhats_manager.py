#!/usr/bin/env python3
"""
DAQ HAT manager
"""
from daqhats import hat_list, mcc152, HatIDs, HatError, DIOConfigItem
from tkinter import *
from tkinter import messagebox
from subprocess import call, check_output
import tkinter.font
from mcc118_control_panel import MCC118ControlApp
from mcc152_control_panel import MCC152ControlApp

class MessageDialog(Toplevel):
    def __init__(self, parent, title, message):
        Toplevel.__init__(self, parent)
        self.transient(parent)

        self.title(title)
        self.grab_set()
        self.protocol("WM_DELETE_WINDOW", self.cancel)
        self.resizable(False, False)
        
        titlefont = tkinter.font.Font(
            family=tkinter.font.nametofont("TkCaptionFont")["family"],
            size=tkinter.font.nametofont("TkCaptionFont")["size"],
            weight=tkinter.font.nametofont("TkCaptionFont")["weight"])
        titlewidth = titlefont.measure(title)
        self.minsize(titlewidth + 160, 0)

        msgfont = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight=tkinter.font.nametofont("TkDefaultFont")["weight"])
        msgwidth = msgfont.measure(message)
        
        Message(self, width=msgwidth, text=message).pack(padx=5, pady=5, fill=BOTH)
        
        b = Button(self, text="OK", command=self.ok)
        b.pack(pady=5)
        
        self.geometry("+%d+%d" % (parent.winfo_rootx() + 20,
                                  parent.winfo_rooty() + 20))
        
        self.wait_window(self)
        
    def ok(self):
        self.withdraw()
        self.update_idletasks()
        #self.apply()
        self.cancel()
        
    def cancel(self, event=None):
        self.master.focus_set()
        self.destroy()
        
class ControlApp:
    
    def __init__(self, master):
        self.master = master
        title = "DAQ HAT Manager"
        master.title(title)
    
        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None
        self.app_list = []

        # GUI Setup
        titlefont = tkinter.font.Font(
            family=tkinter.font.nametofont("TkCaptionFont")["family"],
            size=tkinter.font.nametofont("TkCaptionFont")["size"],
            weight=tkinter.font.nametofont("TkCaptionFont")["weight"])
        titlewidth = titlefont.measure(title)
        master.minsize(titlewidth + 160, 0)

        master.geometry("+0+0")
        master.resizable(False, False)
        
        img = Image("photo", file="/usr/share/mcc/daqhats/icon.png")
        master.tk.call('wm', 'iconphoto', master._w, img)
        
        # Create and organize frames
        self.main_frame = LabelFrame(master, text="Manage devices")
        self.main_frame.pack(side=LEFT, fill=BOTH, expand=1)
        
        self.device_frame = LabelFrame(master, text="Control Apps")
        self.device_frame.pack(side=RIGHT, fill=BOTH, expand=1)

        # Create widgets

        self.list_devices_button = Button(self.main_frame, text="List devices", command=self.pressedListButton)
        self.read_eeprom_button = Button(self.main_frame, text="Read EEPROMs", command=self.pressedReadButton)
   
        self.list_devices_button.pack(fill=X)
        self.read_eeprom_button.pack(fill=X)
        
        self.mcc118_button = Button(self.device_frame, text="MCC 118 App", command=self.pressed118Button)
        self.mcc152_button = Button(self.device_frame, text="MCC 152 App", command=self.pressed152Button)

        self.mcc118_button.pack(fill=X)
        self.mcc152_button.pack(fill=X)
   
        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup
        
    # Event handlers
    def pressedReadButton(self):
        try:
            result = check_output(['gksudo', 'daqhats_read_eeproms'])
        except:
            d = MessageDialog(self.master, 'Read EEPROMs', 'Error reading EEPROMs')
        else:
            d = MessageDialog(self.master, 'Read EEPROMs', result)
        
    def pressedListButton(self):
        try:
            result = check_output('daqhats_list_boards')
        except:
            d = MessageDialog(self.master, 'List Devices', 'No boards found')
        else:
            d = MessageDialog(self.master, 'List Devices', result)

    def pressed118Button(self):
        t = Toplevel(self.master)
        t.group(self.master)
        t.geometry("+%d+%d" % (self.master.winfo_rootx(),
                               self.master.winfo_rooty() + self.master.winfo_height()))
        
        app = MCC118ControlApp(t)
        
    def pressed152Button(self):
        t = Toplevel(self.master)
        t.group(self.master)
        t.geometry("+%d+%d" % (self.master.winfo_rootx(),
                               self.master.winfo_rooty() + self.master.winfo_height()))

        app = MCC152ControlApp(t)

    def close(self):
        for app in self.app_list:
            app.terminate()
        self.master.destroy()


root = Tk(className="daqhatApp")
app = ControlApp(root)
root.mainloop()
