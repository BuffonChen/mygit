/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *      remote_gatt.c
 *
 * DESCRIPTION
 *      Implementation of the remote GATT-related routines
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt.h>
#include <gatt_prim.h>
#include <att_prim.h>
#include <gap_app_if.h>
#include <ls_app_if.h>
#include <mem.h>
#include <security.h>
#include <time.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/
#include "configuration.h"

#include "app_gatt.h"
#include "remote.h"
#include "app_gatt_db.h"
#include "remote_gatt.h"
#include "nvm_access.h"
#include "appearance.h"
#include "gap_conn_params.h"

#include "uuids_gap.h"
#include "uuids_hid.h"
#include "uuids_battery.h"
#include "uuids_scan_params.h"
#include "uuids_dev_info.h"

#include "service_gap.h"
#include "service_hid.h"
#include "service_battery.h"
#include "service_csr_ota.h"
#include "service_gatt.h"


/*=============================================================================*
 *  Private Definitions
 *============================================================================*/


/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/
static void handleAccessRead(GATT_ACCESS_IND_T *p_ind);
static void handleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/


/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleAccessRead
 *
 *  DESCRIPTION
 *      This function handles Read operation on attributes (as received in 
 *      GATT_ACCESS_IND message) maintained by the application and respond 
 *      with the GATT_ACCESS_RSP message.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

static void handleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    /* Check all the services that support attribute 'Read' operation handled
     * by application
     */

    if(GapCheckHandleRange(p_ind->handle))
    {
        GapHandleAccessRead(p_ind);
    }
    else if(HidCheckHandleRange(p_ind->handle))
    {
        HidHandleAccessRead(p_ind);
    }
    else if(BatteryCheckHandleRange(p_ind->handle))
    {
        BatteryHandleAccessRead(p_ind);
    }
    else if(OtaCheckHandleRange(p_ind->handle))
    {
        OtaHandleAccessRead(p_ind);
    }
    else if(GattCheckHandleRange(p_ind->handle))
    {
        GattHandleAccessRead(p_ind);
    }
    else
    {
        GattAccessRsp(p_ind->cid, p_ind->handle, 
                      gatt_status_read_not_permitted,
                      0, NULL);
    }

}

/*-----------------------------------------------------------------------------
 *  NAME
 *      handleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on attributes (as received in 
 *      GATT_ACCESS_IND message) maintained by the application.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/
static void handleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    /* Check all the services that support attribute 'Write' operation handled
     * by application
     */
    if(GapCheckHandleRange(p_ind->handle))
    {
        GapHandleAccessWrite(p_ind);
    }
    else if(HidCheckHandleRange(p_ind->handle))
    {
        HidHandleAccessWrite(p_ind);
    }
    else if(BatteryCheckHandleRange(p_ind->handle))
    {
        BatteryHandleAccessWrite(p_ind);
    }
    else if(OtaCheckHandleRange(p_ind->handle))
    {
        OtaHandleAccessWrite(p_ind);
    }
    else if(GattCheckHandleRange(p_ind->handle))
    {
        GattHandleAccessWrite(p_ind);
    }
    else
    {
        GattAccessRsp(p_ind->cid, p_ind->handle, 
                      gatt_status_write_not_permitted,
                      0, NULL);
    }

}


/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      GattHandleAccessInd
 *
 *  DESCRIPTION
 *      This function handles GATT_ACCESS_IND message for attributes maintained
 *      by the application.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void GattHandleAccessInd(GATT_ACCESS_IND_T *p_ind)
{
    if(p_ind->flags == (ATT_ACCESS_WRITE | 
                        ATT_ACCESS_PERMISSION | 
                        ATT_ACCESS_WRITE_COMPLETE))
    {
        handleAccessWrite(p_ind);
    }
    else if(p_ind->flags == (ATT_ACCESS_READ | 
                             ATT_ACCESS_PERMISSION))
    {
        handleAccessRead(p_ind);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      IsAddressResolvableRandom
 *
 *  DESCRIPTION
 *      This function checks if the address is resolvable random or not.
 *
 *  RETURNS/MODIFIES
 *      Boolean - True (Resolvable Random Address) /
 *                     False (Not a Resolvable Random Address)
 *
 *----------------------------------------------------------------------------*/

extern bool IsAddressResolvableRandom(TYPED_BD_ADDR_T *addr)
{
    if (addr->type != L2CA_RANDOM_ADDR_TYPE || 
        (addr->addr.nap & BD_ADDR_NAP_RANDOM_TYPE_MASK)
                                      != BD_ADDR_NAP_RANDOM_TYPE_RESOLVABLE)
    {
        /* This isn't a resolvable private address... */
        return FALSE;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      IsAddressNonResolvableRandom
 *
 *  DESCRIPTION
 *      This function checks if the address is non-resolvable random
 *      (reconnection address) or not.
 *
 *  RETURNS/MODIFIES
 *      Boolean - True (Non-resolvable Random Address) /
 *                     False (Not a Non-resolvable Random Address)
 *
 *----------------------------------------------------------------------------*/

extern bool IsAddressNonResolvableRandom(TYPED_BD_ADDR_T *addr)
{
    if (addr->type != L2CA_RANDOM_ADDR_TYPE || 
        (addr->addr.nap & BD_ADDR_NAP_RANDOM_TYPE_MASK)
                                      != BD_ADDR_NAP_RANDOM_TYPE_NONRESOLV)
    {
        /* This isn't a non-resolvable private address... */
        return FALSE;
    }
    return TRUE;
}
