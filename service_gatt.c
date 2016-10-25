/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    service_gatt.c
 *
 *  DESCRIPTION
 *    This file defines routines for using GATT service.
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt.h>           /* GATT application interface */
#include <gatt_uuid.h>      /* Common Bluetooth UUIDs and macros */
#include <buf_utils.h>      /* Buffer functions */
#include <nvm.h>            /* Access to Non-Volatile Memory */

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "service_gatt.h"   /* Interface to this file */
#include "app_gatt.h"
#include "app_gatt_db.h"    /* GATT Database definitions (auto-generated) */
#include "remote.h"

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* The position of the Service Changed configuration information in the NVM */
#define GATT_NVM_SERV_CHANGED_CLIENT_CONFIG_OFFSET  0

/* The position of the "this device might have been updated" flag in the NVM */
#define GATT_NVM_SERV_CHANGED_SEND_IND              \
                                (GATT_NVM_SERV_CHANGED_CLIENT_CONFIG_OFFSET + \
                                 sizeof(gattData.service_changed_config))

/* The maximum number of NVM words used by this GATT implementation */
#define GATT_SERV_CHANGED_NVM_MEMORY_WORDS          \
                                (GATT_NVM_SERV_CHANGED_SEND_IND + \
                                 sizeof(gattData.service_changed))

/*=============================================================================*
 *  Private Data Types
 *============================================================================*/

/* GATT service data structure */
typedef struct _GATT_DATA_T
{
    /* Flag to indicate whether the service has changed */
    uint16 service_changed;
    
    /* Client characteristic configuration decriptor value for the Service
     * Changed characteristic
     */
    gatt_client_config service_changed_config;
    
    /* NVM Offset at which the GATT Service data is stored */
    uint16 nvm_offset;
    
} GATT_DATA_T;

/*=============================================================================*
 *  Private Data
 *============================================================================*/

