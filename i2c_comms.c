/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    i2c_comms.c
 *
 *  DESCRIPTION
 *    This file defines different I2C procedures.
 *
 ******************************************************************************/
/*=============================================================================
 *  SDK Header Files
 *============================================================================*/
#include <i2c.h>

/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "configuration.h"
#include "i2c_comms.h"

#if defined(PERIPHERAL_I2C_EXISTS)
#if !defined(PERIPHERAL_SDA_PIO) || !defined(PERIPHERAL_SCL_PIO)
#error "To use the peripheral I2C bus, defined the PIOs used for SDA/SCL"
#endif /* !PERIPHERAL_SDA_PIO || !PERIPHERAL_SCL_PIO */
#endif /* PERIPHERAL_I2C_EXISTS */

/*=============================================================================
 *  Local Definitions
 *============================================================================*/
 
/* The maximum amount of time to wait for the I2C bus to settle after being reset. */
#define I2C_MAX_RESET_DELAY (1*MILLISECOND)

/*=============================================================================
 *  Local Variables
 *============================================================================*/

static I2C_CURRENT_BUS currentBus = I2C_UNKNOWN_BUS;

/*=============================================================================
 *  Private function definitions
 *============================================================================*/
/* None */
 
/*=============================================================================
 *  Public function definitions
 *============================================================================*/
/*---------------------------------------------------------------------------
 *
 *  NAME
 *      checkI2cBusState
 *
 *  DESCRIPTION
 *      Check if I2C bus is ready, if not, reset it and wait a little.
 *
 *----------------------------------------------------------------------------*/
extern void checkI2cBusState(void)
{
    bool result;
    
    if(I2cReady() == FALSE)
    {
        /* We are in a single-threaded environment, so if the I2C bus is not
         * ready now it has probably locked up and needs resetting.
         * This can happen on boards where there is noise on the I2C bus.
         * See also EXCLUSIVE_I2C_AND_KEYSCAN in configuration.h.
         */
        I2cReset();
        
        /* Allow a short settling time */
        TimeWaitWithTimeout16(I2cReady(), I2C_MAX_RESET_DELAY, result);
    }
}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      i2cSetStateUnknown
 *
 *  DESCRIPTION
 *      Forces this module to re-configure the I2C bus next time it is used.
 *
 *----------------------------------------------------------------------------*/
extern void i2cSetStateUnknown(void)
{
    currentBus = I2C_UNKNOWN_BUS;
}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      i2cUseMainBus
 *
 *  DESCRIPTION
 *      This function configures the I2C controller to use the dedicated
 *      I2C bus for communication with peripheral devices.
 *
 *----------------------------------------------------------------------------*/
extern void i2cUseMainBus(void)
{
    if (currentBus != I2C_DEDICATED_BUS)
    {
        /* Disable the I2C controller */
        I2cEnable(FALSE);
    
        /* Configure the I2C controller */
        I2cInit(I2C_RESERVED_PIO,
                I2C_RESERVED_PIO,
                2, 
                pio_i2c_pull_mode_strong_pull_up);
 
#if defined(PERIPHERAL_I2C_EXISTS)               
        /* In case the I2C bus had been changed in order to configure the
         * one of the peripheral devices, reset the pins used for the peripheral 
         * I2C (otherwise these can interfere with the dedicated bus).
         */
        PioSetModes(    ((0x01L << PERIPHERAL_SDA_PIO) | (0x01L << PERIPHERAL_SCL_PIO)), pio_mode_user);
        PioSetDirs(     ((0x01L << PERIPHERAL_SDA_PIO) | (0x01L << PERIPHERAL_SCL_PIO)), FALSE); /* input */
        PioSetPullModes(((0x01L << PERIPHERAL_SDA_PIO) | (0x01L << PERIPHERAL_SCL_PIO)), pio_mode_no_pulls); 
        PioSetEventMask(((0x01L << PERIPHERAL_SDA_PIO) | (0x01L << PERIPHERAL_SCL_PIO)), pio_event_mode_disable);
#endif /* PERIPHERAL_I2C_EXISTS */

        /* Configure 400KHz clock */
        I2cConfigClock(I2C_SCL_400KBPS_HIGH_PERIOD, I2C_SCL_400KBPS_LOW_PERIOD);
        
        /* Enable poll the EEPROM for write completion */
        I2cEepromSetWriteCycleTime(I2C_EEPROM_POLLED_WRITE_CYCLE);

        /* Enable the I2C controller */
        I2cEnable(TRUE);
    
        currentBus = I2C_DEDICATED_BUS;
    }
}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      i2cUsePeripheralBus
 *
 *  DESCRIPTION
 *      This function configures the I2C controller to use the peripheral
 *      I2C bus for communication with peripheral devices.
 *
 *----------------------------------------------------------------------------*/
