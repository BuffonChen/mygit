/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      service_battery.h
 *
 *  DESCRIPTION
 *      Header definitions for Battery service
 *
 ******************************************************************************/

#ifndef __BATT_SERVICE_H__
#define __BATT_SERVICE_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* Initialise the Battery service data structure. */
extern void BatteryDataInit(void);

/* Specific Battery-service initialisations to be performed following a chip-reset. */
extern void BatteryInitChipReset(void);

/* Handler for a READ action from the Central */
extern void BatteryHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* Handler for a WRITE action from the Central */
extern void BatteryHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* Monitor the battery level and send notifications (as required) to the Central. */
extern void BatteryUpdateLevel(uint16 ucid);

/* Read Battery-service -specific data from the non-volatile storage. */
extern void BatteryReadDataFromNVM(bool bonded, uint16 *p_offset);

/* Determine whether a handle is within the range of the Battery subsystem. */ 
extern bool BatteryCheckHandleRange(uint16 handle);

#endif /* __BATT_SERVICE_H__ */
