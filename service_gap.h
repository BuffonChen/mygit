/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      gap_service.h
 *
 *  DESCRIPTION
 *      Header definitions for GAP service
 *
 ******************************************************************************/

#ifndef __GAP_SERVICE_H__
#define __GAP_SERVICE_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "configuration.h"

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

 /* This function initialises the GAP service data structure. */
extern void GapDataInit(void);

/* This function handles READ operations on GAP service attributes. */
extern void GapHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles WRITE operations on GAP service attributes. */
extern void GapHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function reads GAP-specific data from the non-volatile data store. */
extern void GapReadDataFromNVM(uint16 *p_offset);

/* This function writes GAP-specific data into the non-volatile data store. */
extern void GapInitWriteDataToNVM(uint16 *p_offset);

#ifdef __GAP_PRIVACY_SUPPORT__

/* This function indicates whether peripheral privacy is enabled (or not). */
extern bool GapIsPeripheralPrivacyEnabled(void);

/* Obtain a pointer to the reconnection address. */
extern BD_ADDR_T* GapGetReconnectionAddress(void);

/* This function indicates whether the current reconnection address 
 * is valid.
 */
extern bool GapIsReconnectionAddressValid(void);

/* Set/clear the Peripheral Privacy Flag and write the new setting to the NVM. */
extern void GapSetPeripheralPrivacyFlag(bool flag);

/* Set the Reconnection address and write the updated value to the NVM. */
extern void GapSetReconnectionAddress(uint8 *p_val);

#endif /* __GAP_PRIVACY_SUPPORT__ */

/* Determine whether a given handle is one handled by the GAP module. */
extern bool GapCheckHandleRange(uint16 handle);

/* Obtain a pointer to the current GAP device name, and the name length. */
extern uint8 *GapGetNameAndLength(uint16 *p_name_length);

#endif /* __GAP_SERVICE_H__ */
