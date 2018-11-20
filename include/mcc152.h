/**
*   @file mcc152.h
*   @author Measurement Computing Corp.
*   @brief This file contains definitions for the MCC 152.
*
*   06/29/2018
*/
#ifndef _MCC_152_H
#define _MCC_152_H

#include <stdint.h>

/// MCC 152 constant device information.
struct MCC152DeviceInfo
{
    /// The number of digital I/O channels (8.)
    const uint8_t NUM_DIO_CHANNELS;
    /// The number of analog output channels (2.)
    const uint8_t NUM_AO_CHANNELS;
    /// The minimum DAC code (0.)
    const uint16_t AO_MIN_CODE;
    /// The maximum DAC code (4095.)
    const uint16_t AO_MAX_CODE;
    /// The output voltage corresponding to the minimum code (0.0V.)
    const double AO_MIN_VOLTAGE;
    /// The output voltage corresponding to the maximum code (+5.0V - 1 LSB.)
    const double AO_MAX_VOLTAGE;
    /// The minimum voltage of the output range (0.0V.)
    const double AO_MIN_RANGE;
    /// The maximum voltage of the output range (+5.0V.)
    const double AO_MAX_RANGE;
};

/// DIO Configuration Items
enum DIOConfigItem
{
    /// Configure channel direction
    DIO_DIRECTION       = 0,
    /// Configure pull-up/down resistor
    DIO_PULL_CONFIG     = 1,
    /// Enable pull-up/down resistor
    DIO_PULL_ENABLE     = 2,
    /// Configure input inversion
    DIO_INPUT_INVERT    = 3,
    /// Configure input latching
    DIO_INPUT_LATCH     = 4,
    /// Configure output type
    DIO_OUTPUT_TYPE     = 5,
    /// Configure interrupt mask
    DIO_INT_MASK        = 6
};

