/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    service_hid.c
 *
 *  DESCRIPTION
 *    This file defines routines for using HID service.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include "configuration.h"

#include <gatt.h>
#include <gatt_prim.h>
#include <mem.h>
#include <buf_utils.h>
#include <pio.h>
#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
#include <timer.h>
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "remote.h"
#include "app_gatt.h"
#include "service_hid.h"
#include "nvm_access.h"
#include "app_gatt_db.h"
#include "key_scan.h"
#include "notifications.h"
#include "hid_descriptor.h"
#if defined(DISCONNECT_ON_IDLE)
#include "event_handler.h"
#endif /* DISCONNECT_ON_IDLE */

/*=============================================================================*
 *  Private Data Types
 *============================================================================*/

typedef struct
{
    /* Reports Client Configuration */
    gatt_client_config      consumer_client_config;
    
    /* Set to TRUE if the HID device is suspended. By default set to FALSE (ie., 
     * Not Suspended)
     */
    bool                    suspended;

    /* NVM offset at which HID data is stored */
    uint16                  nvm_offset;
} HID_DATA_T;


/*=============================================================================*
 *  Private Data
 *============================================================================*/

HID_DATA_T hid_data;
DECLARE_HID_DESCRIPTOR();
#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
timer_id latency_suspension_timer = TIMER_INVALID;
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */

/*=============================================================================*
 *  Private Definitions
 *============================================================================*/
#define HID_SERVICE_NVM_MEMORY_WORDS_BASE                    (3)
#define HID_SERVICE_USE_MOTION_DATA_HILLCREST_FORMAT         (0)
#define HID_SERVICE_MOTION_REPORT_CONFIG_SIZE                (2) 

#define HID_SERVICE_OTAU_OVER_HID_SIZE                       (0)

#define HID_SERVICE_IRTX_OVER_HID_SIZE                        (0)

/* The base of Maximum usage */
#define HID_SERVICE_NVM_MEMORY_WORDS                    (HID_SERVICE_NVM_MEMORY_WORDS_BASE + HID_SERVICE_MOTION_REPORT_CONFIG_SIZE + HID_SERVICE_OTAU_OVER_HID_SIZE + HID_SERVICE_IRTX_OVER_HID_SIZE)

/* The offset of data being stored in NVM for HID service. This offset is added
 * to HID service offset to NVM region (see hid_data.nvm_offset) to get the 
 * absolute offset at which this data is stored in NVM
 */
#define HID_NVM_CONSUMER_REPORT_CONFIG_OFFSET                (0)
#define HID_NVM_KEYBOARD_REPORT_CONFIG_OFFSET                (1)
#define HID_NVM_VOICE_INPUT_REPORT_CONFIG_OFFSET             (2)
#define HID_NVM_MOTION_REPORT_CONFIG_OFFSET                  (3)
#define HID_NVM_MOUSE_REPORT_CONFIG_OFFSET                   (4)

/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void handleControlPointUpdate(hid_control_point_op control_op);
#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
static void tempDisableConnectionLatency(void);
static void enableConnectionLatency(timer_id tid);
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */

/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------
 *  NAME
 *      handleControlPointUpdate
 *
 *  DESCRIPTION
 *      This function is used to handle HID Control Point updates
 *----------------------------------------------------------------------------*/
static void handleControlPointUpdate(hid_control_point_op control_op)
{
    switch(control_op)
    {
        case hid_suspend:
        {
             hid_data.suspended = TRUE;
#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
             enableConnectionLatency(TIMER_INVALID);
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */

             /* Host has suspended its operations, application may like 
              * to do a low frequency key scan. The sample application is 
              * not doing any thing special in this case other than not sending
              * a connection parameter update request if the remote host is
              * currently suspended.
              */
        }
        break;

        case hid_exit_suspend:
        {
             hid_data.suspended = FALSE;

             /* Host has exited suspended mode, application may like 
              * to do a normal frequency key scan. The sample application is 
              * not doing any thing special in this case.
              */
        }
        break;

        default:
        {
            /* Ignore invalid value */
        }
        break;
    }
}

