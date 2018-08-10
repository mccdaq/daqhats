"""
Wraps the global methods from the MCC Hat library for use in Python.
"""
from collections import namedtuple
from ctypes import cdll, Structure, c_ubyte, c_ushort, c_char, c_int, POINTER
from enum import IntEnum

class HatIDs(IntEnum):
    """Known MCC HAT IDs."""
    ANY = 0             #: Match any MCC ID in :py:func:`hat_list`
    MCC_118 = 0x0142    #: MCC 118 ID

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

# exception classes
class HatError(Exception):
    """
    Exceptions raised for MCC HAT specific errors.

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

class _Info(Structure): # pylint: disable=too-few-public-methods
    _fields_ = [("address", c_ubyte),
                ("id", c_ushort),
                ("version", c_ushort),
                ("product_name", c_char * 256)]

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
