/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    remote.c
 *
 *  DESCRIPTION
 *    This file defines a remote-contoller application.
 *
 ******************************************************************************/

/*=============================================================================
 *  SDK Header Files
 *============================================================================*/
#include <main.h>
#include <ls_app_if.h>
#include <pio.h>
#include <mem.h>
#include <time.h>
#include <security.h>
#include <nvm.h>
#include <pio_ctrlr.h>


/*=============================================================================
 *  Local Header Files
 *============================================================================*/

#include "remote.h"
#include "nvm_access.h"
#include "advertise.h"
#include "event_handler.h"
#include "app_gatt.h"
#include "app_gatt_db.h"
#include "remote_gatt.h"
#include "i2c_comms.h"
#include "key_scan.h"
#include "remote_hw.h"
#include "notifications.h"

#include "service_gap.h"
#include "service_hid.h"
#include "service_battery.h"

#include "service_gatt.h"





/*=============================================================================
 *  Private Definitions
 *============================================================================*/

/* Maximum number of timers.
 * The following items require timers:
 * - advertising
 * - input report sending
 * - gyroscope warm-up
 * - clear pairing key-press timer
 * - infra-red transmissions
 *
 * The following could be simultaneous:
 * 1. (when not connected) advertising, clear pairing, IR
 *      = 4
 * 2. (when connected) gyro warm-up, input report, bonding chance, IR
 *      = 4
 */
#define MAX_APP_TIMERS                      (5) 
                        /* In the best SW tradition, add one for luck */

/*=============================================================================
 *  Private Data
 *============================================================================*/

/* Remote application data structure */
LOCAL_DATA_T localData;

/* Declare space for application timers. */
static uint16 app_timers[SIZEOF_APP_TIMER * MAX_APP_TIMERS];

/* The interrupt handler for interrupts from the PIO controller. */
typedef void (*rominitfn_t)(uint16 *code);
rominitfn_t ROMPioCtrlrInit = (rominitfn_t)0xE99F;

/*=============================================================================
 *  Private Function Prototypes
 *============================================================================*/
static void readPersistentStore(void);
void pio_ctrlr_code(void);  /* Included externally in PIO controller code.*/

/*=============================================================================
 *  Private Function Implementations
 *============================================================================*/
 
/*-----------------------------------------------------------------------------*
 *  NAME
 *      readPersistentStore
 *
 *  DESCRIPTION
 *      This function is used to initialise and read NVM data
 *
 *----------------------------------------------------------------------------*/
