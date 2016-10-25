/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      state.c
 *
 *  DESCRIPTION
 *      Source file for state transition functions
 *
 ******************************************************************************/
/*=============================================================================*
 *  SDK Header File
 *============================================================================*/

#include <time.h>
#include <gatt.h>

/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "remote.h"
#include "key_scan.h"
#include "remote_gatt.h"
#include "state.h"
#include "advertise.h"
#include "notifications.h"
#include "service_hid.h"
#include "notifications.h"
#include "event_handler.h"

#if defined(__GAP_PRIVACY_SUPPORT__)
#include "service_gap.h"
#endif /*__GAP_PRIVACY_SUPPORT__ */




/*=============================================================================
 *  Private function definitions
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      determineAdvertisingType
 *
 *  DESCRIPTION
 *      This function is used to start directed advertisements if a valid
 *      reconnection address has been written by the remote device. Otherwise,
 *      it starts fast undirected advertisements.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static CURRENT_STATE_T determineAdvertisingType(void)
{
    CURRENT_STATE_T advert_type;
    
#ifdef __GAP_PRIVACY_SUPPORT__
    /* If re-connection address is non-zero, start directed adv */
    if(GapIsReconnectionAddressValid())
#else
    if((localData.bonded == TRUE) &&
       (IsAddressResolvableRandom(&localData.bonded_bd_addr) == FALSE))
#endif /* __GAP_PRIVACY_SUPPORT__ */
    {
        advert_type = STATE_DIRECT_ADVERT;
    }
    else
    {    
        advert_type = STATE_FAST_ADVERT;
    }

    return advert_type;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      exitInitState
 *
 *  DESCRIPTION
 *      This function is called upon exiting from INIT state.
 *----------------------------------------------------------------------------*/
