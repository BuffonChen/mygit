/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    advertise.c
 *
 *  DESCRIPTION
 *    This file contains functions that control advertising.
 *
 ******************************************************************************/
/*=============================================================================
 *  SDK Header Files
 *============================================================================*/

#include <gap_app_if.h>
#include <gatt.h>

#include "configuration.h"

#include <mem.h>
#ifdef __GAP_PRIVACY_SUPPORT__
#include <security.h>
#endif /* __GAP_PRIVACY_SUPPORT__ */

/*=============================================================================
 *  Local header Files
 *============================================================================*/
#include "advertise.h"
#include "remote.h"
#include "state.h"
#include "appearance.h"
#include "uuids_hid.h"
#include "service_gap.h"
#include "remote_gatt.h"


/*=============================================================================
 *  Private Function Definitions
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      getSupported16BitUUIDServiceList
 *
 *  DESCRIPTION
 *      This function prepares the list of supported 16-bit service UUIDs to be 
 *      added to Advertisement data. It also adds the relevant AD Type to the 
 *      starting of AD array.
 *
 *  RETURNS/MODIFIES
 *      Return the size AD Service UUID data.
 *
 *----------------------------------------------------------------------------*/

static uint16 getSupported16BitUUIDServiceList(uint8 *p_service_uuid_ad)
{
    uint16 i = 0;

    /* Add 16-bit UUID for Standard HID service  */
    p_service_uuid_ad[i++] = AD_TYPE_SERVICE_UUID_16BIT_LIST;

    p_service_uuid_ad[i++] = LE8_L(HID_SERVICE_UUID);
    p_service_uuid_ad[i++] = LE8_H(HID_SERVICE_UUID);

    return (i);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      addDeviceNameToAdvData
 *
 *  DESCRIPTION
 *      This function is used to add the device name to the advertising-
 *      or scan-response packet.
 *      It tries the following:
 *      a. Try to add complete device name to the advertisment packet
 *      b. Try to add complete device name to the scan response packet
 *      c. Try to add shortened device name to the advertisement packet
 *      d. Try to add shortened (max possible) device name to the scan 
 *         response packet
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void addDeviceNameToAdvData(uint16 adv_data_len, uint16 scan_data_len)
{
    uint8 *p_device_name;
    uint16 device_name_adtype_len;

    /* Read device name along with AD Type and its length */
    p_device_name = GapGetNameAndLength(&device_name_adtype_len);

    /* Assume we can add the complete device name to Advertisement data.
     * This will be corrected (overwritten) below if the complete
     * name does not fit.
     */
    p_device_name[0] = AD_TYPE_LOCAL_NAME_COMPLETE;

    /* Increment device_name_length by one to account for length field
     * which will be added by the GAP layer. 
     */

    /* Determine whether the complete device name can fit into
     * the remaining space in the advertising packet.
     */
    if((device_name_adtype_len + 1) <= (MAX_ADV_DATA_LEN - adv_data_len))
    {
        /* Add Complete Device Name to Advertisement Data */
        (void)LsStoreAdvScanData(device_name_adtype_len, p_device_name, ad_src_advertise);
    }
    /* Determine whether the complete device name can fit into
     * the remaining space in the scan response message.
     */
    else if((device_name_adtype_len + 1) <= (MAX_ADV_DATA_LEN - scan_data_len)) 
    {
        /* Add Complete Device Name to Scan Response Data */
        (void)LsStoreAdvScanData(device_name_adtype_len, p_device_name, ad_src_scan_rsp);
    }
    /* Determine whether the shortened device name can fit into
     * the remaining space in the advertising packet.
     */
    else if((MAX_ADV_DATA_LEN - adv_data_len) >=
            (SHORTENED_DEV_NAME_LEN + 2)) /* Added 2 for Length and AD type
                                           * added by GAP layer
                                           */
    {
        /* Record that the advertising packet contains a shortened name. */
        p_device_name[0] = AD_TYPE_LOCAL_NAME_SHORT;

        (void)LsStoreAdvScanData(SHORTENED_DEV_NAME_LEN, p_device_name, ad_src_advertise);
    }
    else /* Add the shortened device name to the scan-response message. */
    {
        /* Record that the scan-response message contains a shortened name. */
        p_device_name[0] = AD_TYPE_LOCAL_NAME_SHORT;

        (void)LsStoreAdvScanData((MAX_ADV_DATA_LEN - scan_data_len), p_device_name, ad_src_scan_rsp);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      setAdvertisingParameters
 *
 *  DESCRIPTION
 *      This function is used to set advertising parameters.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *  NOTES:
 *  Add device name in the end so that either full name or partial name is added
 *  to AdvData or scan data depending upon the space left in the two fields.
 *----------------------------------------------------------------------------*/

static void setAdvertisingParameters(bool fast_connection, gap_mode_connect connect_mode)
{
    uint8 advert_data[MAX_ADV_DATA_LEN];
    uint16 length;
    uint32 adv_interval_min = RP_ADVERTISING_INTERVAL_MIN;
    uint32 adv_interval_max = RP_ADVERTISING_INTERVAL_MAX;
    int8 tx_power_level; /* Unsigned value */
    
    TYPED_BD_ADDR_T temp_addr;

    if(connect_mode == gap_mode_connect_directed)
    {
#ifdef __GAP_PRIVACY_SUPPORT__
        temp_addr.type = ls_addr_type_random;
        MemCopy(&temp_addr.addr, GapGetReconnectionAddress(), sizeof(BD_ADDR_T));
#else  /* __GAP_PRIVACY_SUPPORT__ */
        temp_addr.type = ls_addr_type_public;
        MemCopy(&temp_addr.addr, &(localData.bonded_bd_addr.addr), sizeof(BD_ADDR_T));
#endif /* __GAP_PRIVACY_SUPPORT__ */
    }

    /* A variable to keep track of the data added to AdvData. The limit is
     * MAX_ADV_DATA_LEN. GAP layer will add AD Flags to AdvData which is 3
     * bytes. Refer BT Spec 4.0, Vol 3, Part C, Sec 11.1.3.
     */
    uint16 length_added_to_adv = 3;
    uint16 length_added_to_scan = 0;
    
    /* Tx power level value prefixed with 'Tx Power' AD Type */
    uint8 device_tx_power[TX_POWER_VALUE_LENGTH] = {
                AD_TYPE_TX_POWER};
    
    uint8 device_appearance[ATTR_LEN_DEVICE_APPEARANCE + 1] = {
                AD_TYPE_APPEARANCE,
                LE8_L(APPEARANCE_REMOTE_VALUE),
                LE8_H(APPEARANCE_REMOTE_VALUE)
        };

    if(fast_connection)
    {
        adv_interval_min = FC_ADVERTISING_INTERVAL_MIN;
        adv_interval_max = FC_ADVERTISING_INTERVAL_MAX;
    }

    (void)GapSetMode(gap_role_peripheral, 
                     gap_mode_discover_general,
                     connect_mode,
                     gap_mode_bond_yes,
                     gap_mode_security_unauthenticate);
    
    /* Reset existing advertising data as application is again going to add
     * data for undirected advertisements.
     */
    (void)LsStoreAdvScanData(0, NULL, ad_src_advertise);

    /* Reset existing scan response data as application is again going to add
     * data for undirected advertisements.
     */
    (void)LsStoreAdvScanData(0, NULL, ad_src_scan_rsp);    
    
    if(connect_mode == gap_mode_connect_directed)
    {
        GapSetAdvAddress(&temp_addr);
    }
    else
    {
        /* Advertisement interval will be ignored for directed advertisement */
        (void)GapSetAdvInterval(adv_interval_min, adv_interval_max);

        /* Set up the advertising data. The GAP layer will automatically add the
         * AD Flags field so all we need to do here is add 16-bit supported 
         * services UUID and Complete Local Name type. Applications are free to
         * add any other AD types based upon the profile requirements and
         * subject to the maximum AD size of 31 octets.
         */

        /* Add 16-bit UUID list of the services supported by the device */
        length = getSupported16BitUUIDServiceList(advert_data);

        /* Before adding data to the ADV_IND, increment 'lengthAddedToAdv' to
         * keep track of the total number of bytes added to ADV_IND. At the end
         * while adding the device name to the ADV_IND, this can be used to
         * verify whether the complete name can fit into the AdvData adhering to
         * the limit of 31 octets. In addition to the above populated fields, a
         * 'length' field will also be added to AdvData by GAP layer. Refer
         * BT 4.0 spec, Vol 3, Part C, Figure 11.1.
         */
        length_added_to_adv += (length + 1);
        (void)LsStoreAdvScanData(length, advert_data, ad_src_advertise);

        length_added_to_adv += (sizeof(device_appearance) + 1);
        /* Add device appearance as advertisement data */
        (void)LsStoreAdvScanData(sizeof(device_appearance), device_appearance, ad_src_advertise);

        /* If 128-bit proprietary service UUID is added to the advData, there
         * will be no space left for any more data. So don't add the
         * TxPowerLevel to the advData.
         */

        /* Read tx power of the chip */
        (void)LsReadTransmitPowerLevel(&tx_power_level);

        /* Add the read tx power level to g_device_tx_power 
         * Tx power level value is of 1 byte 
         */
        device_tx_power[TX_POWER_VALUE_LENGTH - 1] = (uint8)tx_power_level;

        length_added_to_scan += TX_POWER_VALUE_LENGTH + 1;
        
        /* Add tx power value of device to the scan response data */
        (void)LsStoreAdvScanData(TX_POWER_VALUE_LENGTH, device_tx_power, ad_src_scan_rsp);
       
        addDeviceNameToAdvData(length_added_to_adv, length_added_to_scan);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      advertisingTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to stop on-going advertisements at the expiry of 
 *      SLOW_CONNECTION_ADVERT_TIMEOUT_VALUE or 
 *      FAST_CONNECTION_ADVERT_TIMEOUT_VALUE timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
static void advertisingTimerHandler(timer_id tid)
{
    /* Based upon the timer id, stop on-going advertisements */
    if(localData.advertising_tid == tid)
    {
        /* Stop on-going advertisements */
        GattCancelConnectReq();
    }
    
    localData.advertising_tid = TIMER_INVALID;
}

/*=============================================================================
 *  Public Function Definitions
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AdvStart
 *
 *  DESCRIPTION
 *      This function is used to start undirected advertisements.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
extern void AdvStart(bool fast_connection, gap_mode_connect connect_mode)
{
    uint16 connect_flags = L2CAP_CONNECTION_SLAVE_UNDIRECTED | 
                          L2CAP_OWN_ADDR_TYPE_PUBLIC;
    uint32 timeout;

    /* Set UCID to INVALID_UCID */
    localData.st_ucid = GATT_INVALID_UCID;

    /* Set advertisement parameters */
    setAdvertisingParameters(fast_connection, connect_mode);

    /* If white list is enabled, set the controller's advertising filter policy 
     * to "process scan and connection requests only from devices in the White 
     * List"
     */
    if(localData.bonded && 
        !IsAddressResolvableRandom(&localData.bonded_bd_addr))
    {
        connect_flags = (L2CAP_CONNECTION_SLAVE_WHITELIST | L2CAP_OWN_ADDR_TYPE_PUBLIC);
        
        if(connect_mode == gap_mode_connect_directed)
        {
            connect_flags |= L2CAP_CONNECTION_SLAVE_DIRECTED;
        }
    }

#ifdef __GAP_PRIVACY_SUPPORT__

    /* Check whether privacy is enabled */
    if(GapIsPeripheralPrivacyEnabled())
    {
        if(connect_mode == gap_mode_connect_directed)
        {
            /* Set advAddress to reconnection address. */
            GapSetRandomAddress(GapGetReconnectionAddress());

            connect_flags = L2CAP_CONNECTION_SLAVE_DIRECTED | 
                            L2CAP_OWN_ADDR_TYPE_RANDOM | 
                            L2CAP_PEER_ADDR_TYPE_RANDOM;
        }
        else
        {
            /* Generate Resolvable random address and use it as advAddress. */
            SMPrivacyRegenerateAddress(NULL);

            /* Change to random address */
            connect_flags |= L2CAP_OWN_ADDR_TYPE_RANDOM;
        }
    }
#endif /* __GAP_PRIVACY_SUPPORT__ */

    /* Start GATT connection in Slave role */
    GattConnectReq(NULL, connect_flags);

     /* Start advertisement timer */
    if(connect_mode == gap_mode_connect_undirected)
    {
        /* Set the timeout value according to the "fast_connection" parameter */
        timeout = (fast_connection) 
            ? FAST_CONNECTION_ADVERT_TIMEOUT_VALUE 
            : SLOW_CONNECTION_ADVERT_TIMEOUT_VALUE;

        /* Re-start advertisement timer  */
        TimerDelete(localData.advertising_tid);
        localData.advertising_tid = TimerCreate(timeout, TRUE, advertisingTimerHandler);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AdvStop
 *
 *  DESCRIPTION
 *      This function is used to stop advertising.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
extern void AdvStop(void)
{
    GattCancelConnectReq();
}
