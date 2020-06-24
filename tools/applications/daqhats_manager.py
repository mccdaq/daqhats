#!/usr/bin/env python3
"""
DAQ HAT manager
"""
import tkinter
import tkinter.font
from subprocess import check_output, Popen

class MessageDialog(tkinter.Toplevel):
    """ Message dialog box class """
    def __init__(self, parent, title, message):
        tkinter.Toplevel.__init__(self, parent)
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
        tkinter.Message(self, width=maxwidth, text=message).pack(padx=5, pady=5, fill=tkinter.BOTH)

        button = tkinter.Button(self, text="OK", command=self.ok_pressed)
        button.pack(pady=5)

        self.geometry("+%d+%d" % (parent.winfo_rootx() + 20,
                                  parent.winfo_rooty() + 20))

        self.wait_window(self)

    def ok_pressed(self):
        """ Ok button handler """
        self.withdraw()
        self.update_idletasks()
        self.cancel()

    def cancel(self, _event=None):
        """ Cancel handler """
        self.master.focus_set()
        self.destroy()

class ControlApp:
    """ Application class """
    # pylint: disable=too-many-instance-attributes

    def __init__(self, master):
        self.master = master
        master.title("MCC DAQ HAT Manager")

        # Initialize variables
        self.device_open = False
        self.open_address = 0
        self.board = None
        self.app_list = []

        # GUI Setup

        self.bold_font = tkinter.font.Font(
            family=tkinter.font.nametofont("TkDefaultFont")["family"],
            size=tkinter.font.nametofont("TkDefaultFont")["size"],
            weight="bold")

        # Create and organize frames
        self.main_frame = tkinter.LabelFrame(master, text="Manage devices")
        self.main_frame.pack(side=tkinter.LEFT)

        self.device_frame = tkinter.LabelFrame(master, text="Control Apps")
        self.device_frame.pack(side=tkinter.RIGHT)


        # Create widgets

        self.list_devices_button = tkinter.Button(
            self.main_frame, text="List devices", command=self.pressed_list_button)
        self.read_eeprom_button = tkinter.Button(
            self.main_frame, text="Read EEPROMs", command=self.pressed_read_button)

        self.list_devices_button.pack(fill=tkinter.X)
        self.read_eeprom_button.pack(fill=tkinter.X)

        self.mcc118_button = tkinter.Button(
            self.device_frame, text="MCC 118 App", command=self.pressed_118_button)
        self.mcc128_button = tkinter.Button(
            self.device_frame, text="MCC 128 App", command=self.pressed_128_button)
        self.mcc134_button = tkinter.Button(
            self.device_frame, text="MCC 134 App", command=self.pressed_134_button)
        self.mcc152_button = tkinter.Button(
            self.device_frame, text="MCC 152 App", command=self.pressed_152_button)
        self.mcc172_button = tkinter.Button(
            self.device_frame, text="MCC 172 App", command=self.pressed_172_button)

        self.mcc118_button.pack(fill=tkinter.X)
        self.mcc128_button.pack(fill=tkinter.X)
        self.mcc134_button.pack(fill=tkinter.X)
        self.mcc152_button.pack(fill=tkinter.X)
        self.mcc172_button.pack(fill=tkinter.X)

        master.protocol('WM_DELETE_WINDOW', self.close) # exit cleanup

        icon = tkinter.PhotoImage(file='/usr/share/mcc/daqhats/icon.png')
        # pylint: disable=protected-access
        master.tk.call('wm', 'iconphoto', master._w, icon)


    # Event handlers
    def pressed_read_button(self):
        """ Read button handler """
        try:
            result = check_output(['pkexec', 'daqhats_read_eeproms'])
        except FileNotFoundError:
            result = check_output(['gksudo', 'daqhats_read_eeproms'])
        _dlg = MessageDialog(self.master, 'Read EEPROMs', result)

    def pressed_list_button(self):
        """ List button handler """
        result = check_output('daqhats_list_boards')
        _dlg = MessageDialog(self.master, 'List Devices', result)

    def pressed_118_button(self):
        """ MCC 118 button handler """
        result = Popen('/usr/share/mcc/daqhats/mcc118_control_panel.py')
        self.app_list.append(result)

    def pressed_128_button(self):
        """ MCC 128 button handler """
        result = Popen('/usr/share/mcc/daqhats/mcc128_control_panel.py')
        self.app_list.append(result)

    def pressed_134_button(self):
        """ MCC 134 button handler """
        result = Popen('/usr/share/mcc/daqhats/mcc134_control_panel.py')
        self.app_list.append(result)

    def pressed_152_button(self):
        """ MCC 152 button handler """
        result = Popen('/usr/share/mcc/daqhats/mcc152_control_panel.py')
        self.app_list.append(result)

    def pressed_172_button(self):
        """ MCC 172 button handler """
        result = Popen('/usr/share/mcc/daqhats/mcc172_control_panel.py')
        self.app_list.append(result)

    def close(self):
        """ App close handler """
        for app in self.app_list:
            app.terminate()
        self.master.destroy()

def main():
    """ App entry point """
    root = tkinter.Tk()
    root.geometry("+0+0")
    _app = ControlApp(root)
    root.mainloop()

if __name__ == '__main__':
    main()
