/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    i2c_comms.h
 *
 *  DESCRIPTION
 *    Header file for the I2C procedures
 *
 ******************************************************************************/
#ifndef _I2C_COMMS_H
#define _I2C_COMMS_H

/*=============================================================================
 *  SDK Header Files
 *============================================================================*/
#include <pio.h>
#include <types.h>

/*=============================================================================
 *  Public Type Definitions
 *============================================================================*/

typedef enum {
    I2C_DEDICATED_BUS,
    I2C_PERIPHERAL_BUS,
    
    I2C_UNKNOWN_BUS
} I2C_CURRENT_BUS;

/*=============================================================================
 *  Public function prototypes
 *============================================================================*/
/* Use the dedicated I2C for communications */
extern void i2cUseMainBus(void);

/* In some cases, it may be necessary to tell the I2C module that the bus has
 * been fiddled with by other modules (the NVM communications, principly).
 */
extern void i2cSetStateUnknown(void);

#if defined(PERIPHERAL_I2C_EXISTS)

/* Re-route the I2C communications on to the peripheral I2C bus */
extern void i2cUsePeripheralBus(void);

#endif /* PERIPHERAL_I2C_EXISTS */

/* 
 * The baseAddress parameter here is the WRITE address for the device 
 */

/* Check if I2C bus is ready, if not, reset it and wait a little. */
extern void checkI2cBusState(void);

/* Read the specified register (1-byte) from the specified device. */
extern bool i2cReadRegister(uint8 baseAddress, uint8 reg, uint8 *registerValue);

/* Read a contiguous sequence of registers from the specified device. */
extern bool i2cReadRegisters(uint8 baseAddress, uint8 startReg, uint8 numBytes, uint8 *buffer);

/* Write 1 byte to the specified register on the specified device. */
extern bool i2cWriteRegister(uint8 baseAddress, uint8 reg, uint8 registerValue);

/* Write a contiguous sequence of registers on the specified device. */
extern bool i2cWriteRegisters(uint8 baseAddress, uint8 startReg, uint8 numBytes, uint8 *buffer);

#endif /* _I2C_COMMS_H */