static void readPersistentStore(void)
{
    uint16 offset = N_APP_USED_NVM_WORDS;
    uint16 nvm_sanity = 0xffff;

    /* Read persistent storage to know if the device was last bonded 
     * to another device 
     */

    /* If the device was bonded, trigger advertisements for the bonded
     * host(using whitelist filter). If the device was not bonded, trigger
     * advertisements for any host to connect to the application.
     */

    /* Switch to main I2C bus */
    i2cUseMainBus();
    
    /* Check if the I2C bus is ready and reset if not */
    checkI2cBusState();
    
    Nvm_Read(&nvm_sanity, sizeof(nvm_sanity), NVM_OFFSET_SANITY_WORD);

    if(nvm_sanity == NVM_SANITY_MAGIC)
    {
        /* Read Bonded Flag from NVM */
        Nvm_Read((uint16*)&localData.bonded, 
                 sizeof(localData.bonded), 
                 NVM_OFFSET_BONDED_FLAG);
        
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
        /* Read IR controlled device */
        Nvm_Read((uint16*)&localData.controlledDevice, 
                 sizeof(localData.controlledDevice), 
                 NVM_OFFSET_IR_CONTROLLED_DEVICE);
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */

        if(localData.bonded)
        {
            /* Bonded Host Typed BD Address will only be stored if bonded flag
             * is set to TRUE. Read last bonded device address.
             */
            Nvm_Read((uint16*)&localData.bonded_bd_addr, 
                     sizeof(TYPED_BD_ADDR_T), 
                     NVM_OFFSET_BONDED_ADDR);

            /* If the bonded device address is resolvable then read the bonded
             * device's IRK
             */
            if(IsAddressResolvableRandom(&localData.bonded_bd_addr))
            {
                Nvm_Read(localData.central_device_irk.irk,
                         MAX_WORDS_IRK,
                         NVM_OFFSET_SM_IRK);
            }
        }
        
        /* Read the diversifier associated with the presently bonded/last bonded
         * device.
         */
        Nvm_Read(&localData.diversifier,
                 sizeof(localData.diversifier),
                 NVM_OFFSET_SM_DIV);

        /* Read device name and length from NVM */
        GapReadDataFromNVM(&offset);
    }
    else /* NVM Sanity check failed means either the device is being brought up 
          * for the first time or memory has got corrupted in which case discard 
          * the data and start afresh.
          */
    {
        nvm_sanity = NVM_SANITY_MAGIC;

        /* Write NVM Sanity word to the NVM */
        Nvm_Write(&nvm_sanity, 
                  sizeof(nvm_sanity), 
                  NVM_OFFSET_SANITY_WORD);

        /* The device will not be bonded as it is coming up for the first time*/
        localData.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&localData.bonded, 
                  sizeof(localData.bonded),
                  NVM_OFFSET_BONDED_FLAG);

        /* The device is not bonded so don't have BD Address to update */

        /* When the remote is booting up for the first time after flashing the
         * image to it, it will not have bonded to any device. So, no LTK will
         * be associated with it. So, set the diversifier to 0.
         */
        localData.diversifier = 0;

        /* Write the same to NVM. */
        Nvm_Write(&localData.diversifier, 
                  sizeof(localData.diversifier),
                  NVM_OFFSET_SM_DIV);

        /* Write Gap data to NVM */
        GapInitWriteDataToNVM(&offset);
    }
    
    /* Read/write GATT data to/from NVM */
    GattReadDataFromNVM(&offset);

    /* Read HID service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    HidReadDataFromNVM(localData.bonded, &offset);

    /* Read Battery service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    BatteryReadDataFromNVM(localData.bonded, &offset);
    
}


/*=============================================================================
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      WakeRemoteIfRequired
 *
 *  DESCRIPTION
 *      This function handles the Keypad or wheel data and accordingly puts the 
 *      remote in active state.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void WakeRemoteIfRequired(void)
{
    {
        if(localData.state == STATE_IDLE)
        {
            /* Start advertising */
            stateSet(STATE_ADVERTISING);
        }
    }
    
#if defined(DISCONNECT_ON_IDLE)
    /* Some activity occurred, so reset the idle timer */
    handleResetIdleTimer();
