/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      service_csr_ota.c
 *
 *  DESCRIPTION
 *      Implements the CSR OTA-update service
 *
 ******************************************************************************/
 
/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt.h>           /* GATT application interface */
#include <gatt_uuid.h>      /* Common Bluetooth UUIDs and macros */
#include <buf_utils.h>      /* Buffer functions */
#include <nvm.h>            /* Access to Non-Volatile Memory */
#include <ble_hci_test.h>   /* RF test routines */
#include <mem.h>            /* Memory access routines */
#include <memory.h>         /* Memory map */
#include <csr_ota.h>        /* CSR OTA Update library */

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#if defined(USE_STATIC_RANDOM_ADDRESS) || defined(USE_RESOLVABLE_RANDOM_ADDRESS)
  #include <gap_app_if.h>   /* GAP application interface */
#endif /* USE_STATIC_RANDOM_ADDRESS || USE_RESOLVABLE_RANDOM_ADDRESS */

#include "app_gatt.h"
#include "app_gatt_db.h"
#include "i2c_comms.h"
#include "remote.h"
#include "service_gatt.h"
#include "service_csr_ota.h"
#include "uuids_csr_ota.h"

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

/* Convert 16-bit words into 8-bit bytes */
#define WORDS_TO_BYTES(_w_) (_w_ << 1)
#define BYTES_TO_WORDS(_b_) (((_b_) + 1) >> 1)

/*============================================================================
 *  Public Data
 *============================================================================*/

/* Indicates whether the OTA module requires the device to reset on Host device
 * disconnection.
 */
bool g_ota_reset_required = FALSE;

/*=============================================================================*
 *  Private Data
 *============================================================================*/

/* The current value of the DATA TRANSFER characteristic */
static uint8 data_transfer_memory[ATTR_LEN_CSR_OTA_DATA_TRANSFER] = {0};

/* The number of bytes of valid data in data_transfer_memory */
static uint8 data_transfer_data_length = 0;

/* The current configuration of the DATA TRANSFER characteristic */
static uint8 data_transfer_configuration[2] = {gatt_client_config_none, 0};

/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      readCsBlock
 *
 *  DESCRIPTION
 *      Read a section of the CS block.
 *
 *      This function is called when the Host requests the contents of a CS
 *      block, by writing to the OTA_READ_CS_BLOCK handle. This function is
 *      supported only if the application supports the OTA_READ_CS_BLOCK
 *      characteristic.
 *
 *  PARAMETERS
 *      offset [in]             Offset into the CS block to read from, in words.
 *                              This is the value written by the Host to the
 *                              Characteristic.
 *      length [in]             Number of octets to read.
 *      value [out]             Buffer to contain block contents
 *
 *  RETURNS
 *      sys_status_success: The read was successful and "value" contains valid
 *                          information.
 *      CSR_OTA_KEY_NOT_READ: The read was unsuccessful and "value" does not
 *                          contain valid information.
 *----------------------------------------------------------------------------*/
static sys_status readCsBlock(uint16 offset, uint8 length, uint8 *value)
{
    /* Check the length is within the packet size and that the read does not
     * overflow the CS block.
     */
    if ((length > ATTR_LEN_CSR_OTA_DATA_TRANSFER) ||
        (offset + BYTES_TO_WORDS(length) > CSTORE_SIZE))
    {
        return CSR_OTA_KEY_NOT_READ;
    }

    MemCopyUnPack(value, (uint16 *)(DATA_CSTORE_START + offset), length);

    return sys_status_success;
}

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      OtaHandleAccessRead
 *
 *  DESCRIPTION
 *      Handle read-access requests from the Host where the characteristic
 *      handle falls within the range of the OTAU Application Service.
 *
 *  PARAMETERS
 *      p_ind [in]              Read request data
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
void OtaHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    sys_status  rc = sys_status_success;
                                    /* Function status */
    uint8       data_length = 0;    /* Length of data to return in octets */
    uint8      *p_value = NULL;     /* Pointer to return data */
    
    csr_application_id current_app; /* Index of the current application */
    
    switch (p_ind->handle)
    {
        case HANDLE_CSR_OTA_CURRENT_APP:
            /* Read the index of the current application */
            current_app = OtaReadCurrentApp();
            
            p_value = (uint8 *)&current_app;
            data_length = 1;
            break;
            
        case HANDLE_CSR_OTA_DATA_TRANSFER:
            /* Read the value of the Data Transfer characteristic */
            p_value = (uint8 *)data_transfer_memory;
            data_length = data_transfer_data_length;
            break;
            
        case HANDLE_CSR_OTA_DATA_TRANSFER_CLIENT_CONFIG:
            /* Read the value of the Data Transfer Client Characteristic
             * Configuration Descriptor
             */
            p_value = (uint8 *)data_transfer_configuration;
            data_length = 2;
            break;
            
        default:
            /* Reading is not supported on this handle */
            rc = gatt_status_read_not_permitted;
            break;
    }
    
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, data_length, p_value);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      OtaHandleAccessWrite
 *
 *  DESCRIPTION
 *      Handle write-access requests from the Host where the characteristic
 *      handle falls within the range of the OTAU Application Service.
 *
 *  PARAMETERS
 *      p_ind [in]              Write request data
 *
 *  RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
void OtaHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    sys_status  rc = gatt_status_write_not_permitted;
                                    /* Function status */
        
    switch (p_ind->handle)
    {
        case HANDLE_CSR_OTA_CURRENT_APP:
        {
            /* Set the index of the current application */
            const uint8 app_id = p_ind->value[0];
                                            /* New application index */
#if defined(USE_STATIC_RANDOM_ADDRESS) || defined(USE_RESOLVABLE_RANDOM_ADDRESS)
            BD_ADDR_T   bd_addr;            /* Bluetooth Device address */

            GapGetRandomAddress(&bd_addr);
#endif /* USE_STATIC_RANDOM_ADDRESS || USE_RESOLVABLE_RANDOM_ADDRESS */
            
            rc = OtaWriteCurrentApp(app_id,
                                    localData.bonded,
                                    &(localData.con_bd_addr),
                                  localData.diversifier,
#if defined(USE_STATIC_RANDOM_ADDRESS) || defined(USE_RESOLVABLE_RANDOM_ADDRESS)
                                    &bd_addr,
#else
                                    NULL,
#endif
                                    localData.central_device_irk.irk,
                                    GattServiceChangedIndActive());
            if (rc != sys_status_success)
            {
                /* Sanitise the result. If OtaWriteCurrentApp fails it will be
                 * because one or more of the supplied parameters was invalid.
                 */
                rc = gatt_status_invalid_param_value;
            }
        }
            break;
            
        case HANDLE_CSR_OTA_READ_CS_BLOCK:
            /* Set the offset and length of a block of CS to read */
            
            /* Validate input (expecting uint16[2]) */
            if (p_ind->size_value == WORDS_TO_BYTES(sizeof(uint16[2])))
            {
                const uint16 offset = BufReadUint16(&p_ind->value);
                
                data_transfer_data_length = (uint8)BufReadUint16(&p_ind->value);
            
                rc = readCsBlock(offset,
                                 data_transfer_data_length,
                                 data_transfer_memory);
            }
            else
            {
                rc = gatt_status_invalid_length;
            }
            break;
            
        case HANDLE_CSR_OTA_DATA_TRANSFER_CLIENT_CONFIG:
        {
            /* Modify the Data Transfer Client Characteristic Configuration
             * Descriptor
             */
            const uint16 client_config = BufReadUint16(&p_ind->value);
                                            /* Requested descriptor value */

            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))
            {
                data_transfer_configuration[0] = client_config;
                rc = sys_status_success;
            }
            else
            {
                /* INDICATION or RESERVED */

                /* Return error as only notifications are supported */
                rc = gatt_status_desc_improper_config;
            }
        }
            break;
            
        default:
            /* Writing to this characteristic is not permitted */
            break;
    }
 
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);
    
    /* Perform any follow-up actions */
    if (rc == sys_status_success)
    {
        switch (p_ind->handle)
        {
            case HANDLE_CSR_OTA_READ_CS_BLOCK:
                /* If this write action was to trigger a CS key read and
                 * notifications have been enabled send the result now.
                 */
                if (data_transfer_configuration[0] ==
                                                gatt_client_config_notification)
                {
                    GattCharValueNotification(localData.st_ucid, 
                                              HANDLE_CSR_OTA_DATA_TRANSFER, 
                                              data_transfer_data_length,
                                              data_transfer_memory);
                }
                break;

            case HANDLE_CSR_OTA_CURRENT_APP:
                /* If a new application index has been requested disconnect from
                 * the Host and reset the device to run the new application.
                 */
                
                /* Record that the GATT database may be different after the
                 * device has reset.
                 */
                GattOnOtaSwitch();
                
                /* When the disconnect confirmation comes in, call OtaReset() */
                g_ota_reset_required = TRUE;

                /* Disconnect from the Host */
                GattDisconnectReq(localData.st_ucid);
                break;

            default:
                /* No follow up action necessary */
                break;
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      OtaCheckHandleRange
 *
 *  DESCRIPTION
 *      Determine whether a characteristic handle is within the range of the
 *      OTAU Application Service.
 *
 *  PARAMETERS
 *      handle [in]             Characteristic handle to check
 *
 *  RETURNS
 *      TRUE if the characteristic belongs to the OTAU Application Service,
 *      FALSE otherwise.
 *----------------------------------------------------------------------------*/
bool OtaCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_CSR_OTA_SERVICE) &&
            (handle <= HANDLE_CSR_OTA_SERVICE_END))
            ? TRUE : FALSE;
}