#if defined(PERIPHERAL_I2C_EXISTS)  
extern void i2cUsePeripheralBus(void)
{
    if(currentBus != I2C_PERIPHERAL_BUS)
    {
        /* Shut the I2C controller */
        /* Doing so disables pull resistors on dedicated I2C bus */
        I2cEnable(FALSE);

        /* Re-route the I2C onto the peripheral bus pins. */
        
        /* Re-initialise I2C controller on new PIOs */
        I2cInit(PERIPHERAL_SDA_PIO, 
                PERIPHERAL_SCL_PIO, 
                I2C_POWER_PIO_UNDEFINED, 
                pio_mode_no_pulls);
            
        /* Pull down the I2C lines on the main bus, to save power */
        PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down); 
        
        /* Set the 400KHz clock */
        I2cConfigClock(I2C_SCL_400KBPS_HIGH_PERIOD, I2C_SCL_400KBPS_LOW_PERIOD);
        
        /* Re-enable I2C controller*/
        I2cEnable(TRUE);
        
        currentBus = I2C_PERIPHERAL_BUS;
    }
}
#endif /* PERIPHERAL_I2C_EXISTS */

/*-----------------------------------------------------------------------------*
 *  NAME
 *      i2cReadRegister
 *
 *  DESCRIPTION
 *      This function reads 1-byte from the specified register of the 
 *      specified device.
 *
 *  RETURNS
 *      TRUE if successful
 *
 *----------------------------------------------------------------------------*/
extern bool i2cReadRegister(uint8 baseAddress, uint8 reg, uint8 *registerValue)
{
    bool success;
    
    /* Check that the bus is ready */
    checkI2cBusState();

    success = ( (I2cRawStart(TRUE)              == sys_status_success) &&
                (I2cRawWriteByte(baseAddress)   == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawWriteByte(reg)           == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawRestart(TRUE)            == sys_status_success) &&
                (I2cRawWriteByte((baseAddress | 0x1)) == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawReadByte(registerValue)  == sys_status_success) &&
                (I2cRawSendNack(TRUE)           == sys_status_success) &&
                (I2cRawStop(TRUE)               == sys_status_success));
    
    I2cRawTerminate();
    
    return success;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      i2cReadRegisters
 *
 *  DESCRIPTION
 *      This function reads a contiguous sequence of registers from the 
 *      specified device
 *
 *  RETURNS
 *      TRUE if successful
 *
 *----------------------------------------------------------------------------*/
extern bool i2cReadRegisters(uint8 baseAddress, uint8 startReg, uint8 numBytes, uint8 *buffer)
{
    bool success = FALSE;

    /* Check that the bus is ready */
    checkI2cBusState();

    /* We assume that the supplied buffer is big enough. */

    success = (I2cRawStart(TRUE)              == sys_status_success) &&
              (I2cRawWriteByte(baseAddress)   == sys_status_success) &&
              (I2cRawWaitAck(TRUE)            == sys_status_success) &&
              (I2cRawWriteByte(startReg)      == sys_status_success) &&
              (I2cRawWaitAck(TRUE)            == sys_status_success) &&
              (I2cRawRestart(TRUE)            == sys_status_success) &&
              (I2cRawWriteByte((baseAddress | 0x1)) == sys_status_success) &&
              (I2cRawWaitAck(TRUE)            == sys_status_success) &&
              (I2cRawRead(buffer, numBytes)   == sys_status_success) &&
              (I2cRawStop(TRUE)               == sys_status_success);
    
    I2cRawTerminate();
    
    return success;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      i2cWriteRegister
 *
 *  DESCRIPTION
 *      This function writes one byte of data to a specified register on the 
 *      specified device
 *
 *  RETURNS
 *      TRUE if successful
 *
 *----------------------------------------------------------------------------*/
extern bool i2cWriteRegister(uint8 baseAddress, uint8 reg, uint8 registerValue)
{
    bool success;

    /* Check that the bus is ready */
    checkI2cBusState();

    success = ( (I2cRawStart(TRUE)              == sys_status_success) &&
                (I2cRawWriteByte(baseAddress)   == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawWriteByte(reg)           == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawWriteByte(registerValue) == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawStop(TRUE)               == sys_status_success));
    
    I2cRawTerminate();
    
    return success;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      i2cWriteRegisters
 *
 *  DESCRIPTION
 *      This function writes a contiguous sequence of registers to the
 *      specified device
 *
 *  RETURNS
 *      TRUE if successful
 *
 *----------------------------------------------------------------------------*/
extern bool i2cWriteRegisters(uint8 baseAddress, uint8 startReg, uint8 numBytes, uint8 *buffer)
{
    bool success;

    /* Wait till the I2C is ready */
    I2cWaitReady();

    success = ( (I2cRawStart(TRUE)              == sys_status_success) &&
                (I2cRawWriteByte(baseAddress)   == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawWriteByte(startReg)      == sys_status_success) &&
                (I2cRawWaitAck(TRUE)            == sys_status_success) &&
                (I2cRawWrite(buffer, numBytes)  == sys_status_success) &&
                (I2cRawStop(TRUE)               == sys_status_success));

    I2cRawTerminate();

    return success;
}
