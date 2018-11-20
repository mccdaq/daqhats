"""
Wraps the global methods from the MCC Hat library for use in Python.
"""
from collections import namedtuple
from ctypes import cdll, Structure, c_ubyte, c_ushort, c_char, c_int, POINTER, \
    CFUNCTYPE, cast, py_object, c_void_p, pointer
from enum import IntEnum

class HatIDs(IntEnum):
    """Known MCC HAT IDs."""
    ANY = 0             #: Match any MCC ID in :py:func:`hat_list`
    MCC_118 = 0x0142    #: MCC 118 ID
    MCC_152 = 0x0144    #: MCC 152 ID

class TriggerModes(IntEnum):
    """Scan trigger input modes."""
    RISING_EDGE = 0     #: Start the scan on a rising edge of TRIG.
    FALLING_EDGE = 1    #: Start the scan on a falling edge of TRIG.
    ACTIVE_HIGH = 2     #: Start the scan any time TRIG is high.
    ACTIVE_LOW = 3      #: Start the scan any time TRIG is low.

class OptionFlags(IntEnum):
    """Scan / read option flags. See individual methods for detailed
    descriptions."""
    DEFAULT = 0x0000         #: Use default behavior.
    NOSCALEDATA = 0x0001     #: Read / write unscaled data.
    NOCALIBRATEDATA = 0x0002 #: Read / write uncalibrated data.
    EXTCLOCK = 0x0004        #: Use an external clock source.
    EXTTRIGGER = 0x0008      #: Use an external trigger source.
    CONTINUOUS = 0x0010      #: Run until explicitly stopped.

# exception class
class HatError(Exception):
    """
    Exceptions raised for MCC DAQ HAT specific errors.

    Args:
        address (int): the address of the board that caused the exception.
        value (str): the exception description.
    """
    def __init__(self, address, value):
        super(HatError, self).__init__(value)
        self.address = address
        self.value = value
    def __str__(self):
        return "Addr {}: ".format(self.address) + self.value

# HAT info structure class
class _Info(Structure): # pylint: disable=too-few-public-methods
    _fields_ = [("address", c_ubyte),
                ("id", c_ushort),
                ("version", c_ushort),
                ("product_name", c_char * 256)]

# Callback function class
class HatCallback(object):
    """
    DAQ HAT interrupt callback function class.

    This class handles passing Python functions to the shared library as a
    callback then retrieving the passed user data as a Python object when
    the function is called from the library.

    The callback function should have a single argument (optional) that is a
    Python object the user provides to :py:func:`interrupt_callback_enable` that
    will be passed to the callback function whenever it is called.

    Args:
        function (function): the function to be called when an interrupt occurs.
    """
    def __init__(self, function):
        """
        Store the function and create the CFUNCTYPE.
        """
        if not callable(function):
            raise TypeError("Argument 1 must be a function or method.")
        self.function = function
        self.cbfunctype = CFUNCTYPE(None, c_void_p)
        self.cbfunc = None

    def get_callback_func(self):
        """
        Create a wrapper function without the self argument since that can't
        get passed to the library function, and assign it to a variable to
        avoid it getting garbage collected.
        """
        def func(user_data):
            """
            Function wrapper.
            """
            self.handle_callback(user_data)
        self.cbfunc = self.cbfunctype(func)
        return self.cbfunc

    def handle_callback(self, user_data):
        """
        This is directly called from the interrupt thread. It converts the
        void* back into a python object then calls the user's callback.
        """
        p_user_data = cast(user_data, POINTER(py_object)).contents.value
        self.function(p_user_data)

def _load_daqhats_library():
    """
    Load the library
    """
    libname = 'libdaqhats.so.1'
    try:
        lib = cdll.LoadLibrary(libname)
    except: # pylint: disable=bare-except
        lib = 0
    return lib

def hat_list(filter_by_id=0):
    """
    Return a list of detected DAQ HAT boards.

    Scans certain locations for information from the HAT EEPROMs.  Verifies the
    contents are valid HAT EEPROM contents and returns a list of namedtuples
    containing information on the HAT.  Info will only be returned for DAQ HATs.
    The EEPROM contents are stored in /etc/mcc/hats when using the
    daqhats_read_eeproms tool, or in /proc/device-tree in the case of a single
    HAT at address 0.

    Args:
        filter_by_id (int): If this is :py:const:`HatIDs.ANY` return all DAQ
            HATs found.  Otherwise, return only DAQ HATs with ID matching this
            value.

    Returns:
        list: A list of namedtuples, the number of elements match the number of
        DAQ HATs found. Each namedtuple will contain the following field names

        * **address** (int): device address
        * **id** (int): device product ID, identifies the type of DAQ HAT
        * **version** (int): device hardware version
        * **product_name** (str): device product name
    """
    _libc = _load_daqhats_library()
    if _libc == 0:
        return []

    _libc.hat_list.argtypes = [c_ushort, POINTER(_Info)]
    _libc.hat_list.restype = c_int

    # find out how many structs we need
    count = _libc.hat_list(filter_by_id, None)
    if count == 0:
        return []

    # allocate array of Info structs
    my_info = (_Info * count)()

    # get the info
    count = _libc.hat_list(filter_by_id, my_info)

    # create the list of dictionaries to return
    my_list = []
    hat_info = namedtuple('HatInfo',
                          ['address', 'id', 'version', 'product_name'])
    for item in my_info:
        info = hat_info(
            address=item.address,
            id=item.id,
            version=item.version,
            product_name=item.product_name.decode('ascii'))
        my_list.append(info)

    return my_list