/* GAP service data instance */
static GATT_DATA_T gattData;

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function reads the GATT service data from NVM.
 *
 *  PARAMETERS
 *      p_offset [in]           Offset to GATT Service data in NVM
 *               [out]          Offset to first octet following GATT Service
 *                              data
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void GattReadDataFromNVM(uint16 *p_offset)
{
    /* Record offset to GATT Service data in NVM */
    gattData.nvm_offset = *p_offset;
    
    /* Read NVM only if devices are bonded */
    if (localData.bonded)
    {
        /* Read Service Changed client configuration */
        NvmRead((uint16 *)&gattData.service_changed_config,
                sizeof(gattData.service_changed_config),
                *p_offset + GATT_NVM_SERV_CHANGED_CLIENT_CONFIG_OFFSET);

        /* Read Service Has Changed flag */
        NvmRead((uint16 *)&gattData.service_changed,
                sizeof(gattData.service_changed),
                *p_offset + GATT_NVM_SERV_CHANGED_SEND_IND);
    }
    else
    {
        /* Initialise values in the NVM */
        gattData.service_changed_config = gatt_client_config_none;
        gattData.service_changed = FALSE;
        
        NvmWrite((uint16 *)&gattData.service_changed_config,
                 sizeof(gattData.service_changed_config),
                 *p_offset + GATT_NVM_SERV_CHANGED_CLIENT_CONFIG_OFFSET);

        NvmWrite((uint16 *)&gattData.service_changed,
                 sizeof(gattData.service_changed),
                 *p_offset + GATT_NVM_SERV_CHANGED_SEND_IND);
    }
    
    /* Increment the offset by the number of words of NVM memory required by the
     * GATT Service */
    *p_offset += GATT_SERV_CHANGED_NVM_MEMORY_WORDS;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattOnConnection
 *
 *  DESCRIPTION
 *      This function should be called when a bonded Host connects.
 *
 *  PARAMETERS
 *      None
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void GattOnConnection(void)
{
    /* If the GATT database has changed indicate this to the Host device if it
     * has enabled indications on this characteristic.
     */
    if (gattData.service_changed &&
       (gattData.service_changed_config == gatt_client_config_indication))
    {
        /* Service Changed characteristic value indicating the range of handles
         * that have changed. Assuming the GATT Service is included first in
         * app_gatt_db.db the range of handles that may have changed is from
         * HANDLE_GATT_SERVICE_END to the largest possible value (0xffff)
         * inclusive. See Bluetooth Core Specification v4.1 Vol 3 Part G
         * Section 7.1.
         */
        uint8 service_changed_data[] = {WORD_LSB(HANDLE_GATT_SERVICE_END),
                                        WORD_MSB(HANDLE_GATT_SERVICE_END),
                                        WORD_LSB(0xffff),
                                        WORD_MSB(0xffff)};

        GattCharValueIndication(localData.st_ucid, 
                                HANDLE_SERVICE_CHANGED, 
                                sizeof(service_changed_data),
                                service_changed_data);
        
        /* Now that the indication has been sent, clear the flag in the NVM */
        gattData.service_changed = FALSE;
        NvmWrite((uint16 *)&gattData.service_changed,
                 sizeof(gattData.service_changed),
                 gattData.nvm_offset + GATT_NVM_SERV_CHANGED_SEND_IND);
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattOnOtaSwitch
 *
 *  DESCRIPTION
 *      This function should be called when the device is switched into 
 *      Over-the-Air Update mode.
 *
 *  PARAMETERS
 *      None
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void GattOnOtaSwitch(void)
{
    if (localData.bonded &&
        (gattData.service_changed_config == gatt_client_config_indication))
    {
        /* Record that a Service Changed indication should be sent to the Host
         * next time it connects.
         */
        gattData.service_changed = TRUE;
        NvmWrite((uint16 *)&gattData.service_changed,
                 sizeof(gattData.service_changed),
                 gattData.nvm_offset + GATT_NVM_SERV_CHANGED_SEND_IND);
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattServiceChangedIndActive
 *
 *  DESCRIPTION
 *      This function allows other modules to read whether the bonded device
 *      has requested indications on the Service Changed characteristic.
 *
 *  PARAMETERS
 *      None
 *
 *  RETURNS
 *      TRUE if the device is bonded and indications have been enabled,
 *      FALSE otherwise
 *----------------------------------------------------------------------------*/
extern bool GattServiceChangedIndActive(void)
{
    return (localData.bonded && 
            (gattData.service_changed_config == gatt_client_config_indication));
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattServiceChangedReset
 *
 *  DESCRIPTION
 *      This function resets the GATT Service Changed configuration in NVM. It
 *      is called whenever pairing information is removed so that the next
 *      paired device does not inherit stale configuration data.
 *
 *  PARAMETERS
 *      None
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void GattServiceChangedReset(void)
{
    /* Initialise values in the NVM */
    gattData.service_changed_config = gatt_client_config_none;
    gattData.service_changed = FALSE;
        
    NvmWrite((uint16 *)&gattData.service_changed_config,
             sizeof(gattData.service_changed_config),
             gattData.nvm_offset + GATT_NVM_SERV_CHANGED_CLIENT_CONFIG_OFFSET);

    NvmWrite((uint16 *)&gattData.service_changed,
             sizeof(gattData.service_changed),
             gattData.nvm_offset + GATT_NVM_SERV_CHANGED_SEND_IND);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      GattHandleAccessRead
 *
 *  DESCRIPTION
 *      Handle read-access requests from the Host where the characteristic
 *      handle falls within the range of the GATT Service.
 *
 *  PARAMETERS
 *      p_ind [in]              Data received in GATT_ACCESS_IND message.
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void GattHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    sys_status  rc = gatt_status_read_not_permitted;
                                            /* Function status */
    uint16      data_length = 0;            /* Length of value, in octets */
    uint8       value[2];                   /* Buffer to hold value */
    uint8      *p_value = NULL;             /* Pointer to value buffer */

    if (p_ind->handle == HANDLE_SERVICE_CHANGED_CLIENT_CONFIG)
    {
        data_length = 2;
        p_value = value;
        BufWriteUint16(&p_value, gattData.service_changed_config);
        rc = sys_status_success;
    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc, data_length, p_value);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      GattHandleAccessWrite
 *
 *  DESCRIPTION
 *      Handle write-access requests from the Host where the characteristic
 *      handle falls within the range of the GATT Service.
 *
 *  PARAMETERS
 *      p_ind [in]              Write request data
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
extern void GattHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    sys_status  rc = gatt_status_write_not_permitted;
                                            /* Function status */
    uint8      *p_value = p_ind->value;     /* Pointer to value buffer */
    
    if (p_ind->handle == HANDLE_SERVICE_CHANGED_CLIENT_CONFIG)
    {
        /* Requested client characteristic configuration descriptor value */
        const uint16 client_config = BufReadUint16(&p_value);
        
        if ((client_config == gatt_client_config_indication) ||
            (client_config == gatt_client_config_none))
        {
            gattData.service_changed_config = client_config;
            NvmWrite((uint16 *)&gattData.service_changed_config,
              sizeof(gattData.service_changed_config),
              gattData.nvm_offset + GATT_NVM_SERV_CHANGED_CLIENT_CONFIG_OFFSET);
            rc = sys_status_success;
        }
        else
        {
            rc = gatt_status_desc_improper_config;
        }
    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      GattCheckHandleRange
 *
 *  DESCRIPTION
 *      Determine whether a characteristic handle is within the range of the
 *      GATT Service.
 *
 *  PARAMETERS
 *      handle [in]             Characteristic handle to check
 *
 *  RETURNS
 *      TRUE if the characteristic belongs to the GATT Service,
 *      FALSE otherwise.
 *----------------------------------------------------------------------------*/
extern bool GattCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_GATT_SERVICE) &&
            (handle <= HANDLE_GATT_SERVICE_END))
            ? TRUE : FALSE;
}
