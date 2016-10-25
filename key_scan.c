/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2015
 *
 * FILE
 *    key_scan.c
 *
 *  DESCRIPTION
 *    This file defines the key-scanning hardware related routines.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <pio.h>
#include <sleep.h>
#include <mem.h>
#include <security.h>
#include <gatt_uuid.h>
#if defined(CLEAR_PAIRING_KEY)
#include <time.h>
#endif /* CLEAR_PAIRING_KEY */


/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "key_scan.h"

#include "service_hid.h"
#include "app_gatt_db.h"
#include "configuration.h"
#include "remote.h"
#include "state.h"
#include "advertise.h"
#include "event_handler.h"
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
#include "nvm_access.h"
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */


/*=============================================================================*
 *  Private Data
 *============================================================================*/

DECLARE_KEY_MATRIX();

/* State variables for comparing previous and current key-scan results */
static uint16 lastKeyCount;
static BUTTON_TYPE lastPressedButtonType = BUTTON_UNKNOWN;
static uint16 holdButton = 0x0000;

#if defined(CLEAR_PAIRING_KEY)
static timer_id clear_pairing_tid = TIMER_INVALID;

#endif /* CLEAR_PAIRING_KEY */

/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/
 
static void onFunctionButton(uint8 fnNum);
#if defined(CLEAR_PAIRING_KEY)
static void clear_pairing_timer(timer_id tid);
#endif /* CLEAR_PAIRING_KEY */


/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/

#if defined(CLEAR_PAIRING_KEY)
 
/*-----------------------------------------------------------------------------*
 *  NAME
 *      clear_pairing_timer
 *
 *  DESCRIPTION
 *      This function handles the clear pairing timer. It is called when the
 *      timer expires, indicating that the clear pairing button has been held
 *      down for long enough to trigger clearing of the pairing information.
 *----------------------------------------------------------------------------*/
static void clear_pairing_timer(timer_id tid)
{
    /* The user wants to re-pair this remote. */
    
    switch(localData.state)
    {
        case STATE_IDLE:
            handleClearPairing();
            break;
    
        case STATE_FAST_ADVERT:
        case STATE_SLOW_ADVERT:
        case STATE_DIRECT_ADVERT:
            localData.pairing_button_pressed = TRUE;
            
            /* Stop advertising */
            AdvStop();
            break;
            
        default:
            break;
    }
    
    clear_pairing_tid = TIMER_INVALID;
}
#endif /* CLEAR_PAIRING_KEY */

/*-----------------------------------------------------------------------------*
 *  NAME
 *      onFunctionButton
 *
 *  DESCRIPTION
 *      This function handles function button presses, updating the controlled
 *      device as necessary.
 *
 *  PARAMETERS
 *      fnNum   The number of the function button pressed (1-8)
 *
 *  RETURNS
 *      Nothing.
 *----------------------------------------------------------------------------*/
static void onFunctionButton(uint8 fnNum)
{
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
    uint8 prevControlledDevice;
    uint8 requestedIRDeviceIndex;
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */



#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
    prevControlledDevice = localData.controlledDevice;
    requestedIRDeviceIndex = fnNum-1;
    
    
    if(localData.controlledDevice != prevControlledDevice)
    {
        /* IR controlled device has changed, write new value to NVM */
        Nvm_Write((uint16 *)&localData.controlledDevice, 
                 sizeof(localData.controlledDevice), 
                 NVM_OFFSET_IR_CONTROLLED_DEVICE);
    }
    
    if(localData.controlledDevice != IRCONTROL_HOST)
    {
        /* Stop processing and sending anything else to the Host,
           we are no longer controlling it. */
        if(localData.state & STATE_CONNECTED)
        {
            stateSet(STATE_CONNECTED_IDLE);
        }
    }
    
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
}



/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      keyscanInit
 *
 *  DESCRIPTION
 *      This function allocates the key-scan -matrix PIOS to the PIO controller.
 *      Note: if the scroll-wheel is enabled, then these PIOs are also included
 *      here in the allocation.
 *----------------------------------------------------------------------------*/