static void exitInitState(void)
{
    /* Start running the PIO controller code */
    PioCtrlrStart();

     /* PIO controller code will be in a loop. It won't start key scanning unless
      * application interrupts it to do so (so do that now).
      */
    hwSetControllerForKeyscan(TRUE, TRUE);

    /* Application will start advertising upon exiting STATE_INIT state. So,
     * update the whitelist.
     */
    AppUpdateWhiteList();
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      exitConnectedState
 *
 *  DESCRIPTION
 *      This function is called upon exiting from CONNECTED state.
 *----------------------------------------------------------------------------*/
static void exitConnectedState(void)
{
#if defined(DISCONNECT_ON_IDLE)
    handleResetIdleTimer();
#endif /* DISCONNECT_ON_IDLE */
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      exitMotionState
 *
 *  DESCRIPTION
 *      This function is called upon exiting from ACTIVE state.
 *----------------------------------------------------------------------------*/
static void exitMotionState(void)
{
    /* Radio event notifications are received for all the data sent.
     * If enabled, these notifications come after the messages like
     * write response, read response are transmitted. So, disable
     * these events when the mouse is not sending any reports
     */
    LsRadioEventNotification(localData.st_ucid, radio_event_none);

    TimerDelete(localData.next_report_timer_id);
    localData.next_report_timer_id = TIMER_INVALID;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      exitIdleState
 *
 *  DESCRIPTION
 *      This function is called upon exiting from IDLE state.
 *----------------------------------------------------------------------------*/
static void exitIdleState(void)
{

}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      enterAdvertisingState
 *
 *  DESCRIPTION
 *      This function performs actions generic to all advertising states.
 *
 *----------------------------------------------------------------------------*/
static void enterAdvertisingState(void)
{


    if(localData.state == STATE_DIRECT_ADVERT)
    {
        /* Directed advertisement doesn't use any timer. Directed
         * advertisements are done for 1.28 seconds always.
         */
        AdvStart(FALSE, gap_mode_connect_directed);
    }
    else
    {
        AdvStart((localData.state == STATE_FAST_ADVERT), gap_mode_connect_undirected);
    }
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      enterConnectedIdleState
 *
 *  DESCRIPTION
 *      This function puts the remote into CONNECTED_IDLE state.
 *
 *----------------------------------------------------------------------------*/
static void enterConnectedIdleState(void)
{
    /* Common things to do upon entering STATE_CONNECTED_IDLE state */
            
#if defined(SPEECH_TX_PRESENT) && !defined(CODEC_IS_MAX9860)

    /* Power up (into low-power mode) the audio CODEC
     * so that it is ready for use. We do this now because the CODEC
     * warm-up time is too long for on-demand use.
     */
    if(codec_isInitialised == FALSE)
    {
        /* Power up the audio codec */
        codec_enable(TRUE);

        /* setup the codec to use 16kHz 16-bit PCM */
        codec_configure();
    }
    
#endif /* SPEECH_TX_PRESENT && !CODEC_IS_MAX9860 */


                
#if defined(DISCONNECT_ON_IDLE)
    handleResetIdleTimer();
#endif /* DISCONNECT_ON_IDLE */
    
    AppBackgroundTick(TRUE);
}

#if defined(SPEECH_TX_PRESENT) && defined(CODEC_IS_MAX9860)
/*-----------------------------------------------------------------------------*
 *  NAME
 *      enterConnectedAudioState
 *
 *  DESCRIPTION
 *      This function puts the remote into CONNECTED_AUDIO state.
 *
 *----------------------------------------------------------------------------*/
static void enterConnectedAudioState(void)
{
    /* Power up the audio CODEC. */
    if(codec_isInitialised == FALSE)
    {
        /* Power up the audio codec */
        codec_enable(TRUE);

        /* setup the codec to use 16kHz 16-bit PCM */
        codec_configure();
    }
}
#endif /* SPEECH_TX_PRESENT && CODEC_IS_MAX9860 */

/*-----------------------------------------------------------------------------*
 *  NAME
 *      enterDisconnectingState
 *
 *  DESCRIPTION
 *      This function puts the remote into DISCONNECTING state.
 *
 *----------------------------------------------------------------------------*/
static void enterDisconnectingState(void)
{


    GattDisconnectReasonReq(localData.st_ucid,localData.disconnect_reason);
    localData.disconnect_reason = DEFAULT_DISCONNECTION_REASON;

}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      enterIdleState
 *
 *  DESCRIPTION
 *      This function puts the remote into IDLE state.
 *
 *----------------------------------------------------------------------------*/
static void enterIdleState(void)
{



    notificationDropAll();
    
    AppBackgroundTick(FALSE);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      enterConnectedMotionState
 *
 *  DESCRIPTION
 *      This function puts the remote into CONNECED_MOTION state.
 *
 *----------------------------------------------------------------------------*/
static void enterConnectedMotionState(void)
{
    /* In MOTION state, the remote control application will send button press 
     * and motion data to the remote host device.
     */

    if(0)
    {

#if defined(DISCONNECT_ON_IDLE)
        handleResetIdleTimer();
#endif /* DISCONNECT_ON_IDLE */
    }
    else
    {
        /* Notifications are not enabled, switch back to the CONNECTED_IDLE state. */
        stateSet(STATE_CONNECTED_IDLE);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      stateSet
 *
 *  DESCRIPTION
 *      This function is used to set the state of the application.
 *----------------------------------------------------------------------------*/
extern void stateSet(CURRENT_STATE_T new_state)
{
    /* Check if the new state to be set is not the same as the present state
     * of the application.
     */
    uint16 old_state = localData.state;
    
    if(new_state != old_state)
    {
        /* Handle exiting old state */
        switch (old_state)
        {
            case STATE_INIT:
                exitInitState();
                break;
                    
            case STATE_CONNECTED_IDLE:
            case STATE_CONNECTED_AUDIO:
                exitConnectedState();
                break;
                    
            case STATE_CONNECTED_MOTION:
                exitMotionState();
                break;
    
            case STATE_IDLE:
                exitIdleState();
                break;
    
            default:
                /* Nothing to do. */    
                break;
        }
    
        /* Set new state */
        if(new_state == STATE_ADVERTISING)
        {
            new_state = determineAdvertisingType();
        }
            
        localData.state = new_state;
    
        /* Handle entering new state */
        switch (new_state)
        {
            case STATE_DIRECT_ADVERT:
            case STATE_FAST_ADVERT:
            case STATE_SLOW_ADVERT:
                enterAdvertisingState();
                break;
    
            case STATE_CONNECTED_AUDIO:
#if defined(SPEECH_TX_PRESENT) && defined(CODEC_IS_MAX9860)
                enterConnectedAudioState();
#endif /* SPEECH_TX_PRESENT && CODEC_IS_MAX9860 */
                // FALL THRU

            case STATE_CONNECTED_IDLE:
                enterConnectedIdleState();
                break;
    
            case STATE_DISCONNECTING:
                enterDisconnectingState();
                break;
    
            case STATE_IDLE:
                enterIdleState();
                break;
    
            case STATE_CONNECTED_MOTION:
                enterConnectedMotionState();
                break;
    
            default:
                /* Nothing to do. */    
                break;
        }
    }
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      stateSetDisconnect
 *
 *  DESCRIPTION
 *      This function is used to set the application state to disconnecting 
 *  ensuring that a specific error code is sent in the disconnect message.
 *----------------------------------------------------------------------------*/

void stateSetDisconnect(ls_err disconnect_reason)
{
    localData.disconnect_reason = disconnect_reason;
    stateSet(STATE_DISCONNECTING);
}

