"""
MCC DAQ HATs module.
"""
from daqhats.hats import HatError, hat_list, HatIDs, TriggerModes, \
    OptionFlags, wait_for_interrupt, interrupt_state, \
    interrupt_callback_enable, interrupt_callback_disable, HatCallback
from daqhats.mcc118 import mcc118
from daqhats.mcc128 import mcc128, AnalogInputMode, AnalogInputRange
from daqhats.mcc152 import mcc152, DIOConfigItem
from daqhats.mcc134 import mcc134, TcTypes
from daqhats.mcc172 import mcc172, SourceType
