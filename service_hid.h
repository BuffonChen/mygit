/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      service_hid.h
 *
 *  DESCRIPTION
 *      Header definitions for HID service
 *
 ******************************************************************************/

#ifndef __HID_SERVICE_H__
#define __HID_SERVICE_H__

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt.h"
#include "configuration.h"

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Control operation values can be referred from 
 * http://developer.bluetooth.org/gatt/characteristics/Pages/
 * CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_control_point.xml
 */
/* Suspend command is sent from the host when it wants to enter power saving
 * mode. Exit suspend is used by host to resume normal operations.
 */
typedef enum
{
    hid_suspend      = 0,
    hid_exit_suspend = 1,
    hid_rfu
} hid_control_point_op;

#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
/* Connection interval is in 1.25ms units. */
/* 6 * CI is the spec tolerance for missed events.  See Bluetooth Spec [Vol 6] Part B, Section 4.5.2). */
#define CONNECTION_LATENCY_DISABLE_TIMEOUT (6 * localData.actual_interval * 1250U)
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
/* Initialise the HID module ready for use */
extern void HidDataInit(void);

/* Handler for a READ action from the Central */
extern void HidHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* Handler for a WRITE action from the Central */
extern void HidHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* Determine whether the Central has enabled notifications on the given
 * report identifier.
 */
extern bool HidIsNotifyEnabledOnReportId(uint8 report_id);

/* Send a report to the Central. */
extern void HidSendInputReport(uint8 report_id, uint8 *report, bool force_send);

/* Read HID data from the non-volatile storage. This is used to recover
 * settings information about bonded devices.
 */
extern void HidReadDataFromNVM(bool bonded, uint16 *p_offset);

/* Determine whether a handle is within the range of the HID subsystem. */
extern bool HidCheckHandleRange(uint16 handle);

/* Determine whether the HID service has been suspended by the Central */
extern bool HidIsStateSuspended(void);

#endif /* __HID_SERVICE_H__ */
