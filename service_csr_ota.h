/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      service_csr_ota.h
 *
 *  DESCRIPTION
 *      Header definitions for CSR OTA-update service
 *
 ******************************************************************************/

#ifndef _CSR_OTA_SERVICE_H
#define _CSR_OTA_SERVICE_H

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>          /* Commonly used type definitions */
#include <bt_event_types.h> /* Type definitions for Bluetooth events */

/*============================================================================
 *  Public Data Declarations
 *============================================================================*/

/* Indicates whether the OTA module requires the device to reset on Host device
 * disconnection.
 */
extern bool g_ota_reset_required;

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* Handler for a READ action from the Host */
extern void OtaHandleAccessRead(GATT_ACCESS_IND_T *pInd);

/* Handler for a WRITE action from the Host */
extern void OtaHandleAccessWrite(GATT_ACCESS_IND_T *pInd);

/* Determine whether a handle is within the range of the OTAU Application
 * Service.
 */ 
extern bool OtaCheckHandleRange(uint16 handle);

#endif /* _CSR_OTA_SERVICE_H */
