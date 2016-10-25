/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2014
 *
 *  FILE
 *      hid_ota.h
 *
 *  DESCRIPTION
 *      This file contains definitions for the OTA update with HoG
 *
 ******************************************************************************/
#ifndef __HID_OTA_H__
#define __HID_OTA_H__

/*=============================================================================*
 *  SDK Header File
 *============================================================================*/
#include <types.h>

/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "configuration.h"
#include "hid_ota_report_ids.h"

/*=============================================================================*
 *  Public Data Types
 *============================================================================*/
/* Type of the message being processed */
typedef enum {
    CSR_HID_OTA_MSG_TYPE_FEATURE,           /* Feature message contains the
                                                    protocol information */
    CSR_HID_OTA_MSG_TYPE_CTRL_FROM_HOST,    /* Control data sent by the host */
    CSR_HID_OTA_MSG_TYPE_CTRL_TO_HOST,      /* Control data sent to the host */
    CSR_HID_OTA_MSG_TYPE_FRAGMENT,          /* Update image fragment sent
                                                by the host */
} CSR_HID_OTA_MSG_TYPE;

/*============================================================================*
 *  Public function prototypes
 *============================================================================*/
/* Initialise the OTA update service data structure. */
extern void OtaDataInit(void);

/* Initialise the the OTAU data structures.
 *
 * NOTE: this function claims the I2C bus for the duration of the call.
 * Applications using the I2C bus may need to re-initialise the bus after
 * calling this function.
 *  */
extern void OtaInit(void);

/* Obtain the OTA Feature information.
 *
 * The length must be at lease the size of the feature information
 * as set by OTA_LIB_LEN_FEATURE_MSG.
 *
 * @return FALSE if invalid length specified, TRUE otherwise
 */
extern bool GetHidOtaFeatureInformation( uint8* msg, uint16 len );

/* Process the control message from the Central device
 *
 * NOTE: this function claims the I2C bus for the duration of the call.
 * Applications using the I2C bus may need to re-initialise the bus after
 * calling this function.
 *
 * NOTE: The EEPROM Write protection (if configured) will be disabled
 * at the start of the OTA image transfer and will be enabled once
 * the transfer is completed. Application should therefore, during the
 * course of the OTA process should not alter the EEPROM write protection.
 *  */
extern void ProcessOtaMsgFromHost(CSR_HID_OTA_MSG_TYPE msgType,
            uint8* msg, uint16 len);

/* Process a disconnection, if requested by the OTA Library.
 *
 * NOTE: this will reboot the device if an OTA update transfer has been
 * completed and a reset was pending.
 *
 * NOTE: if challenge-response mechanism is in force, the EEPROM Write
 * protection (if configured) will be re-enabled and the OTA transfer
 * status will be reset. Upon a re-connection host shall re-negotiate
 * and re-start the OTAU data transfer.
 */
extern void OtaProcessDisconnection( void );

/*=============================================================================*
 *  OTA Library callbacks into the application, to be implemented by the
 *  application.
 *============================================================================*/
/* Send OTA message to the Central device
 *
 * NOTE: len must not exceed the maximum size of the messagse as set
 * by OTA_LIB_LEN_CTRL_MSG_TO_HOST
 * */
extern bool SendOtaMsgToHost(CSR_HID_OTA_MSG_TYPE msgType,
            uint8* msg, uint16 len);

/* OTA Upgrade library calls this function to request the application to
 * disconnect from the host (if not disconnected already) and reboot.
 *
 *  */
extern void OtaPerformDisconnect( void );

#endif /* __HID_OTA_H__ */