void keyscanInit(void)
{
    /* Give the PIO controller access to the PIOs */
    PioSetModes(PIO_CONTROLLER_BIT_MASK, pio_mode_pio_controller);

    /* Set the pull mode of the PIOs. We need strong pull ups on inputs and
     * outputs because outputs need to be open collector. This allows rows and
     * columns to be shorted together in the key matrix.
     */
     
    /* Note that the scroll-wheel PIOs are not included here. */
    PioSetPullModes(KEY_MATRIX_PIO_BIT_MASK, pio_mode_strong_pull_up);
    
#if defined(SWHEEL_PRESENT) && !defined(SCROLL_WHEEL_IS_POSITIVELY_COUPLED)
    /* The scroll wheel pios use weak pull-ups, to minimise current consumption. */
    PioSetPullModes(SWHEEL_PIOS, pio_mode_weak_pull_up);
#else
    PioSetPullModes(PIO_CONTROLLER_BIT_MASK, pio_mode_strong_pull_up);
#endif /* SWHEEL_PRESENT && SCROLL_WHEEL_IS_POSITIVELY_COUPLED */
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      keyscanProcessScanReport
 *
 *  DESCRIPTION
 *      Processes the scan_report from the PIO controller in order to determine
 *      which buttons are currently pressed down, fills in a hid_report
 *      accordingly, and then reports back to the calling function by means of a
 *      BUTTON_SCAN_T object. The actual buttons pressed are communicated with
 *      HID codes in the HID report. Thus only one Consumer page button can be
 *      reported at a time, as Consumer HID reports only contain one button. The
 *      number of buttons pressed down is however passed out in buttonStatus,
 *      and since key-scan occurs rather quickly, we should be able to pick them
 *      out one at a time as the number increments. Keyboard Page HID reports
 *      support multiple buttons per report. If IR is enabled, then IR "presses"
 *      and "releases" are dealt with in this function too (only one code can be
 *      sent at a time).
 *
 *  PARAMETERS
 *      *scan_report    [Input]
 *                      Pointer to the array in which the scan results from the 
 *                      PIO controller are stored. The array is always 8 bits
 *                      wide (hence supporting a maximum of 8 key-scan columns),
 *                      its length is defined by the number of key-scan rows.
 *
 *      *hid_report     [Output]
 *                      Pointer to the HID report to be filled in. The length of
 *                      the report (really just an array) is defined by the
 *                      longest report type supported. The type of the returned
 *                      HID report is passed back in the buttonStatus object.
 *
 *      *buttonStatus   [Output]
 *                      Pointer to a BUTTON_SCAN_T structure object that will be
 *                      populated with various information about the scan_report
 *                      e.g. number of pressed buttons, HID report type, mouse
 *                      button status (if applicable) and so on.
 *  RETURNS
 *      "Nothing". (Data is returned through objects passed in as parameters.)
 *----------------------------------------------------------------------------*/
extern void keyscanProcessScanReport(uint8* scan_report, uint8* hid_report, BUTTON_SCAN_T* buttonStatus)
{
    uint8 i, j;             /* loop counters */
    uint8 arrayIndex = 1;   /* current "look up" location in key matrix */
    uint16 reportWord;      /* temporary storage for an entire key-scan row */
    uint16 this_key = 0;
    bool done = FALSE;      /* skip main scan loop (or what remains) */
    uint16 keyCount = 0;
    

    /* Set data defaults, as required */
    buttonStatus->pressedButtonType = BUTTON_UNKNOWN;
    buttonStatus->numPressedConsumerKeys = 0;
    
    /* First do a quick pass to check how many keys are held down in total. If
     * it's 0 then the last key has been released and we don't need to do a full
     * scan/decode.
     */
    for(i = 0; i < SCAN_MATRIX_ROWS_BYTE_COUNT; i++)
    {
        for(j = 1; j & 0xFF; j <<= 1)
        {
            if(scan_report[i] & j) keyCount++;
        }
    }
    
    if (keyCount < lastKeyCount)
    {
        /* Key release event */  
        holdButton = 0x0000;
        if (keyCount == 0)
        {
            /* All keys released, don't do a full scan/decode */
            done = TRUE;
            /* Reset state variable */
            lastPressedButtonType = BUTTON_UNKNOWN;
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
            /* Stop sending IR */
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
        }
    }

    /* Look for report bytes (rows) containing set bits (pressed keys) */
    i = 0;
    while((i < SCAN_MATRIX_ROWS_BYTE_COUNT) && !done)
    {
        /* Grab the next row */
        reportWord = scan_report[i];

        if(reportWord > 0)
        {
            /* This byte (row) contains a pressed key */
            
            /* For each bit (key) of the byte (row)... */
            for(j = 0; j < 8; j++)
            {
                if(reportWord & 0x1)
                {
                    /* If reportWord lowest bit is set then we have found
                     * a pressed key, it is shifted right after each iteration
                     * to scan along the row, with arrayIndex identifying the
                     * particular key (also incremented on each iteration).
                     */
                    this_key = RemoteKeyMatrix[arrayIndex];

#if defined(CLEAR_PAIRING_KEY)
                    /* Catch clear pairing key */
                    if(this_key == CLEAR_PAIRING_KEY)
                    {
                        if(clear_pairing_tid == TIMER_INVALID)
                        {
                            clear_pairing_tid = TimerCreate(CLEAR_PAIRING_TIMER, TRUE, clear_pairing_timer);
                        }
                    }
#ifndef CLEAR_PAIRING_DUAL_PURPOSE
                    else
#endif /* !CLEAR_PAIRING_DUAL_PURPOSE */
#endif /* CLEAR_PAIRING_KEY */
                    {
                        /* Catch special keys first, handle later */
                        switch(this_key)
                        {
                            /* If this remote defines function buttons, insert
                             * the necessary handlers here. By default they set
                             * the active controlled device if using IR.
                             */
                            case FUNCTION_BUTTON_1:
                            case FUNCTION_BUTTON_2:
                            case FUNCTION_BUTTON_3:
                            case FUNCTION_BUTTON_4:
                            case FUNCTION_BUTTON_5:
                            case FUNCTION_BUTTON_6:
                            case FUNCTION_BUTTON_7:
                            case FUNCTION_BUTTON_8:
                                onFunctionButton((uint8)((this_key - FUNCTION_BUTTON_1 + 1) & 0xFF));
                                break;
                            
                            default:
                                /* Now perform "normal" keyscan operations, as applicable */
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
                                if (   keyCount == 1 
                                    && keyCount >= lastKeyCount 
                                   )
                                {
                                    /* If there is exactly 1 key pressed, 
                                        and there were no keys pressed before,
                                        and this pressed key has an IR assignment
                                        and there is an IR command definition for this IR assignment on the currently selected device*/
                                }
                                else
                                {
                                    /* Only perform HID report sending if selected device is the Host
                                     * and HID reports are not disabled for this button.
                                     */
                                    if(localData.controlledDevice == IRCONTROL_HOST && this_key != 0)
#else
                                    /* Only perform HID report sending if HID reports are not disabled 
                                     * for this button.
                                     */
                                    if(this_key != 0)
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
                                    {
                                        {
                                            /* Button on the Consumer Page */
                                            buttonStatus->numPressedConsumerKeys++;
                                            
                                            if (holdButton == this_key)
                                            {
                                                /* We have already reported this key in a previous instance
                                                 * of the loop, we are looking for new buttons so do not
                                                 * fill in the HID report.
                                                 */
                                                buttonStatus->pressedButtonType = BUTTON_CONSUMER;
                                                
                                            }
                                            else
                                            {
                                                if (lastPressedButtonType == BUTTON_UNKNOWN)
                                                {
                                                    /* No other keys of interest detected yet on this loop
                                                     * instance (this is the first, there may be more).
                                                     * 
                                                     * If we already had a button held down, then this is
                                                     * the new button and just happens to be earlier in the
                                                     * key matrix than the held button. If not, then we are
                                                     * starting from scratch and we need to record that we
                                                     * have already seen this button for future loop
                                                     * instances. In any case the button press needs to be
                                                     * reported so fill in the HID report.
                                                     */
                                                    if (holdButton == 0x0000)
                                                    {
                                                        holdButton = this_key;
                                                    }
                                                    buttonStatus->pressedButtonType = BUTTON_CONSUMER;
                                                    hid_report[0] = WORD_LSB(this_key);
                                                    hid_report[1] = WORD_MSB(this_key);
                                                }
                                                else if (lastPressedButtonType == BUTTON_CONSUMER)
                                                {
                                                    /* This is the second key of this type detected on this
                                                     * loop instance. Only one Consumer key can be reported
                                                     * at a time, so report this one as it is newer (only 1
                                                     * consumer key press is really supported, but we need
                                                     * to know what state we are in so we leave it up to a
                                                     * higher level calling function to decide what to do).
                                                     */
                                                    buttonStatus->pressedButtonType = BUTTON_CONSUMER;
                                                    hid_report[0] = WORD_LSB(this_key);
                                                    hid_report[1] = WORD_MSB(this_key);
                                                }
                                            }
                                        }
                                        lastPressedButtonType = buttonStatus->pressedButtonType;
                                    }
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
                                }
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
                            break;
                            
                        }   /* End switch(this_key)*/
                    }
                }   /* End if(reportWord & 0x1) */
                
                /* We want to detect multiple key-presses, so continue. */
                reportWord >>= 1;
                arrayIndex++;
                
            }   /* End loop over keys in row */
        }
        else    /* reportWord = 0 */
        {
            /* Jump 8 positions along the lookup table (next row down) */
            arrayIndex += 8;
        }
        /* We want to detect multiple key-presses, so continue. */
        i++;
    }   /* End loop over rows */
    
    

    
#if defined(CLEAR_PAIRING_KEY)
    if((this_key != CLEAR_PAIRING_KEY) &&
       (clear_pairing_tid != TIMER_INVALID))
    {
        /* Assume the pairing key has just been released */
        
        TimerDelete(clear_pairing_tid);
        clear_pairing_tid = TIMER_INVALID;
    }
#endif /* CLEAR_PAIRING_USING_KEYS */
    lastKeyCount = keyCount;
}


/*============================================================================
 * End of file: key_scan.c
 *============================================================================*/
