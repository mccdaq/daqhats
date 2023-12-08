/*
*   mcc134_adc.c
*   Measurement Computing Corp.
*   This file contains functions used with the ADC on the MCC 134.
*
*   02/25/2019
*/
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "daqhats.h"
#include "mcc134_adc.h"
#include "util.h"

// SPI stuff
#define SPI_BUS     0
#define SPI_RATE    2000000         // Keep low due to propagation delay through
                                    // isolators
#define SPI_MODE    SPI_MODE_1
#define SPI_DELAY   0
#define SPI_BITS    8


//*****************************
// Register definitions
#define REG_ID          0x00
#define REG_STATUS      0x01
#define REG_INPMUX      0x02
#define REG_PGA         0x03
#define REG_DATARATE    0x04
#define REG_REF         0x05
#define REG_IDACMAG     0x06
#define REG_IDACMUX     0x07
#define REG_VBIAS       0x08
#define REG_SYS         0x09
#define REG_OFCAL0      0x0A
#define REG_OFCAL1      0x0B
#define REG_OFCAL2      0x0C
#define REG_FSCAL0      0x0D
#define REG_FSCAL1      0x0E
#define REG_FSCAL2      0x0F
#define REG_GPIODAT     0x10
#define REG_GPIOCON     0x11

//*****************************
// Command definitions
#define CMD_NOP			0x00
#define CMD_WAKEUP		0x02
#define CMD_POWERDOWN   0x04
#define CMD_RESET		0x06
#define CMD_START		0x08
#define CMD_STOP		0x0A
#define CMD_SYOCAL		0x16
#define CMD_SYGCAL		0x17
#define CMD_SFOCAL	    0x19
#define CMD_RDATA		0x12
#define CMD_RREG		0x20
#define CMD_WREG		0x40

#define GLOBAL_CHOP

// The MCC 134 uses the 20Hz data rate for maximum 50/60Hz noise rejection.
#define DATA_RATE_INDEX         4
// The TC gain is fixed at 32x for a +/-78.125 mV range to cover all TC types
// plus common-mode headroom.
#define TC_PGA_GAIN_INDEX       5
// The CJC gain is fixed at 1x for a +/-2.5 V range.
#define CJC_PGA_GAIN_INDEX      0

