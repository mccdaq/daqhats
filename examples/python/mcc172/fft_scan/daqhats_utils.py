"""
    This file contains helper functions for the MCC DAQ HAT Python examples.
"""
from __future__ import print_function
from daqhats import hat_list, HatError


def select_hat_device(filter_by_id):
    # type: (HatIDs) -> int
    """
    This function performs a query of available DAQ HAT devices and determines
    the address of a single DAQ HAT device to be used in an example.  If a
    single HAT device is present, the address for that device is automatically
    selected, otherwise the user is prompted to select an address from a list
    of displayed devices.

    Args:
        filter_by_id (int): If this is :py:const:`HatIDs.ANY` return all DAQ
            HATs found.  Otherwise, return only DAQ HATs with ID matching this
            value.

    Returns:
        int: The address of the selected device.

    Raises:
        Exception: No HAT devices are found or an invalid address was selected.

    """
    selected_hat_address = None

    # Get descriptors for all of the available HAT devices.
    hats = hat_list(filter_by_id=filter_by_id)
    number_of_hats = len(hats)

    # Verify at least one HAT device is detected.
    if number_of_hats < 1:
        raise HatError(0, 'Error: No HAT devices found')
    elif number_of_hats == 1:
        selected_hat_address = hats[0].address
    else:
        # Display available HAT devices for selection.
        for hat in hats:
            print('Address ', hat.address, ': ', hat.product_name, sep='')
        print('')

        address = int(input('Select the address of the HAT device to use: '))

        # Verify the selected address if valid.
        for hat in hats:
            if address == hat.address:
                selected_hat_address = address
                break

    if selected_hat_address is None:
        raise ValueError('Error: Invalid HAT selection')

    return selected_hat_address


def enum_mask_to_string(enum_type, bit_mask):
    # type: (Enum, int) -> str
    """
    This function converts a mask of values defined by an IntEnum class to a
    comma separated string of names corresponding to the IntEnum names of the
    values included in a bit mask.

    Args:
        enum_type (Enum): The IntEnum class from which the mask was created.
        bit_mask (int): A bit mask of values defined by the enum_type class.

    Returns:
        str: A comma separated string of names corresponding to the IntEnum
        names of the values included in the mask

    """
    item_names = []
    if bit_mask == 0:
        item_names.append('DEFAULT')
    for item in enum_type:
        if item & bit_mask:
            item_names.append(item.name)
    return ', '.join(item_names)


def chan_list_to_mask(chan_list):
    # type: (list[int]) -> int
    """
    This function returns an integer representing a channel mask to be used
    with the MCC daqhats library with all bit positions defined in the
    provided list of channels to a logic 1 and all other bit positions set
    to a logic 0.

    Args:
        chan_list (int): A list of channel numbers.

    Returns:
        int: A channel mask of all channels defined in chan_list.

    """
    chan_mask = 0

    for chan in chan_list:
        chan_mask |= 0x01 << chan

    return chan_mask


def validate_channels(channel_set, number_of_channels):
    # type: (set, int) -> None
    """
    Raises a ValueError exception if a channel number in the set of
    channels is not in the range of available channels.

    Args:
        channel_set (set): A set of channel numbers.
        number_of_channels (int): The number of available channels.

    Returns:
        None

    Raises:
        ValueError: If there is an invalid channel specified.

    """
    valid_chans = range(number_of_channels)
    if not channel_set.issubset(valid_chans):
        raise ValueError('Error: Invalid channel selected - must be '
                         '{} - {}'.format(min(valid_chans), max(valid_chans)))