#endif /* DISCONNECT_ON_IDLE */
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      RemoteDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise remote application data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void RemoteDataInit(void)
{
    /* Initialise the data structure variables used by the application to their
     * default values. Each service data has also to be initialised.
     */

    /* Delete all the timers */
    TimerDelete(localData.advertising_tid);
    localData.advertising_tid = TIMER_INVALID;

    TimerDelete(localData.recrypt_tid);
    localData.recrypt_tid = TIMER_INVALID;

    localData.st_ucid = GATT_INVALID_UCID;
    localData.encrypt_enabled = FALSE;
    localData.pairing_button_pressed = FALSE;
    localData.blockNotifications = TRUE;
    localData.actual_interval = PREFERRED_MIN_CON_INTERVAL;
    localData.actual_latency = (PREFERRED_SLAVE_LATENCY + 1);
    localData.actual_timeout = (PREFERRED_SUPERVISION_TIMEOUT + 1);

    localData.disconnect_reason = DEFAULT_DISCONNECTION_REASON;

    MemSet(localData.latest_button_report, 0, HID_KEYPRESS_DATA_LENGTH);

    TimerDelete(localData.next_report_timer_id);
    localData.next_report_timer_id = TIMER_INVALID;

    /* Initialise GAP Data Structure */
    GapDataInit();
	

    /* HID Service data initialisation */
    HidDataInit();

    /* Battery Service data initialisation */
    BatteryDataInit();
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppUpdateWhiteList
 *
 *  DESCRIPTION
 *      This function updates the whitelist with bonded device address if
 *      it's not private, and also reconnection address when it has been written
 *      by the remote device.
 *
 *----------------------------------------------------------------------------*/

extern void AppUpdateWhiteList(void)
{
    LsResetWhiteList();
    
    if(localData.bonded && 
            (!IsAddressResolvableRandom(&localData.bonded_bd_addr)) &&
            (!IsAddressNonResolvableRandom(&localData.bonded_bd_addr)))
    {
        /* If the device is bonded and bonded device address is not private
         * (resolvable random or non-resolvable random), configure White list
         * with the Bonded host address 
         */
         
        (void)LsAddWhiteListDevice(&localData.bonded_bd_addr);
    }

#ifdef __GAP_PRIVACY_SUPPORT__
    /* If the reconnection address is valid, copy it into the white list. */
    if(GapIsReconnectionAddressValid())
    {
        TYPED_BD_ADDR_T temp_addr;

        temp_addr.type = ls_addr_type_random;
        MemCopy(&temp_addr.addr, GapGetReconnectionAddress(), sizeof(BD_ADDR_T));
        (void)LsAddWhiteListDevice(&temp_addr);
    }

#endif /* __GAP_PRIVACY_SUPPORT__ */

}



/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppPowerOnReset
 *
 *  DESCRIPTION
 *      This function is called just after a power-on reset (including after
 *      a firmware panic).
 *
 *      NOTE: this function should only contain code to be executed after a
 *      power-on reset or panic. Code that should also be executed after an
 *      HCI_RESET should instead be placed in the reset() function.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppPowerOnReset(void)
{
    /* Configure the application constants */
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppInit
 *
 *  DESCRIPTION
 *      This function is called after a power-on reset (including after a
 *      firmware panic) or after an HCI Reset has been requested.
 *
 *      NOTE: In the case of a power-on reset, this function is called
 *      after app_power_on_reset.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppInit(sleep_state LastSleepState)
{
    uint16 gatt_database_length;
    uint16 *gatt_database_pointer = NULL;
    
    /* Don't wakeup on UART RX line toggling */
    SleepWakeOnUartRX(FALSE);

    PioSetModes(0xffffffff, pio_mode_user);
    PioSetDirs(0xffffffff, FALSE);
    /* All PIOs are pulled high by default */
    PioSetPullModes(0xffffffff, pio_mode_strong_pull_up);
    /* These PIOs must be pulled low, for various reasons */
    PioSetPullModes(PIOS_TO_PULL_LOW, pio_mode_strong_pull_down);
    
    /* Initialise the application timers */
    TimerInit(MAX_APP_TIMERS, (void*)app_timers);

    /* Initialise GATT entity */
    GattInit();

    /* Install GATT Server support for the optional Write procedures */
    GattInstallServerWrite();
    
    /* Initialise the NVM to be I2C EEPROM */
    NvmConfigureI2cEeprom();
    
    /* Initialise GAP Data Structure */
    GapDataInit();

    /* Battery Service Initialisation on Chip reset */
    BatteryInitChipReset();

    /* Read persistent storage */
    readPersistentStore();

    /* Tell Security Manager module about the value it needs to initialize it's
     * diversifier to.
     */
    SMInit(localData.diversifier);
    
    /* Initialise remote application data structure */
    RemoteDataInit();

    /* Set up the PIO controller. */
    ROMPioCtrlrInit((uint16*)&pio_ctrlr_code);

    /* Initialise Hardware to set PIO controller for PIOs scanning */
    keyscanInit();
    




    /* We have finished using the NVM for now, so disable it to save power */
    Nvm_Disable();

    /* Initialise remote state */
    localData.state = STATE_INIT;

    /* Tell GATT about our database. We will get a GATT_ADD_DB_CFM event when
     * this has completed.
     */
    gatt_database_pointer = GattGetDatabase(&gatt_database_length);
    GattAddDatabaseReq(gatt_database_length, gatt_database_pointer);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessSystemEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a system event, such
 *      as a battery low notification, is received by the system.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

void AppProcessSystemEvent(sys_event_id id, void *data)
{
#if defined(AUDIO_BUTTON_PIO) || defined(ACCELEROMETER_INTERRUPT_PIO) || defined(GYROSCOPE_INTERRUPT_PIO) || defined(TOUCHSENSOR_INTERRUPT_PIO)
    uint32 pioState;
#endif /* AUDIO_BUTTON_PIO || ACCELEROMETER_INTERRUPT_PIO ||  GYROSCOPE_INTERRUPT_PIO || TOUCHSENSOR_INTERRUPT_PIO */


    switch(id)
    {
        case sys_event_battery_low:
            /* Battery low event received - notify the connected Central. 
             * If  not connected, the battery level will get notified when 
             * device gets connected again
             */
            if(STATE_CONNECTED & localData.state)
            {
                BatteryUpdateLevel(localData.st_ucid);
            }
            break;

        case sys_event_pio_ctrlr:   /* Event from the PIO controller */
            hwHandlePIOControllerEvent();
            break;

#if defined(AUDIO_BUTTON_PIO) || defined(ACCELEROMETER_INTERRUPT_PIO) || defined(GYROSCOPE_INTERRUPT_PIO) || defined(TOUCHSENSOR_INTERRUPT_PIO)
        case sys_event_pio_changed: /* Event because of PIO state change.*/
            /* Record the new PIO states*/
            pioState = ((pio_changed_data*)data)->pio_state;
            
#if defined(TOUCHSENSOR_PRESENT) && defined(TOUCHSENSOR_INTERRUPT_PIO)
            /* Pass the interrupt (pio) state to the touch-sensor module for processing */
            TouchsensorHandleInterrupt(pioState);
#endif /* TOUCHSENSOR_INTERRUPT_PIO && TOUCHSENSOR_PRESENT */
            break;
#endif /* AUDIO_BUTTON_PIO || ACCELEROMETER_INTERRUPT_PIO ||  GYROSCOPE_INTERRUPT_PIO  || TOUCHSENSOR_INTERRUPT_PIO */

        default:
            /* Do nothing. */
            break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessLmEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a LM-specific event is
 *      received by the system.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

bool AppProcessLmEvent(lm_event_code event_code, LM_EVENT_T *event_data)
{
    switch(event_code)
    {
        /* Below messages are received in STATE_INIT state */
        case GATT_ADD_DB_CFM:
            handleSignalGattAddDBCfm((GATT_ADD_DB_CFM_T*)event_data);
            break;

        /* Below messages are received in advertising state. */
        case GATT_CONNECT_CFM:
            handleSignalGattConnectCfm((GATT_CONNECT_CFM_T*)event_data);
            break;
        
        case GATT_CANCEL_CONNECT_CFM:
            handleSignalGattCancelConnectCfm();
            break;

        case GATT_ACCESS_IND:
            /* Received when HID Host tries to read or write to any
             * characteristic with FLAG_IRQ.
             */             
            handleSignalGattAccessInd((GATT_ACCESS_IND_T*)event_data);
            break;

        case LM_EV_DISCONNECT_COMPLETE:
            handleSignalLmEvDisconnectComplete(&((LM_EV_DISCONNECT_COMPLETE_T *)event_data)->data);
            break;
        
        case LM_EV_ENCRYPTION_CHANGE:
            handleSignalLMEncryptionChange(event_data);
            break;
            
        case SM_PAIRING_AUTH_IND:
            handleSignalSmPairingAuthInd((SM_PAIRING_AUTH_IND_T*)event_data);
            break;
            
        case SM_KEYS_IND:
            handleSignalSmKeysInd((SM_KEYS_IND_T*)event_data);
            break;
        
        case SM_SIMPLE_PAIRING_COMPLETE_IND:
            handleSignalSmSimplePairingCompleteInd((SM_SIMPLE_PAIRING_COMPLETE_IND_T*)event_data);
            break;

        case LS_RADIO_EVENT_IND:
            handleSignalLsRadioEventInd();
            break;

        /* Received in response to the L2CAP_CONNECTION_PARAMETER_UPDATE request
         * sent from the slave after encryption is enabled. If the request has
         * failed, the device should again send the same request only after
         * Tgap(conn_param_timeout). Refer Bluetooth 4.0 spec Vol 3 Part C,
         * Section 9.3.9 and HID over GATT profile spec section 5.1.2.
         */
        case LS_CONNECTION_PARAM_UPDATE_CFM:
            handleSignalLsConnParamUpdateCfm((LS_CONNECTION_PARAM_UPDATE_CFM_T*)event_data);
            break;

        case LS_CONNECTION_PARAM_UPDATE_IND:
            handleSignalLsConnParamUpdateInd((LS_CONNECTION_PARAM_UPDATE_IND_T *)event_data);
            break;
            
        case LM_EV_CONNECTION_UPDATE:
            handleConnectionUpdateInd((LM_EV_CONNECTION_UPDATE_T *)event_data);
            break;
            
        case SM_DIV_APPROVE_IND:
            handleSignalSmDivApproveInd((SM_DIV_APPROVE_IND_T *)event_data);
            break;
            
        case GATT_CHAR_VAL_NOT_CFM:
        case GATT_CHAR_VAL_IND_CFM:
            handleCharValIndCfm((GATT_CHAR_VAL_IND_CFM_T *) event_data);
            break;
            
        case SYS_BACKGROUND_TICK_IND:
            handleBackgroundTickInd();
            break;
        
        default:
            /* Unhandled event. */    
            break;
        
    }

    return TRUE;    /* Indicate to the FW that we have finished processing this event. */
}

