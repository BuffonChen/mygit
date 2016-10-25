/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      service_gatt.h
 *
 *  DESCRIPTION
 *      Header definitions for GAP service
 *
 ******************************************************************************/

#ifndef __GATT_SERVICE_H__
#define __GATT_SERVICE_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <bt_event_types.h> /* Type definitions for Bluetooth events */

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function reads the GATT Service data from NVM. */
extern void GattReadDataFromNVM(uint16 *p_offset);

/* This function should be called when a bonded Host connects. */
extern void GattOnConnection(void);

/* This function allows other modules to read whether the bonded device
 * has requested indications on the Service Changed characteristic.
 */
extern bool GattServiceChangedIndActive(void);

/* This function should be called whenever pairing information is removed for
 * the device.
 */
extern void GattServiceChangedReset(void);

/* This function should be called when the device is switched into 
 * Over-the-Air Update mode.
 */
extern void GattOnOtaSwitch(void);

/* This function handles read operations on GATT Service attributes. */
extern void GattHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles write operations on GATT Service attributes. */
extern void GattHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* Determine whether the supplied handle is supported by the GATT Service. */
extern bool GattCheckHandleRange(uint16 handle);

#endif /* __GATT_SERVICE_H__ */
