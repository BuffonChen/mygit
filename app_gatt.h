/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      app_gatt.h
 *
 *  DESCRIPTION
 *      Header definitions for common application attributes
 *
 ******************************************************************************/

#ifndef __APP_GATT_H__
#define __APP_GATT_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <panic.h>

#ifdef DEBUG_ENABLE
#include <debug.h>
#endif /* DEBUG_ENABLE */

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* This error code should be returned when a remote connected device writes a
 * configuration which the application does not support.
 */
#define gatt_status_desc_improper_config (STATUS_GROUP_GATT+ 0xFD)
 
/* Extract low order byte of 16-bit UUID */
#define LOW_BYTE(x)        ((x) & 0x00ff)
/* Extract high order byte of 16-bit UUID */
#define HIGH_BYTE(x)       (((x) >> 8) & 0x00ff)
/* Extract bits 16-23 of a 32-bit number */
#define THIRD_BYTE(x)      (((x) >> 16) & 0x000000ff)

/* The Maximum Transmission Unit length supported by this device. */
#define ATT_MTU     23

/* The maximum user data that can be carried in each radio packet. */
#define MAX_DATA_LENGTH     (ATT_MTU-3)     /* MTU minus 3 bytes header */

/* Maximum Length of Device Name. After taking into consideration other
 * elements added to advertisement, the advertising packet can receive upto a 
 * maximum of 31 octets from application
 */
#define DEVICE_NAME_MAX_LENGTH               (20)

/* Timer value for remote device to re-encrypt the link using old keys */
#define BONDING_CHANCE_TIMER                 (30*SECOND)

/* Invalid UCID indicating we are not currently connected */
#define GATT_INVALID_UCID                    (0xFFFF)

/* Invalid Attribute Handle */
#define INVALID_ATT_HANDLE                   (0x0000)

/* Extract low order byte of 16-bit UUID */
#define LE8_L(x)                             ((x) & 0xff)

/* Extract low order byte of 16-bit UUID */
#define LE8_H(x)                             (((x) >> 8) & 0xff)

/* Client Characteristic Configuration Descriptor improperly configured */
#define GATT_CCCD_ERROR                      (0xFD)

/*=============================================================================*
 *  Public Data Types
 *============================================================================*/

/* GATT Client Charactieristic Configuration Value [Ref GATT spec, 3.3.3.3]*/
typedef enum
{
    gatt_client_config_none = 0x0000,
    gatt_client_config_notification = 0x0001,
    gatt_client_config_indication = 0x0002,
    gatt_client_config_reserved = 0xFFF4

} gatt_client_config;

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

extern void AppUpdateWhiteList(void);

#endif /* __APP_GATT_H__ */