// These are the times from the data sheet based on a 4.096 MHz clock.  However,
// the internal oscillator has a 1.5% tolerance so add that to the times for
// worst case timing.
#ifdef GLOBAL_CHOP
#define N_SETTLE    28
static const uint32_t _conversion_times_us[] =
{
    (uint32_t)((813.008*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((413.008*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((213.008*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((120.508*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((113.008*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((40.313*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((33.820*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((20.313*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((10.313*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((5.313*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((2.813*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((2.313*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((1.313*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((0.813*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5)
};
#else
#define N_SETTLE    14
static const uint32_t _conversion_times_us[] =
{
    (uint32_t)((406.504*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((206.504*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((106.504*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((60.254*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((56.504*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((20.156*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((16.910*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((10.156*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((5.156*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((2.656*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((1.406*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((1.156*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((0.656*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5),
    (uint32_t)((0.406*1.015*1000 + N_SETTLE*16*1.015/4.096e3) + 0.5)
};
#endif

static int spi_fd = -1;


/******************************************************************************
  Perform a SPI transfer to the ADC
 *****************************************************************************/
int _mcc134_spi_transfer(uint8_t address, void* tx_data, void* rx_data,
    uint8_t data_count)
{
    int lock_fd;
    uint8_t temp;
    uint8_t spi_mode;
    int ret;

    if (address >= MAX_NUMBER_HATS)         // Address is invalid
    {
        return RESULT_BAD_PARAMETER;
    }

    // check the SPI device handle
    if (spi_fd < 0)
    {
        return RESULT_RESOURCE_UNAVAIL;
    }

    // Obtain a spi lock
    if ((lock_fd = _obtain_lock()) < 0)
    {
        // could not get a lock within 5 seconds, report as a timeout
        return RESULT_LOCK_TIMEOUT;
    }

    _set_address(address);

    // check spi mode and change if necessary
    ret = ioctl(spi_fd, SPI_IOC_RD_MODE, &temp);
    if (ret == -1)
    {
        _free_address();
        _release_lock(lock_fd);
        return RESULT_COMMS_FAILURE;
    }
    if (temp != SPI_MODE)
    {
        spi_mode = SPI_MODE;
        ret = ioctl(spi_fd, SPI_IOC_WR_MODE, &spi_mode);
        if (ret == -1)
        {
            _free_address();
            _release_lock(lock_fd);
            return RESULT_COMMS_FAILURE;
        }
    }

    // Init the spi ioctl structure
    struct spi_ioc_transfer tr = {
        .tx_buf = (uintptr_t)tx_data,
        .rx_buf = (uintptr_t)rx_data,
        .len = data_count,
        .delay_usecs = SPI_DELAY,
        .speed_hz = SPI_RATE,
        .bits_per_word = SPI_BITS,
    };

    if ((ret = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr)) < 1)
    {
        ret = RESULT_COMMS_FAILURE;
    }
    else
    {
        ret = RESULT_SUCCESS;
    }

    _free_address();
    // clear the SPI lock
    _release_lock(lock_fd);

    return ret;
}


int _mcc134_adc_init(uint8_t address)
{
    uint8_t buffer[10];
    int result;

    if (address >= MAX_NUMBER_HATS)         // check address failed
    {
        return RESULT_BAD_PARAMETER;
    }

    if (spi_fd == -1)
    {
        // SPI device has not been opened yet.
        spi_fd = open(SPI_DEVICE_0, O_RDWR);

        if (spi_fd < 0)
        {
            return RESULT_RESOURCE_UNAVAIL;
        }
    }

    // lock the board
    if (_obtain_board_lock(address) != RESULT_SUCCESS)
    {
        // could not get a lock within 5 seconds, report as a timeout
        return RESULT_LOCK_TIMEOUT;
    }

    // reset the ADC to defaults
    buffer[0] = CMD_RESET;
    if ((result = _mcc134_spi_transfer(address, buffer, NULL, 1)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    // wait 4096 clocks (1ms) before using the ADC
    usleep(1000);

    // check ID and device ready
    buffer[0] = CMD_RREG | REG_ID;
    buffer[1] = 2 - 1;  // count - 1
    buffer[2] = CMD_NOP;
    buffer[3] = CMD_NOP;
    if ((result = _mcc134_spi_transfer(address, buffer, buffer, 4)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    if ((buffer[2] & 0x07) != 0x000)
    {
        // incorrect ID
        _release_board_lock(address);
        return RESULT_INVALID_DEVICE;
    }
    if ((buffer[3] & 0x40) != 0x00)
    {
        _release_board_lock(address);
        return RESULT_BUSY;
    }

    // Initialize the registers that don't use default values
    buffer[0] = CMD_WREG | REG_INPMUX;
    buffer[1] = 8-1;        // count - 1
    buffer[2] = 0x88;       // INPMUX, REF 2.5V on both
    buffer[3] = 0x08 + TC_PGA_GAIN_INDEX;    // PGA
#ifdef GLOBAL_CHOP
    buffer[4] = 0x90 + DATA_RATE_INDEX;      // DATARATE
#else
    buffer[4] = 0x10 + DATA_RATE_INDEX;      // DATARATE
#endif
    buffer[5] = 0x3A;       // REF
    buffer[6] = 0x80;       // IDACMAG
    buffer[7] = 0xFF;       // IDACMUX
    buffer[8] = 0x00;       // VBIAS
    buffer[9] = 0x01;       // SYS
    if ((result = _mcc134_spi_transfer(address, buffer, NULL, 10)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    buffer[0] = CMD_START;
    if ((result = _mcc134_spi_transfer(address, buffer, NULL, 1)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    usleep(1000);

    _release_board_lock(address);

    return RESULT_SUCCESS;
}

int _mcc134_adc_read_tc_code(uint8_t address, uint8_t hi_input,
    uint8_t lo_input, uint32_t* code)
{
    uint32_t mycode;
    uint8_t regval;
    uint8_t buffer[5];
    uint8_t rbuffer[5];
    int result;

    // lock the board
    if (_obtain_board_lock(address) != RESULT_SUCCESS)
    {
        // could not get a lock within 5 seconds, report as a timeout
        return RESULT_LOCK_TIMEOUT;
    }

    // change the mux before the pga in case we are on a CJC channel
    regval = (hi_input << 4) | (lo_input);
    buffer[0] = CMD_WREG | REG_INPMUX;
    buffer[1] = 2-1;
    buffer[2] = regval; // INPMUX: select positive and negative ADC inputs
    buffer[3] = 0x08 + TC_PGA_GAIN_INDEX;  // PGA
    if ((result = _mcc134_spi_transfer(address, buffer, NULL, 4)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    // wait for conversion
    usleep(_conversion_times_us[DATA_RATE_INDEX]);

    buffer[0] = CMD_RDATA;
    buffer[1] = CMD_NOP;
    buffer[2] = CMD_NOP;
    buffer[3] = CMD_NOP;
    buffer[4] = CMD_NOP;

    memset(rbuffer, 0, 5);

    if ((result = _mcc134_spi_transfer(address, buffer, rbuffer, 5)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    mycode = ((uint32_t)rbuffer[1] << 24) |
             ((uint32_t)rbuffer[2] << 16) |
             ((uint32_t)rbuffer[3] << 8)  |
             rbuffer[4];

    // check for open channel; if found, switch to Vref for both inputs to help
    // settle for next reading
    if ((mycode & 0x00FFFFFF) == 0x7FFFFF)
    {
        buffer[0] = CMD_WREG | REG_INPMUX;
        buffer[1] = 1-1;
        buffer[2] = 0x88; // INPMUX: select positive and negative ADC inputs
        if ((result = _mcc134_spi_transfer(address, buffer, NULL, 3)) !=
            RESULT_SUCCESS)
        {
            _release_board_lock(address);
            return result;
        }
    }

    _release_board_lock(address);

    if (code)
    {
        *code = mycode;
    }

    return RESULT_SUCCESS;
}

int _mcc134_adc_read_cjc_code(uint8_t address, uint8_t hi_input,
    uint8_t lo_input, uint32_t* code)
{
    int32_t mycode;
    uint8_t regval;
    uint8_t buffer[7];
    uint8_t rbuffer[5];
    int result;

    // lock the board
    if (_obtain_board_lock(address) != RESULT_SUCCESS)
    {
        // could not get a lock within 5 seconds, report as a timeout
        return RESULT_LOCK_TIMEOUT;
    }

    buffer[0] = CMD_WREG | REG_INPMUX;
    buffer[1] = 2-1;
    regval = (hi_input << 4) | (lo_input);
    buffer[2] = regval; // INPMUX: select positive and negative ADC inputs
    buffer[3] = 0x08 + CJC_PGA_GAIN_INDEX;  // PGA
    if ((result = _mcc134_spi_transfer(address, buffer, NULL, 4)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    // wait for conversion
    usleep(_conversion_times_us[DATA_RATE_INDEX]);

    buffer[0] = CMD_RDATA;
    buffer[1] = CMD_NOP;
    buffer[2] = CMD_NOP;
    buffer[3] = CMD_NOP;
    buffer[4] = CMD_NOP;

    memset(rbuffer, 0, 5);

    if ((result = _mcc134_spi_transfer(address, buffer, rbuffer, 5)) !=
        RESULT_SUCCESS)
    {
        _release_board_lock(address);
        return result;
    }

    mycode = (int32_t)rbuffer[2] << 16 |
             (int32_t)rbuffer[3] << 8  |
             rbuffer[4];

    _release_board_lock(address);

    // check sign - should never be negative
    if (mycode & 0x00800000)
    {
        return RESULT_UNDEFINED;
    }

    if (code)
    {
        *code = (uint32_t)mycode;
    }

    return RESULT_SUCCESS;
}

