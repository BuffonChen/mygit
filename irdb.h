/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2015
 *
 * FILE
 *    irdb.h
 *
 *  DESCRIPTION
 *    Data defined IR transmission interface definition. 
 *
 ******************************************************************************/

#ifndef __IRDB_H__
#define __IRDB_H__

/*============================================================================
 *  Local Header Files
 *============================================================================*/

#include "ircdfs.h"

/*============================================================================
 *  Public Function Prototypes
 *============================================================================*/

/*-----------------------------------------------------------------------------
 *  NAME
 *      irdb_Init
 *
 *  DESCRIPTION
 *      This function is called during the application's initialisation.
 *      
 *----------------------------------------------------------------------------*/
extern void irdb_Init(void);

/*-----------------------------------------------------------------------------
 *  NAME
 *      irdb_KeyPressed
 *
 *  DESCRIPTION
 *      This function is called when a key is pressed. 
 * 
 *  RETURNS
 *      TRUE if valid IR command definition is found for the key. 
 *      FALSE otherwise.
 *      
 *----------------------------------------------------------------------------*/
extern bool irdb_KeyPressed(uint16 keyId);

/*-----------------------------------------------------------------------------
 *  NAME
 *      irdb_KeyReleased
 *
 *  DESCRIPTION
 *      This function is called when the previously pressed key is released.
 *      
 *----------------------------------------------------------------------------*/
extern void irdb_KeyReleased(void);

/*-----------------------------------------------------------------------------
 *  NAME
 *      irdb_SetNextPattern
 *
 *  DESCRIPTION
 *      This function is called when an interrupt is received from the PIO
 *      controller requesting for the next Mark Space periods. It sets the 
 *      next pair of Mark and Space times for the PIO controller, or if the 
 *      end of the IR command is reached, sets the PIO controller to cease
 *      transmission.
 *      
 *----------------------------------------------------------------------------*/
extern void irdb_SetNextPattern(void);

/*-----------------------------------------------------------------------------
 *  NAME
 *      irdb_PrepareDevice
 *
 *  DESCRIPTION
 *      This function is called to when the controlled device is changed.    
 *      
 *----------------------------------------------------------------------------*/
extern bool irdb_PrepareDevice(IRCDFS_ACCESSOR *ircdDetails);


#endif /* __IRDB_H__ */

/*============================================================================
 * End of file: irdb.h
 *============================================================================*/