def interrupt_state():
    """
    Read the current DAQ HAT interrupt status

    Returns the status of the interrupt signal, True if active or False if
    inactive. The signal can be shared by multiple DAQ HATs so the status of
    each board that may generate an interrupt must be read and the interrupt
    source(s) cleared before the interrupt will become inactive.

    This function only applies when using devices that can generate an
    interrupt:
    
    * MCC 152
        
    Returns:
        bool: The interrupt status.
    """
    _libc = _load_daqhats_library()
    if _libc == 0:
        return []

    _libc.hat_interrupt_state.argtypes = []
    _libc.hat_interrupt_state.restype = c_int

    # get the info
    state = _libc.hat_interrupt_state()

    return state == 1

def wait_for_interrupt(timeout):
    """
    Wait for an interrupt from a DAQ HAT to occur.

    Pass a timeout in seconds. Pass -1 to wait forever or 0 to return
    immediately. If the interrupt has not occurred before the timeout elapses
    the function will return 0.

    This function only applies when using devices that can generate an
    interrupt:
    
    * MCC 152

    Returns:
        bool: The interrupt status - True = interrupt active, False = interrupt
        inactive.
    """
    _libc = _load_daqhats_library()
    if _libc == 0:
        return []

    _libc.hat_wait_for_interrupt.argtypes = [c_int]
    _libc.hat_wait_for_interrupt.restype = c_int

    if timeout == -1:
        timeout_ms = -1
    elif timeout == 0:
        timeout_ms = 0
    else:
        timeout_ms = timeout * 1000

    state = _libc.hat_wait_for_interrupt(timeout_ms)
    return state == 1

def interrupt_callback_enable(callback, user_data):
    """
    Enable an interrupt callback function.

    Set a function that will be called when a DAQ HAT interrupt occurs.

    The function will be called when the DAQ HAT interrupt signal becomes
    active, and cannot be called again until the interrupt signal becomes
    inactive. Active sources become inactive when manually cleared (such as
    reading the digital I/O inputs or clearing the interrupt enable.) If not
    latched, an active source also becomes inactive when the value returns to
    the original value (the value at the source before the interrupt was
    generated.)

    There may only be one callback function at a time; if you call this
    when a function is already set as the callback function then it will be
    replaced with the new function and the old function will no longer be called
    if an interrupt occurs. The data argument to this function will be passed
    to the callback function when it is called.

    The callback function must be encapsulated in a :py:class:`HatCallback`
    class. For example: ::

        def my_function(data):
            # This is my callback function.
            print("The interrupt occurred, and returned {}.".format(data))
            data[0] += 1

        value = [0]
        callback = HatCallback(my_function)
        interrupt_enable_callback(callback, value)

    In this example *my_function()* will be called when the interrupt occurs,
    and the list *value* will be passed as the user_data. Inside the callback it
    will be received as *data*, but will still be the same object so any changes
    made will be present in the original *value*. Every time the interrupt
    occurs *value[0]* will be incremented and a higher number will be printed.

    An integer was not used for *value* because integers are immutable in Python
    so the original *value* would never change.

    The callback may be disabled with :py:func:`interrupt_callback_disable`.

    This function only applies when using devices that can generate an
    interrupt:
    
    * MCC 152

    Args:
        callback (:py:class:`HatCallback`): The callback function encapsulated
            in a :py:class:`HatCallback` class.
        user_data (object)      Optional Python object or data to pass to the
            callback function.

    Raises:
        Exception: Internal error enabling the callback.
    """
    _libc = _load_daqhats_library()
    if _libc == 0:
        return []

    # callback must be an instance of HatCallback
    if not isinstance(callback, HatCallback):
        raise TypeError("Argument 1 must be an instance of HatCallback")

    # function argtype is provided by callback class
    _libc.hat_interrupt_callback_enable.argtypes = [callback.cbfunctype,
                                                    c_void_p]
    _libc.hat_interrupt_callback_enable.restype = c_int

    # cast the python user_data object to a void*
    c_user_data = cast(pointer(py_object(user_data)), c_void_p)
    # pass the callback class handler function and void * to the library
    if (_libc.hat_interrupt_callback_enable(callback.get_callback_func(),
                                            c_user_data) != 0):
        raise Exception("Could not enable callback function.")

def interrupt_callback_disable():
    """
    Disable interrupt callbacks.

    Raises:
        Exception: Internal error disabling the callback.
    """
    _libc = _load_daqhats_library()
    if _libc == 0:
        return []

    _libc.hat_interrupt_callback_disable.argtypes = []
    _libc.hat_interrupt_callback_disable.restype = c_int

    if _libc.hat_interrupt_callback_disable() != 0:
        raise Exception("Could not disabled callback function.")

class Hat(object): # pylint: disable=too-few-public-methods
    """
    DAQ HAT base class.

    Args:
        address (int): board address, must be 0-7.

    Raises:
        ValueError: the address is invalid.
    """
    _RESULT_SUCCESS = 0
    _RESULT_BAD_PARAMETER = -1
    _RESULT_BUSY = -2
    _RESULT_TIMEOUT = -3
    _RESULT_LOCK_TIMEOUT = -4
    _RESULT_INVALID_DEVICE = -5
    _RESULT_RESOURCE_UNAVAIL = -6
    _RESULT_COMMS_FAILURE = -7
    _RESULT_UNDEFINED = -10

    def __init__(self, address=0):
        """Initialize the class.  Address must be 0-7."""
        self._initialized = False
        self._address = 0

        # max address value is 7
        if address in range(8):
            self._address = address
        else:
            raise ValueError("Invalid address {}. Must be 0-7.".format(address))

        self._lib = _load_daqhats_library()
        if self._lib == 0:
            raise Exception("daqhats shared library is not installed.")

        self._initialized = True
        return

    def address(self):
        """Return the device address."""
        return self._address
