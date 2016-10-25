/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    event_handler.c
 *
 *  DESCRIPTION
 *    This file handler funtions for different events
 *
 ******************************************************************************/

/*=============================================================================
 *  SDK Header Files
 *============================================================================*/
#include <gatt.h>
#include <security.h>
#include <mem.h>
#include <pio.h>
#include <csr_ota.h> 
 
/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "remote.h"
#include "nvm_access.h"
#include "remote_gatt.h"
#include "event_handler.h"
#include "advertise.h"
#include "key_scan.h"
#include "notifications.h"
#include "service_gap.h"
#include "service_hid.h"
#include "service_battery.h"
#include "service_gatt.h"
#include "service_csr_ota.h"
#include "motion.h"
#include "mouse.h"

/*=============================================================================
 *  Private Definitions
 *============================================================================*/
/* Time after which a L2CAP connection parameter update request will be
 * re-sent upon failure of an earlier sent request.
 * Due to using the watchdog clock (with a 15-second resolution) as the time 
 * source, the actual timeout value could be up to 15-seconds later than the
 * time defined here.
 */
#define GAP_CONN_PARAM_TIMEOUT  (30 * SECOND)

/* Despite being bonded, some Central devices re-subscribe to notifications
 * each time connection is established. When this happens, queued messages
 * (notifications) can be lost.
 * To overcome this, we use a simple timer that allows enough time for
 * reconnection and service configuration before sending any queued notifications.
 */
#define NOTIFICATION_DELAY_AFTER_RECONNECTION      (200*MILLISECOND)

/*=============================================================================
 *  Private Data
 *============================================================================*/


/*=============================================================================
 *  Private Function definitions
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      requestConnParamUpdate
 *
 *  DESCRIPTION
 *      This function is used to send L2CAP_CONNECTION_PARAMETER_UPDATE_REQUEST
 *      to the remote device when an earlier sent request had failed.
 *----------------------------------------------------------------------------*/