#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
static void tempDisableConnectionLatency()
{
    if(latency_suspension_timer != TIMER_INVALID)
    {
        TimerDelete(latency_suspension_timer);
    }
    else
    {
        LsDisableSlaveLatency(TRUE);
    }
    latency_suspension_timer = TimerCreate(CONNECTION_LATENCY_DISABLE_TIMEOUT, 
                                           TRUE, 
                                           enableConnectionLatency);
}

static void enableConnectionLatency(timer_id tid)
{
    if(latency_suspension_timer != TIMER_INVALID)
    {
        TimerDelete(latency_suspension_timer);
    }
    latency_suspension_timer = TIMER_INVALID;
    LsDisableSlaveLatency(FALSE);
}
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise HID service data 
 *      structure.
 *----------------------------------------------------------------------------*/
extern void HidDataInit(void)
{
    if(localData.bonded == FALSE)
    {
        /* Initialise all the Input Report Characteristic Client Configuration 
         * only if device is not bonded
         */
        
        hid_data.consumer_client_config = gatt_client_config_none;
        /* Update the NVM  with the same. */
        Nvm_Write((uint16*)&hid_data.consumer_client_config,
                  sizeof(gatt_client_config), 
                  hid_data.nvm_offset + HID_NVM_CONSUMER_REPORT_CONFIG_OFFSET);
        


    }

    /* Default to Report Mode */
    hid_data.suspended = FALSE;
	
}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles Read operation on HID service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *----------------------------------------------------------------------------*/
extern void HidHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 2;  /* in bytes */
    uint8  *p_value = NULL;
    uint8  val[HID_KEYPRESS_DATA_LENGTH];
    uint16 client_config = gatt_client_config_none;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {
        case HANDLE_HID_REPORT_MAP:
            /*Set data pointer to the start of next chunk of descriptor*/
            p_value = hidDescriptor + p_ind->offset;
            /*Set length to the remaining number of bytes to send*/
            length = sizeof(hidDescriptor) - p_ind->offset;
            break;
        




        case HANDLE_HID_CONSUMER_REPORT_CLIENT_CONFIG:
            client_config = hid_data.consumer_client_config;
            break;

        case HANDLE_HID_CONSUMER_REPORT:
            /* Remote device is reading the last input report */
            p_value = localData.latest_button_report;
            length = ATTR_LEN_HID_CONSUMER_REPORT;
            break;
            
        default:
            /* Let firmware handle the request */
            rc = gatt_status_irq_proceed;
            break;

    }
    
    if((rc == sys_status_success) &&
       (p_value == NULL))
    {
        /* write the characteristic value to the buffer */
        p_value = val;

        BufWriteUint16(&p_value, client_config);
        length = 2; /* 2-bytes */

        /* BufWriteUint16 increments p_value. Revert it back */
        p_value -= length;
    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc, length, p_value);
}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on HID service attributes 
 *      maintained by the application.and responds with the GATT_ACCESS_RSP 
 *      message.
 *----------------------------------------------------------------------------*/
extern void HidHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    uint16 client_config;
    uint16 offset = 0;
    uint8 *p_value = p_ind->value;
    sys_status rc = sys_status_success;
    gatt_client_config* client_config_ptr = NULL;

#if defined(ENABLE_IGNORE_CL_ON_OUTPUT_HID)
    tempDisableConnectionLatency();