#ifdef __cplusplus
extern "C" {
#endif

/**
*   @brief Open a connection to the MCC 152 device at the specified address.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode), 
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_open(uint8_t address);

/**
*   @brief Check if an MCC 152 is open.
*
*   @param address  The board address (0 - 7).
*   @return 1 if open, 0 if not open.
*/
int mcc152_is_open(uint8_t address);

/**
*   @brief Close a connection to an MCC 152 device and free allocated resources.
*
*   @param address  The board address (0 - 7).
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_close(uint8_t address);

/**
*   @brief Return constant device information for all MCC 152s.
*
*   @return Pointer to struct MCC152DeviceInfo.
*       
*/
struct MCC152DeviceInfo* mcc152_info(void);

/**
*   @brief Read the MCC 152 serial number
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param buffer   Pass a user-allocated buffer pointer to receive the serial
*       number as a string. The buffer must be at least 9 characters in length.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_serial(uint8_t address, char* buffer);

/**
*   @brief Perform a write to an analog output channel.
*
*   Updates the analog output channel in either volts or DAC code (set the
*   [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA) option to use DAC code.) The
*   voltage must be 0.0 - 5.0 and DAC code 0.0 - 4095.0.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The analog output channel number, 0 - 1.
*   @param options  Options bitmask
*   @param value    The analog output value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_a_out_write(uint8_t address, uint8_t channel, uint32_t options,
    double value);

/**
*   @brief Perform a write to all analog output channels simultaneously.
*
*   Update all analog output channels in either volts or DAC code (set the
*   [OPTS_NOSCALEDATA](@ref OPTS_NOSCALEDATA) option to use DAC code.) The
*   outputs will update at the same time.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param options  Options bitmask
*   @param values   The array of analog output values; there must be at least
*       2 values, but only the first two values will be used.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_a_out_write_all(uint8_t address, uint32_t options, double* values);

/**
*   @brief Reset the digital I/O to the default configuration.
*
*   - All channels input
*   - Output registers set to 1
*   - Input inversion disabled
*   - No input latching
*   - Pull-up resistors enabled
*   - All interrupts disabled
*   - Push-pull output type
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_reset(uint8_t address);

/**
*   @brief Read a single digital input channel.
*
*   Returns 0 or 1 in \b value. If the specified channel is configured as an
*   output this will return the value present at the terminal.
*
*   This function reads the entire input register, so care must be taken when
*   latched inputs are enabled. If a latched input changes between input reads
*   then changes back to its original value, the next input read will report the
*   change to the first value then the following read will show the original
*   value. If another input is read then this input change could be missed so it
*   is best to use mcc152_dio_input_read_port() when using latched inputs.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The DIO channel number, 0 - 7.
*   @param value    Receives the input value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_input_read_bit(uint8_t address, uint8_t channel, uint8_t* value);

/**
*   @brief Read all digital input channels simultaneously.
*
*   Returns an 8-bit value in \b value representing all channels in channel
*   order (bit 0 is channel 0, etc.) If a channel is configured as an output
*   this will return the value present at the terminal.
*
*   Care must be taken when latched inputs are enabled. If a latched input
*   changes between input reads then changes back to its original value, the
*   next input read will report the change to the first value then the following
*   read will show the original value.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param value    Receives the input values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_input_read_port(uint8_t address, uint8_t* value);

/**
*   @brief Write a single digital output channel.
*
*   If the specified channel is configured as an input this will not have any
*   effect at the terminal, but allows the output register to be loaded before
*   configuring the channel as an output.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The DIO channel number, 0 - 7.
*   @param value    The output value (0 or 1)
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_output_write_bit(uint8_t address, uint8_t channel,
    uint8_t value);

/**
*   @brief Write all digital output channels simultaneously.
*
*   Pass an 8-bit value in \b value representing the desired output for all
*   channels in channel order (bit 0 is channel 0, etc.)
*
*   If the specified channel is configured as an input this will not have any
*   effect at the terminal, but allows the output register to be loaded before
*   configuring the channel as an output.
*
*   For example, to set channels 0 - 3 to 0 and channels 4 - 7 to 1 call:
*
*       mcc152_dio_output_write(address, 0xF0);
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param value    The output values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_output_write_port(uint8_t address, uint8_t value);

/**
*   @brief Read a single digital output register.
*
*   Returns 0 or 1 in \b value.
*
*   This function returns the value stored in the output register. It may not
*   represent the value at the terminal if the channel is configured as input or
*   open-drain output.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The DIO channel number, 0 - 7.
*   @param value    Receives the output value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_output_read_bit(uint8_t address, uint8_t channel,
    uint8_t* value);

/**
*   @brief Read all digital output registers simultaneously.
*
*   Returns an 8-bit value in \b value representing all channels in channel
*   order (bit 0 is channel 0, etc.)
*
*   This function returns the value stored in the output register. It may not
*   represent the value at the terminal if the channel is configured as input or
*   open-drain output.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param value    Receives the output values.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_output_read_port(uint8_t address, uint8_t* value);

/**
*   @brief Read the interrupt status for a single channel.
*
*   Returns 0 when the channel is not generating an interrupt, 1 when the
*   channel is generating an interrupt.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The DIO channel number, 0 - 7.
*   @param value    Receives the interrupt status value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_int_status_read_bit(uint8_t address, uint8_t channel,
    uint8_t* value);

/**
*   @brief Read the interrupt status for all channels.
*
*   Returns an 8-bit value in \b value representing all channels in channel
*   order (bit 0 is channel 0, etc.) A 0 in a bit indicates the corresponding
*   channel is not generating an interrupt, a 1 indicates the channel is
*   generating an interrupt.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param value    Receives the interrupt status value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_int_status_read_port(uint8_t address, uint8_t* value);

/**
*   @brief Write a digital I/O configuration value for a single channel.
*
*   There are several configuration items that may be written for the digital
*   I/O. The item is selected with the \b item argument, which may be one of
*   the [DIOConfigItem](@ref DIOConfigItem) values:
*   - [DIO_DIRECTION](@ref DIO_DIRECTION): Set the digital I/O channel direction
*       by passing 0 for output and 1 for input.
*   - [DIO_PULL_CONFIG](@ref DIO_PULL_CONFIG): Configure the pull-up/down
*       resistor by passing 0 for pull-down or 1 for pull-up. The resistor may
*       be enabled or disabled with the [DIO_PULL_ENABLE](@ref DIO_PULL_ENABLE)
*       item.
*   - [DIO_PULL_ENABLE](@ref DIO_PULL_ENABLE): Enable or disable the
*       pull-up/down resistor by passing 0 for disabled or 1 for enabled. The
*       resistor is configured for pull-up/down with the
*       [DIO_PULL_CONFIG](@ref DIO_PULL_CONFIG) item. The resistor is
*       automatically disabled if the bit is set to output and is configured as
*       open-drain.
*   - [DIO_INPUT_INVERT](@ref DIO_INPUT_INVERT): Enable inverting the input by
*       passing a 0 for normal input or 1 for inverted.
*   - [DIO_INPUT_LATCH](@ref DIO_INPUT_LATCH): Enable input latching by passing
*       0 for non-latched or 1 for latched.
*
*       When the input is non-latched, reads show the current status of the
*       input. A state change in the input generates an interrupt (if it is not
*       masked). A read of the input clears the interrupt. If the input goes
*       back to its initial logic state before the input is read, then the
*       interrupt is cleared. 
*
*       When the input is latched, a change of state of the input generates an
*       interrupt and the input logic value is loaded into the input port
*       register. A read of the input will clear the interrupt. If the input
*       returns to its initial logic state before the input is read, then the
*       interrupt is not cleared and the input register keeps the logic value
*       that initiated the interrupt. The next read of the input will show the
*       initial state. Care must be taken when using bit reads on the input when
*       latching is enabled - the bit function still reads the entire input
*       register so a change on another bit could be missed. It is best to use
*       port input reads when using latching.
*
*       If the input is changed from latched to non-latched, a read from the
*       input reflects the current terminal logic level. If the input is changed
*       from non-latched to latched input, the read from the input represents
*       the latched logic level.
*   - [DIO_OUTPUT_TYPE](@ref DIO_OUTPUT_TYPE): Set the output type by writing 0
*       for push-pull or 1 for open-drain. This setting affects all outputs so
*       is not a per-channel setting and the channel argument will be ignored.
*       It should be set to the desired type before using the
*       [DIO_DIRECTION](@ref DIO_DIRECTION) item to set channels as outputs.
*       Internal pull-up/down resistors are disabled when a bit is set to output
*       and is configured as open-drain, so external resistors should be used.
*   - [DIO_INT_MASK](@ref DIO_INT_MASK): Enable or disable interrupt generation
*       for the input by masking the interrupt. Write 0 to enable the interrupt
*       or 1 to disable it.
*
*       All MCC 152s share a single interrupt signal to the CPU, so when an
*       interrupt occurs the user must determine the source, optionally act on
*       the interrupt, then clear that source so that other interrupts may be
*       detected. The current interrupt state may be read with
*       hat_interrupt_state(). A user program may wait for the interrupt to
*       become active with hat_wait_for_interrupt(), or may register an
*       interrrupt callback function with hat_interrupt_callback_enable(). This
*       allows the user to wait for a change on one or more inputs without
*       constantly reading the inputs. The source of the interrupt may be
*       determined by reading the interrupt status of each MCC 152 with
*       mcc152_dio_int_status_read_bit() or mcc152_dio_int_status_read_port(),
*       and all active interrupt sources must be cleared before the interrupt
*       will become inactive. The interrupt is cleared by reading the input(s)
*       with mcc152_dio_input_read_bit() or mcc152_dio_input_read_port().
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The digital I/O channel, 0 - 7.
*   @param item     The config item, one of [DIOConfigItem](@ref DIOConfigItem).
*   @param value    The config value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_config_write_bit(uint8_t address, uint8_t channel, uint8_t item,
    uint8_t value);

/**
*   @brief Write a digital I/O configuration value for all channels.
*
*   There are several configuration items that may be written for the digital
*   I/O. They are written for all channels at once using the 8-bit value passed
*   in \b value, where each bit corresponds to a channel (bit 0 is channel 0,
*   etc.) The item is selected with the \b item argument, which may be one of
*   the [DIOConfigItem](@ref DIOConfigItem) values:
*   - [DIO_DIRECTION](@ref DIO_DIRECTION): Set the digital I/O channel
*       directions by passing 0 in a bit for output and 1 for input.
*   - [DIO_PULL_CONFIG](@ref DIO_PULL_CONFIG): Configure the pull-up/down
*       resistors by passing 0 in a bit for pull-down or 1 for pull-up. The
*       resistors may be enabled or disabled with the
*       [DIO_PULL_ENABLE](@ref DIO_PULL_ENABLE) item.
*   - [DIO_PULL_ENABLE](@ref DIO_PULL_ENABLE): Enable or disable pull-up/down
*       resistors by passing 0 in a bit for disabled or 1 for enabled. The
*       resistors are configured for pull-up/down with the
*       [DIO_PULL_CONFIG](@ref DIO_PULL_CONFIG) item. The resistors are
*       automatically disabled if a bit is set to output and is configured as
*       open-drain.
*   - [DIO_INPUT_INVERT](@ref DIO_INPUT_INVERT): Enable inverting inputs by
*       passing a 0 in a bit for normal input or 1 for inverted.
*   - [DIO_INPUT_LATCH](@ref DIO_INPUT_LATCH): Enable input latching by passing
*       0 in a bit for non-latched or 1 for latched.
*
*       When the input is non-latched, reads show the current status of the
*       input. A state change in the corresponding input generates an interrupt
*       (if it is not masked). A read of the input clears the interrupt. If the
*       input goes back to its initial logic state before the input is read,
*       then the interrupt is cleared.
*
*       When the input is latched, a change of state of the input generates an
*       interrupt and the input logic value is loaded into the input port
*       register. A read of the input will clear the interrupt. If the input
*       returns to its initial logic state before the input is read, then the
*       interrupt is not cleared and the input register keeps the logic value
*       that initiated the interrupt. The next read of the input will show the
*       initial state. Care must be taken when using bit reads on the input when
*       latching is enabled - the bit function still reads the entire input
*       register so a change on another bit could be missed. It is best to use
*       port input reads when using latching.
*
*       If the input is changed from latched to non-latched, a read from the
*       input reflects the current terminal logic level. If the input is changed
*       from non-latched to latched input, the read from the input represents
*       the latched logic level.
*   - [DIO_OUTPUT_TYPE](@ref DIO_OUTPUT_TYPE): Set the output type by writing 0
*       for push-pull or 1 for open-drain. This setting affects all outputs so
*       is not a per-channel setting. It should be set to the desired type
*       before using [DIO_DIRECTION](@ref DIO_DIRECTION) to set channels as
*       outputs. Internal pull-up/down resistors are disabled when a bit is set
*       to output and is configured as open-drain, so external resistors should
*       be used.
*   - [DIO_INT_MASK](@ref DIO_INT_MASK): Enable or disable interrupt generation
*       for specific inputs by masking the interrupts. Write 0 in a bit to
*       enable the interrupt from that channel or 1 to disable it.
*
*       All MCC 152s share a single interrupt signal to the CPU, so when an
*       interrupt occurs the user must determine the source, optionally act on
*       the interrupt, then clear that source so that other interrupts may be
*       detected. The current interrupt state may be read with
*       hat_interrupt_state(). A user program may wait for the interrupt to
*       become active with hat_wait_for_interrupt(), or may register an
*       interrrupt callback function with hat_interrupt_callback_enable(). This
*       allows the user to wait for a change on one or more inputs without
*       constantly reading the inputs. The source of the interrupt may be
*       determined by reading the interrupt status of each MCC 152 with
*       mcc152_dio_int_status_read_bit() or mcc152_dio_int_status_read_port(),
*       and all active interrupt sources must be cleared before the interrupt
*       will become inactive. The interrupt is cleared by reading the input(s)
*       with mcc152_dio_input_read_bit() or mcc152_dio_input_read_port().
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param item     The config item, one of [DIOConfigItem](@ref DIOConfigItem).
*   @param value    The config value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_config_write_port(uint8_t address, uint8_t item, uint8_t value);

/**
*   @brief Read a digital I/O configuration value for a single channel.
*
*   There are several configuration items that may be read for the digital
*   I/O. The item is selected with the \b item argument, which may be one of
*   the [DIOConfigItem](@ref DIOConfigItem) values:
*   - [DIO_DIRECTION](@ref DIO_DIRECTION): Read the digital I/O channel
*       direction setting, where 0 is output and 1 is input.
*   - [DIO_PULL_CONFIG](@ref DIO_PULL_CONFIG): Read the pull-up/down resistor
*       configuration where 0 is pull-down and 1 is pull-up.
*   - [DIO_PULL_ENABLE](@ref DIO_PULL_ENABLE): Read the pull-up/down resistor
*       enable setting where 0 is disabled and 1 is enabled.
*   - [DIO_INPUT_INVERT](@ref DIO_INPUT_INVERT): Read the input invert setting
*       where 0 is normal input and 1 is inverted.
*   - [DIO_INPUT_LATCH](@ref DIO_INPUT_LATCH): Read the input latching setting
*       where 0 is non-latched and 1 is latched. 
*   - [DIO_OUTPUT_TYPE](@ref DIO_OUTPUT_TYPE): Read the output type setting
*       where 0 is push-pull and 1 is open-drain. This setting affects all
*       outputs so is not a per-channel setting and the channel argument is
*       ignored.
*   - [DIO_INT_MASK](@ref DIO_INT_MASK): Read the interrupt mask setting where 0
*       enables the interrupt and 1 disables it.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param channel  The digital I/O channel, 0 - 7.
*   @param item     The config item, one of [DIOConfigItem](@ref DIOConfigItem).
*   @param value    Receives the config value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_config_read_bit(uint8_t address, uint8_t channel, uint8_t item,
    uint8_t* value);

/**
*   @brief Read a digital I/O configuration value for all channels.
*
*   There are several configuration items that may be read for the digital
*   I/O. They are read for all channels at once, returning an 8-bit value
*   in \b value, where each bit corresponds to a channel (bit 0 is channel 0,
*   etc.) The item is selected with the \b item argument, which may be one of
*   the [DIOConfigItem](@ref DIOConfigItem) values:
*   - [DIO_DIRECTION](@ref DIO_DIRECTION): Read the digital I/O channels
*       direction settings, where 0 for a bit is output and 1 is input.
*   - [DIO_PULL_CONFIG](@ref DIO_PULL_CONFIG): Read the pull-up/down resistor
*       configurations where 0 for a bit is pull-down and 1 is pull-up.
*   - [DIO_PULL_ENABLE](@ref DIO_PULL_ENABLE): Read the pull-up/down resistor
*       enable settings where 0 for a bit is disabled and 1 is enabled.
*   - [DIO_INPUT_INVERT](@ref DIO_INPUT_INVERT): Read the input invert settings
*       where 0 for a bit is normal input and 1 is inverted.
*   - [DIO_INPUT_LATCH](@ref DIO_INPUT_LATCH): Read the input latching settings
*       where 0 for a bit is non-latched and 1 is latched. 
*   - [DIO_OUTPUT_TYPE](@ref DIO_OUTPUT_TYPE): Read the output type setting
*       where 0 is push-pull and 1 is open-drain. This setting affects all
*       outputs so is not a per-channel setting.
*   - [DIO_INT_MASK](@ref DIO_INT_MASK): Read the interrupt mask settings where
*       0 enables the interrupt from the corresponding channel and 1 disables
*       it.
*
*   @param address  The board address (0 - 7). Board must already be opened.
*   @param item     The config item, one of [DIOConfigItem](@ref DIOConfigItem).
*   @param value    Receives the config value.
*   @return [Result code](@ref ResultCode),
*       [RESULT_SUCCESS](@ref RESULT_SUCCESS) if successful.
*/
int mcc152_dio_config_read_port(uint8_t address, uint8_t item, uint8_t* value);

#ifdef __cplusplus
}
#endif

#endif