static void requestConnParamUpdate(void)
{
    ble_con_params remote_pref_conn_params;
    
    /* Send a connection parameter update request only if the remote device
     * has not entered 'suspend' state.
     */
    if(HidIsStateSuspended() == FALSE)
    {
        if((localData.actual_latency > PREFERRED_SLAVE_LATENCY) ||
           (localData.actual_timeout > PREFERRED_SUPERVISION_TIMEOUT) ||
           (localData.actual_interval > PREFERRED_MAX_CON_INTERVAL))
        {
            remote_pref_conn_params.con_max_interval =
                                        PREFERRED_MAX_CON_INTERVAL;
            remote_pref_conn_params.con_min_interval =
                                        PREFERRED_MIN_CON_INTERVAL;
            remote_pref_conn_params.con_slave_latency =
                                        PREFERRED_SLAVE_LATENCY;
            remote_pref_conn_params.con_super_timeout =
                                        PREFERRED_SUPERVISION_TIMEOUT;

            (void)LsConnectionParamUpdateReq(&(localData.con_bd_addr), 
                                             &remote_pref_conn_params);
            localData.conn_param_update_count++;
            localData.conn_param_counter_active = FALSE;
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      SendNextInputReport
 *
 *  DESCRIPTION
 *      If more motion data is available, send it now. Otherwise, drop out
 *      of MOTION state into IDLE state.
 *----------------------------------------------------------------------------*/
static void SendNextInputReport(timer_id tid)
{
    localData.next_report_timer_id = TIMER_INVALID;

    if(localData.state & STATE_CONNECTED_MOTION)
    {
        /* Reception of radio_event_tx_data event indicates successful
         * transmission of data to the remote device. This should have freed
         * up some buffer space in f/w for transmitting another mouse input 
         * report.
         * If the radio event indicating a data packet transmission is 
         * received in any other states, ignore the event
         */
        if(0)
        {
            
            /* Create a new timer close to the connection interval */
            handleCreateReportTimer();
        }
        else
        {
            /* Notifications are not enabled, switch back to the CONNECTED_IDLE state.*/
            stateSet(STATE_CONNECTED_IDLE);
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleBondingChanceTimerExpiry
 *
 *  DESCRIPTION
 *      
 *----------------------------------------------------------------------------*/
static void handleBondingChanceTimerExpiry(timer_id tid)
{
    localData.recrypt_tid = TIMER_INVALID;
    
    /* The bonding chance timer has expired. This means the remote device
     * has not encrypted the link using old keys. Disconnect the link.
     */
    stateSet(STATE_DISCONNECTING);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      notificationConnectionDelay
 *
 *  DESCRIPTION
 *      Despite being bonded, some Central devices re-subscribe to notifications
 *      each time connection is established. When this happens, queued messages
 *      (notifications) can be lost.
 *      When this timer fires (this function is called) it is now safe to send
 *      notifications again.
 *----------------------------------------------------------------------------*/
static void notificationConnectionDelay(timer_id tid)
{
    localData.blockNotifications = FALSE;
    
    /* Send the first queued notificaton (if any) */
    notificationSendNext();
}

/*=============================================================================
 *  Public Function definitions
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleCreateReportTimer
 *
 *  DESCRIPTION
 *      Create the timer used to trigger transmission of motion data.
 *
 *----------------------------------------------------------------------------*/
extern void handleCreateReportTimer(void)
{
    /* The remove-control application has to send one notification per connection 
     * interval. The sensor poll timer is started upon receiving 'radio_event_first_tx'. 
     * The application has to put data to the firmware queue 1.8 ms before the start
     * of the next connection interval. Once the timer expires, the data is read
     * from the sensors and then sent to the firmware to be sent to the connected
     * device. Reading the sensor data takes 300 microseconds/0.3ms. So, the sensor
     * poll period should expire 2.1 ms before the start of the next connection
     * interval.
     *
     * For CSR100x the time taken for the application to receive the 
     * 'radio_event_first_tx' has been observed to be 900 microseconds after the 
     * start of the connection event. To ensure that the sensor poll timer expires 
     * only once in a connection interval, the timer should start after the
     * application receives 'radio_event_first_tx' and expire 2.1ms before the next
     * connection event starts. The sum of these two periods gives 3000
     * microseconds. This period should be subtracted from the value of connection
     * interval period to get the value of the sensor poll timer to be used.
     *
     * For CSR101x, it's observed that the 'radio_event_first_tx' arrives at the
     * beginning of the connection interval and a value of 450 micro seconds has
     * been observed instead of 900 microseconds in case of Robinson. Hence, the
     * residual time has been calibrated for Baldrick with a value 2550 to save 
     * power.
     *
     * NOTE: The value of 900 micro seconds above has been found for the
     * case when the longest report is of 6 bytes (48 bits) length.  If the
     * largest report increases one bit, then this value has to be increased
     * by 1 micro second, that is, if the largest mouse report is of length 8 bytes,
     * then this value should be 3016.
     */
#if defined(CSR101x_A05)
#define RESIDUAL_TIME                2550
#elif defined(CSR100x)
#define RESIDUAL_TIME                3000
#endif

    /* Delete the timer if running */
    if(localData.next_report_timer_id != TIMER_INVALID)
    {
        TimerDelete(localData.next_report_timer_id);
    }

    /* Create a new timer close to the connection interval. 
     * 1250UL is the slots-to-ms conversion factor
     */    
    localData.next_report_timer_id = 
        TimerCreate(((localData.actual_interval * 1250UL) - RESIDUAL_TIME), 
                    TRUE, 
                    SendNextInputReport);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleClearPairing
 *
 *  DESCRIPTION
 *      This function handles clearing existing pairing information.
 *----------------------------------------------------------------------------*/
extern void handleClearPairing(void)
{
    localData.bonded = FALSE;
    
    /* If the device was bonded earlier, remove it from whitelist */
    AppUpdateWhiteList();
    
    /* We are not paired, so encryption cannot be enabled */
    localData.encrypt_enabled = FALSE;

    /* Record the updated bonded status in the NVM */
    Nvm_Write((uint16*)&localData.bonded,
              sizeof(localData.bonded), 
              NVM_OFFSET_BONDED_FLAG);
    
    /* Re-initialise service data */
    GapDataInit();
    HidDataInit();
    BatteryDataInit();
}

#if defined(DISCONNECT_ON_IDLE)
/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleResetIdleTimer
 *
 *  DESCRIPTION
 *      
 *----------------------------------------------------------------------------*/
extern void handleResetIdleTimer(void)
{
    localData.disconnect_counter = 0;
}
#endif /* DISCONNECT_ON_IDLE */

/*-----------------------------------------------------------------------------*
 *  NAME
 *      idleTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to handle IDLE timer expiry in connected states.
 *      
 *----------------------------------------------------------------------------*/
extern void handleBackgroundTickInd(void)
{
    /* A tick is received every 15 seconds. Assuming the first tick was received
     * exactly 15 seconds after the last reset event, the number of ticks
     * required to measure idle time would be timeout value / 15 seconds.
     * However, we do not know when the first tick will arrive (*up to* 15 secs),
     * so add 1 to the result and be a little over rather than a little under.
     */
    const uint8 connectionParamUpdateTicks = (GAP_CONN_PARAM_TIMEOUT / (15*SECOND)) + 1;
#if defined(DISCONNECT_ON_IDLE)
    const uint8 disconnectionTimeoutTicks = (CONNECTED_IDLE_TIMEOUT_VALUE / (15*SECOND)) + 1;
    
    /* At the expiry of this timer (enough ticks have been received and we are
     * in CONNECTED IDLE state), the application shall disconnect from the 
     * host and shall move to IDLE state.
     */
    localData.disconnect_counter++;
    
    if(localData.disconnect_counter >= disconnectionTimeoutTicks)
    {
        if(localData.state == STATE_CONNECTED_IDLE)
        {
            /* We don't seem to be doing much useful, so disconnect now */
            stateSet(STATE_DISCONNECTING);
        }
        else
        {
            handleResetIdleTimer(); 
        }
    }
#endif /* DISCONNECT_ON_IDLE */
    
    /* If we are waiting to update the connection parameters and the counter
     * expires (i.e., enough ticks have been received), request a change to the
     * current connection parameters.
     */
    if(localData.conn_param_counter_active)
    {
        localData.conn_param_update_tick_count++;
        
        if(localData.conn_param_update_tick_count >= connectionParamUpdateTicks)
        {
            requestConnParamUpdate();
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAddDBCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ADD_DB_CFM
 *----------------------------------------------------------------------------*/
extern void handleSignalGattAddDBCfm(GATT_ADD_DB_CFM_T *event_data)
{
    /* Handling signal as per current state */
    if( (localData.state == STATE_INIT) &&
        (event_data->result == sys_status_success))
    {
        stateSet(STATE_ADVERTISING);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CONNECT_CFM
 *----------------------------------------------------------------------------*/
extern void handleSignalGattConnectCfm(GATT_CONNECT_CFM_T* event_data)
{
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_FAST_ADVERT:
        case STATE_SLOW_ADVERT:
        case STATE_DIRECT_ADVERT:
            if(event_data->result == sys_status_success)
            {
                /* Store received UCID */
                localData.st_ucid = event_data->cid;

                if(localData.bonded &&
                   (IsAddressResolvableRandom(&localData.bonded_bd_addr) &&
                    (SMPrivacyMatchAddress(&event_data->bd_addr,
                                           localData.central_device_irk.irk,
                                           MAX_NUMBER_IRK_STORED, 
                                           MAX_WORDS_IRK) < 0)))
                {
                    /* The application is bonded to a remote device using 
                     * resolvable random address and the application has failed 
                     * to resolve the remote device address to which we just 
                     * connected, so disconnect and start advertising again
                     */
                    stateSetDisconnect(ls_err_authentication);
                }
                else
                {
                    localData.con_bd_addr = event_data->bd_addr;
                    
                    if(localData.bonded)
                    {
                        GattOnConnection();
                    }

                    /* Security supported by the remote HID host */
                    
                    /* Request Security only if remote device address is not
                     * resolvable random.
                     */
                    if(IsAddressResolvableRandom(&localData.con_bd_addr) == FALSE)
                    {
                        SMRequestSecurityLevel(&localData.con_bd_addr);    
                    }

                    /* We are now connected, but not doing anything */
                    stateSet(STATE_CONNECTED_IDLE);
                    
                    /* Trigger sending any buffered key-presses */
                    if(TimerCreate(NOTIFICATION_DELAY_AFTER_RECONNECTION, 
                                   TRUE, 
                                   notificationConnectionDelay) == TIMER_INVALID)
                    {
                        /* If the timer could not be created, unblock notifications
                         * now and don't worry about losing some.
                         */
                        localData.blockNotifications = FALSE;
                    }
                    
                }
            }
            else if(event_data->result == HCI_ERROR_DIRECTED_ADVERTISING_TIMEOUT)
            {
                /* Case where bonding has been removed when directed 
                 * advertisements were on-going
                 */
                if(localData.pairing_button_pressed)
                {
                    /* Reset and clear the whitelist */
                    handleClearPairing();
                }
                
                /* Drop any pending notifications */
                notificationDropAll();

                /* Trigger undirected advertisements as directed 
                 * advertisements have timed out.
                 */
                stateSet(STATE_FAST_ADVERT);
            }
            break;
        
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattCancelConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CANCEL_CONNECT_CFM
 *----------------------------------------------------------------------------*/
extern void handleSignalGattCancelConnectCfm(void)
{
    /* GATT_CANCEL_CONNECT_CFM is received when undirected advertisements
     * are stopped.
     */
    if (localData.state == STATE_FAST_ADVERT)
    {
        if(localData.pairing_button_pressed)
        {
            /* Reset and clear the whitelist */
            handleClearPairing();

            localData.pairing_button_pressed = FALSE;
            
            /* Stay in fast-advertising state.
             * Re-trigger advertising.
             */
            AdvStart(TRUE, gap_mode_connect_undirected);
        }
        else
        {
            /* Switch to slow advertising to save power */
            stateSet(STATE_SLOW_ADVERT);
        }
    }
    else if(localData.state == STATE_SLOW_ADVERT)
    {
        if(localData.pairing_button_pressed)
        {
            /* The user wants to re-pair this remote control. */
            
            /* Reset and clear the whitelist */
            handleClearPairing();

            localData.pairing_button_pressed = FALSE;
        
            /* Start fast advertising */
            stateSet(STATE_FAST_ADVERT);
        }
        else
        {
            /* Slow undirected advertisements have been stopped. Device 
             * shall move to IDLE state until next user activity or
             * pending notification.                     
             */
            stateSet(STATE_IDLE);
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAccessInd
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ACCESS_IND
 *----------------------------------------------------------------------------*/
extern void handleSignalGattAccessInd(GATT_ACCESS_IND_T *event_data)
{
    /* Handling signal as per current state */
    if(localData.state & STATE_CONNECTED)
    {
        GattHandleAccessInd(event_data);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmEvDisconnectComplete
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_DISCONNECT_COMPLETE.
 *----------------------------------------------------------------------------*/
extern void handleSignalLmEvDisconnectComplete(HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data)
{
    if(g_ota_reset_required)
    {
        /* Switch into OTA-update mode */
        OtaReset(); /* This call does not return */
    }
    

    /* Don't try to send notifications if we're not connected. */
    localData.blockNotifications = TRUE;


    
    /* Delete the bonding chance timer */
    TimerDelete(localData.recrypt_tid);
    localData.recrypt_tid = TIMER_INVALID;

    /* Handling signal as per current state */
    switch(localData.state)
    {
        /* LM_EV_DISCONNECT_COMPLETE is received by the application when
         * 1. The remote side initiates a disconnection. The remote side can
         *    disconnect in any of the following states:
         *    i. STATE_CONNECTED.
         *    ii. STATE_DISCONNECTING.
         * 2. The remote application itself initiates a disconnection. The
         *    remote application will have moved to STATE_DISCONNECTING state
         *    when it initiates a disconnection.
         * 3. There is a link loss.
         */
        case STATE_DISCONNECTING:
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
        
            /* Initialize the remote data structure. This will expect that the
             * remote side re-enables encryption on the re-connection though it
             * was a link loss.
             */
            RemoteDataInit();

            /* The remote needs to advertise after disconnection in the
             * following cases.
             * 1. If there was a link loss.
             * 2. If the application is not bonded to any host(A central device
             *    may have connected, read the device name and disconnected).
             * 3. If a new data is there in the queue.
             * Otherwise, it can move to STATE_IDLE state.
             */
            if((p_event_data->reason == HCI_ERROR_CONN_TIMEOUT) ||
               (localData.bonded == FALSE))
            {
                stateSet(STATE_ADVERTISING);
            }
            else
            {
                stateSet(STATE_IDLE);
            }
            break;
        
        default:
            /* Control should never reach here. */
            break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLMEncryptionChange
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_ENCRYPTION_CHANGE
 *----------------------------------------------------------------------------*/
extern void handleSignalLMEncryptionChange(LM_EVENT_T *event_data)
{
    HCI_EV_DATA_ENCRYPTION_CHANGE_T *pEvDataEncryptChange = &event_data->enc_change.data;
    
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
        
            if(pEvDataEncryptChange->status == sys_status_success)
            {
                localData.encrypt_enabled = pEvDataEncryptChange->enc_enable; 
                               
                if(localData.encrypt_enabled)
                {
                    /* Delete the bonding chance timer */
                    TimerDelete(localData.recrypt_tid);
                    localData.recrypt_tid = TIMER_INVALID;

                    /* Check whether the connection parameter update timer is running.
                     * If not, start it to trigger the Connection Parameter Update 
                     * procedure.
                     */
                    if(localData.conn_param_counter_active == FALSE)
                    {
                        /* Reset the number of connection parameter update attempts */
                        localData.conn_param_update_count = 0;
                        /* Start timer to trigger Connection Paramter Update procedure */
                        localData.conn_param_counter_active = TRUE;
                    } /* Else at the expiry of timer Connection parameter 
                       * update procedure will get triggered
                       */
                
                    /* Update battery status at every connection instance. It
                     * may not be worth updating timer more ofter, but again it 
                     * will primarily depend upon application requirements 
                     */
                    BatteryUpdateLevel(localData.st_ucid);

                }
            }
            break;

        default:
            break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmKeysInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_KEYS_IND
 *----------------------------------------------------------------------------*/
extern void handleSignalSmKeysInd(SM_KEYS_IND_T *event_data)
{
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
            /* Store the diversifier which will be used for accepting/rejecting
             * the encryption requests.
             */
            localData.diversifier = (event_data->keys)->div;
            
            /* Write the new diversifier to NVM */
            Nvm_Write(&localData.diversifier,
                      sizeof(localData.diversifier), NVM_OFFSET_SM_DIV);

            /* Store IRK if the connected host is using random resolvable 
             * address. IRK is used afterwards to validate the identity of 
             * connected host.
             */

            if(IsAddressResolvableRandom(&localData.con_bd_addr)) 
            {
                MemCopy(localData.central_device_irk.irk,
                        (event_data->keys)->irk,
                        MAX_WORDS_IRK);
                
                /* store IRK in NVM */
                Nvm_Write(localData.central_device_irk.irk, 
                          MAX_WORDS_IRK, 
                          NVM_OFFSET_SM_IRK);
            }
            break;
        
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmSimplePairingCompleteInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_SIMPLE_PAIRING_COMPLETE_IND
 *----------------------------------------------------------------------------*/
extern void handleSignalSmSimplePairingCompleteInd(SM_SIMPLE_PAIRING_COMPLETE_IND_T *event_data)
{
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
            if(event_data->status == sys_status_success)
            {
                localData.bonded = TRUE;
                localData.bonded_bd_addr = event_data->bd_addr;

                /* Store bonded host typed bd address to NVM */

                /* Write one word bonded flag */
                Nvm_Write((uint16*)&localData.bonded,
                           sizeof(localData.bonded), 
                           NVM_OFFSET_BONDED_FLAG);

                /* Write typed bd address of bonded host */
                Nvm_Write((uint16*)&localData.bonded_bd_addr,
                          sizeof(TYPED_BD_ADDR_T), 
                          NVM_OFFSET_BONDED_ADDR);

                /* White list is configured with the Bonded host address */
                AppUpdateWhiteList();

                /* Send an updated battery level to the remote device */
                BatteryUpdateLevel(localData.st_ucid);
            }
            else
            {
                /* Pairing has failed.
                 * 1. If pairing has failed due to repeated attempts, the 
                 *    application should immediately disconnect the link.
                 * 2. The application was bonded and pairing has failed.
                 *    Since the application was using whitelist, so the remote 
                 *    device has same address as our bonded device address.
                 *    The remote connected device may be a genuine one but 
                 *    instead of using old keys, wanted to use new keys. We 
                 *    don't allow bonding again if we are already bonded but we
                 *    will give some time to the connected device to encrypt the
                 *    link using the old keys. if the remote device encrypts the
                 *    link in that time, it's good. Otherwise we will disconnect
                 *    the link.
                 */
                 if(event_data->status == sm_status_repeated_attempts)
                 {
                    stateSet(STATE_DISCONNECTING);
                 }
                 else if(localData.bonded)
                 {
                    localData.encrypt_enabled = FALSE;
                    localData.recrypt_tid = TimerCreate(BONDING_CHANCE_TIMER,
                                                        TRUE, 
                                                        handleBondingChanceTimerExpiry);
                 }
            }
            break;
        
        default:
            break;
    }
}
/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmPairingAuthInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_PAIRING_AUTH_IND
 *----------------------------------------------------------------------------*/
extern void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *ind)
{
    /* Handling signal as per current state */
    if(localData.state & STATE_CONNECTED)
    {
        /* Authorise the pairing request if the application is NOT bonded */
        SMPairingAuthRsp(ind->data, (localData.bonded == FALSE));
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateCfm
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_CFM.
 *----------------------------------------------------------------------------*/
extern void handleSignalLsConnParamUpdateCfm(LS_CONNECTION_PARAM_UPDATE_CFM_T *event_data)
{
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
            if ((event_data->status != ls_err_none) && 
                (localData.conn_param_update_count <= MAX_NUM_CONN_PARAM_UPDATE_REQS))
            {
                localData.conn_param_update_tick_count = 0;
                localData.conn_param_counter_active = TRUE;
            }
            break;
        
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_IND
 *----------------------------------------------------------------------------*/
extern void handleSignalLsConnParamUpdateInd(LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
            /* Connection parameters have been updated. Check if new parameters 
             * comply with application prefered parameters. If not, application
             * shall trigger Connection parameter update procedure 
             */
             if(p_event_data->conn_interval < PREFERRED_MIN_CON_INTERVAL ||
                p_event_data->conn_interval > PREFERRED_MAX_CON_INTERVAL ||
                p_event_data->conn_latency < PREFERRED_SLAVE_LATENCY)
            {
                /* Set the connection parameter update attempts counter to zero */
                localData.conn_param_update_count = 0;
                localData.conn_param_update_tick_count = 0;
                /* Start timer to trigger Connection Paramter Update procedure */
                localData.conn_param_counter_active = TRUE;
            }
            break;

        default:
            break;
    }
 }


/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleConnectionUpdateInd
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_UPDATE.
 *----------------------------------------------------------------------------*/
extern void handleConnectionUpdateInd(LM_EV_CONNECTION_UPDATE_T *event_data)
{
    /* Handling signal as per current state */
    switch(localData.state)
    {
        case STATE_CONNECTED_IDLE:
        case STATE_CONNECTED_MOTION:
        case STATE_CONNECTED_AUDIO:
            /* Connection parameters have been updated. */
            localData.actual_interval = event_data->data.conn_interval;
            localData.actual_latency = event_data->data.conn_latency;
            localData.actual_timeout = event_data->data.supervision_timeout;
            break;

        default:
            break;
    }
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmDivApproveInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_DIV_APPROVE_IND.
 *----------------------------------------------------------------------------*/
extern void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    if(localData.state & STATE_CONNECTED)
    {
        /* Request for approval from application comes only when 
         * pairing is not in progress.
         */
        sm_div_verdict approve_div = SM_DIV_REVOKED;
        
        /* Check whether the application is still bonded (bonded flag gets
         * reset upon 'connect' button press by the user). Then check whether
         * the diversifier is the same as the one stored by the application
         */
        if(localData.bonded)
        {
            if(localData.diversifier == p_event_data->div)
            {
                approve_div = SM_DIV_APPROVED;
            }
        }

        SMDivApproval(p_event_data->cid, approve_div);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsRadioEventInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_RADIO_EVENT_IND
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void handleSignalLsRadioEventInd(void)
{
    /* Reception of radio_event_tx_data event indicates successful
     * transmission of data to the remote device. This should have freed up
     * some buffer space in f/w for transmitting another input report.
     */

#if defined(ACCELEROMETER_PRESENT) || defined(GYROSCOPE_PRESENT) || defined(TOUCHSENSOR_PRESENT)
    {
        /* Create a new timer close to the connection interval. */
        handleCreateReportTimer();
    }
    
#else
    
    /* There is nothing to do here in this version of the Smart Remote. */

#endif /* ACCELEROMETER_PRESENT || GYROSCOPE_PRESENT || TOUCHSENSOR_PRESENT */
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleCharValIndCfm
 *
 *  DESCRIPTION
 *      This function handles the signals GATT_CHAR_VAL_NOT_CFM and 
 *      GATT_CHAR_VAL_IND_CFM.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void handleCharValIndCfm(GATT_CHAR_VAL_IND_CFM_T *cfm)
{
    bool success = (cfm->result == sys_status_success);

    notificationRegisterResult(success);
    
    
    if(success)
    {
        notificationSendNext();
    }
}