#endif /* ENABLE_IGNORE_CL_ON_OUTPUT_HID */
    switch(p_ind->handle)
    {




        case HANDLE_HID_CONSUMER_REPORT_CLIENT_CONFIG:
            client_config_ptr = &hid_data.consumer_client_config;
            offset = HID_NVM_CONSUMER_REPORT_CONFIG_OFFSET;
            break;

            


        case HANDLE_HID_CONTROL_POINT:
            {
                uint8 control_op = BufReadUint8(&p_value);
                handleControlPointUpdate(control_op);
            }
            break;

        default:
            /* Other characteristics in HID don't support the "write" property */
            rc = gatt_status_write_not_permitted;
            break;
    }
    
    if(client_config_ptr != NULL)
    {
        /* process client configurations on HID reports */
        client_config = BufReadUint16(&p_value);

        if((client_config == gatt_client_config_notification) ||
           (client_config == gatt_client_config_none))
        {
            /* store the new client configuration */
            *client_config_ptr = client_config;

            /* offset to the start of the HID NVM area */
            offset += hid_data.nvm_offset;

            /* update the NVM */
            Nvm_Write(&client_config, sizeof(gatt_client_config), offset);
        }
        else
        {
            /* HID report supports only notifications */
            rc = gatt_status_app_mask;
        }
    }

    /* Send ACCESS RESPONSE */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);

}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidIsNotifyEnabledOnReportId
 *
 *  DESCRIPTION
 *      This function returns whether notifications are enabled on CCCD of
 *      reports specified by 'report_id'.
 *
 *  RETURNS/MODIFIES
 *      TRUE/FALSE: Notification configured or not
 *
 *----------------------------------------------------------------------------*/
extern bool HidIsNotifyEnabledOnReportId(uint8 report_id)
{
    bool result = FALSE;
    
    switch(report_id)
    {
        case HID_CONSUMER_REPORT_ID:
            result = (hid_data.consumer_client_config == gatt_client_config_notification);
            break;
            
        default:
            break;
    }

    return result;
}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidSendInputReport
 *
 *  DESCRIPTION
 *      This function is used to notify key presses to connected host.
 *----------------------------------------------------------------------------*/
extern void HidSendInputReport(uint8 report_id, uint8 *report, bool force_send)
{
    switch(report_id)
    {
        case HID_CONSUMER_REPORT_ID:
            if(force_send)
            {
                notificationForceBufferItem(HANDLE_HID_CONSUMER_REPORT, 
                                            ATTR_LEN_HID_CONSUMER_REPORT, 
                                            (uint16*)report);
            }
            else
            {
                notificationBufferItem(HANDLE_HID_CONSUMER_REPORT, 
                                       ATTR_LEN_HID_CONSUMER_REPORT, 
                                       (uint16*)report);
            }
            break;

    }
}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read HID service specific data store in NVM
 *----------------------------------------------------------------------------*/
extern void HidReadDataFromNVM(bool bonded, uint16 *p_offset)
{
    hid_data.nvm_offset = *p_offset;

    /* Read NVM only if devices are bonded */
    if(bonded)
    {
        /* Read CONSUMER Report Client Configuration */
        Nvm_Read((uint16*)&hid_data.consumer_client_config,
                 sizeof(gatt_client_config),
                 hid_data.nvm_offset + HID_NVM_CONSUMER_REPORT_CONFIG_OFFSET);
        



        
                 
    }
    
    /* Increment the offset by the number of words of NVM memory required 
     * by HID service 
     */
    *p_offset += HID_SERVICE_NVM_MEMORY_WORDS;

}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the HID 
 *      service
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/
extern bool HidCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_HID_SERVICE) &&
            (handle <= HANDLE_HID_SERVICE_END))
            ? TRUE : FALSE;
}

/*-----------------------------------------------------------------------------
 *  NAME
 *      HidIsStateSuspended
 *
 *  DESCRIPTION
 *      This function is used to check if the HID host has entered suspend state
 *
 *  RETURNS/MODIFIES
 *      Boolean - TRUE if the remote device is in suspended state.
 *
 *----------------------------------------------------------------------------*/
 extern bool HidIsStateSuspended(void)
{
    return hid_data.suspended;
}

