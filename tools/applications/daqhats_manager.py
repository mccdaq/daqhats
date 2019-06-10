#!/usr/bin/env python3
"""
DAQ HAT manager
"""
from tkinter import *
from tkinter import messagebox
from subprocess import call, check_output, Popen
import tkinter.font

class MessageDialog(Toplevel):
    def __init__(self, parent, title, message):
        Toplevel.__init__(self, parent)
        self.transient(parent)
        #top = self.top = Toplevel(parent)
        self.title(title)
        self.grab_set()
        self.protocol("WM_DELETE_WINDOW", self.cancel)
        
        myfont = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight=tkinter.font.nametofont("TkDefaultFont")["weight"])
        maxwidth = myfont.measure(message)
        Message(self, width=maxwidth, text=message).pack(padx=5, pady=5, fill=BOTH)
        
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
        master.title("MCC DAQ HAT Manager")
    
        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None
        self.app_list = []

        # GUI Setup

        self.BOLD_FONT = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.main_frame = LabelFrame(master, text="Manage devices")
        self.main_frame.pack(side=LEFT)
        
        self.device_frame = LabelFrame(master, text="Control Apps")
        self.device_frame.pack(side=RIGHT)


        # Create widgets

        self.list_devices_button = Button(self.main_frame, text="List devices", command=self.pressedListButton)
        self.read_eeprom_button = Button(self.main_frame, text="Read EEPROMs", command=self.pressedReadButton)
   
        self.list_devices_button.pack(fill=X)
        self.read_eeprom_button.pack(fill=X)
        
        self.mcc118_button = Button(self.device_frame, text="MCC 118 App", command=self.pressed118Button)
        self.mcc134_button = Button(self.device_frame, text="MCC 134 App", command=self.pressed134Button)
        self.mcc152_button = Button(self.device_frame, text="MCC 152 App", command=self.pressed152Button)

        self.mcc118_button.pack(fill=X)
        self.mcc134_button.pack(fill=X)
        self.mcc152_button.pack(fill=X)
   
        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup
        

        
    # Event handlers
    def pressedReadButton(self):
        result = check_output(['gksudo', 'daqhats_read_eeproms'])
        d = MessageDialog(self.master, 'Read EEPROMs', result)
        
    def pressedListButton(self):
        result = check_output('daqhats_list_boards')
        d = MessageDialog(self.master, 'List Devices', result)

    def pressed118Button(self):
        result = Popen('/usr/share/mcc/daqhats/mcc118_control_panel.py')
        self.app_list.append(result)
        
    def pressed134Button(self):
        result = Popen('/usr/share/mcc/daqhats/mcc134_control_panel.py')
        self.app_list.append(result)
        
    def pressed152Button(self):
        result = Popen('/usr/share/mcc/daqhats/mcc152_control_panel.py')
        self.app_list.append(result)

    def close(self):
        for app in self.app_list:
            app.terminate()
        self.master.destroy()


root = Tk()
root.geometry("+0+0")
app = ControlApp(root)
root.mainloop()
