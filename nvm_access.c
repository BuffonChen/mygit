/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      nvm_access.c
 *
 *  DESCRIPTION
 *      This file defines routines used by application to access NVM.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <pio.h>
#include <nvm.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "nvm_access.h"
#include "i2c_comms.h"

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------
 *  NAME
 *      Nvm_Read
 *
 *  DESCRIPTION
 *      Read words from the NVM Store after preparing the NVM to be readable. 
 *      After the read operation, perform things necessary in slave 
 *      application to save power on NVM.
 *
 *      Read words starting at the word offset, and store them in the supplied
 *      buffer.
 *
 *      \param buffer  The buffer to read words into.
 *      \param length  The number of words to read.
 *      \param offset  The word offset within the NVM Store to read from.
 *      \return Status of operation.
 *----------------------------------------------------------------------------*/
extern sys_status Nvm_Read(uint16* buffer, uint16 length, uint16 offset)
{
    /* If the NVM has been earlier disabled, the firmware automatically 
     * enables the NVM before writing to it. Therefore, we do not need
     * to do that here.
     */
    sys_status res;
    res = NvmRead(buffer, length, offset);
    
    /* Disable NVM now to save power after read operation */
    Nvm_Disable();
    return res;
}

/*-----------------------------------------------------------------------------
 *  NAME
 *      Nvm_Write
 *
 *  DESCRIPTION
 *      Write words to the NVM store after preparing the NVM to be writable. 
 *      After the write operation, perform things necessary in slave 
 *      application to save power on NVM.
 *
 *      Write words from the supplied buffer into the NVM Store, starting at the
 *      given word offset.
 *
 *      \param buffer  The buffer to write.
 *      \param length  The number of words to write.
 *      \param offset  The word offset within the NVM Store to write to.
 *      \return Status of operation.
 *----------------------------------------------------------------------------*/
extern sys_status Nvm_Write(uint16* buffer, uint16 length, uint16 offset)
{
    /* If the NVM has been earlier disabled, the firmware automatically 
     * enables the NVM before writing to it. Therefore, we do not need
     * to do that here.
     */
    sys_status res;
    res = NvmWrite(buffer, length, offset);
    
    /* Disable NVM now to save power after write operation */
    Nvm_Disable();
    return res;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Disable
 *
 *  DESCRIPTION
 *      Disable the NVM (to save power)
 *
 *---------------------------------------------------------------------------*/
extern void Nvm_Disable(void)
{
    /* Disable the NVM module in the FW */
    NvmDisable();
    
    /* Pull down the I2C lines on the main bus, to save a little power */
    PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down); 
        
    /* Force I2C bus reset next time it is used */
    i2cSetStateUnknown();
}
