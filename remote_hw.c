/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    remote_hw.c
 *
 *  DESCRIPTION
 *    This file defines general hardware routines for the remote control
 *    application.
 *
 ******************************************************************************/

 /*=============================================================================
 *  SDK Header Files
 *============================================================================*/
#include <gatt.h>
#include <mem.h>

/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "configuration.h"
#include "remote_hw.h"

#include "state.h"
#include "i2c_comms.h"
#include "remote.h"
#include "notifications.h"
#include "service_hid.h"
#include "key_scan.h"



/*=============================================================================
 *  Private definitions
 *============================================================================*/

/* Instruct the PIO controller on what it should be doing. Note that the values
 * here must match those in the file pio_ctrlr_code.asm (look for CMD_)
 */
#define PIO_CONTROLLER_IDLE     0x0     /* IDLE */
#define PIO_CONTROLLER_KEYSCAN  0x1     /* Do key-scanning */
#define PIO_CONTROLLER_AUDIO    0x2     /* Capture audio */
#define PIO_CONTROLLER_IRTX     0x5     /* Transmit IR */


/*=============================================================================
 *  Private data
 *============================================================================*/
 


/*=============================================================================
 *  Private function declarations
 *============================================================================*/

/* This function reads the latest key-scan information from the shared memory */
static void readKeyData(uint8* data, uint16 dataSize);

/* This function handles key-scan matrix related PIO controller events */
static void handleKeypadEvent(void);

/* This function checks whether sending a notification at this time is possible
 * and desirabled (i.e., that the Central has enabled the notification).
 */
static bool notificationNowIsAppropriate(uint8 reportId);


/*=============================================================================
 *  Private function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleKeypadEvent
 *
 *  DESCRIPTION
 *      This function handles key-scan matrix related PIO controller events.
 *      Mouse and IR interrupts are handled as necessary. If the interrupt is a
 *      button interrupt, first the key data is read from the PIO controller,
 *      then it is converted into a more readable form (button press information
 *      and a HID report) by keyscanProcessScanReport(). We can then carry out
 *      the appropriate actions depending on which key changed. We assume that
 *      only one key will change at a time as key-scan happens rather quickly,
 *      thus it is quite difficult to change two keys within one key-scan
 *      period, even when deliberately trying.
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/
static void handleKeypadEvent(void)
{
    /* Raw key-scan data from PIO controller */
    uint8 keywords[SCAN_MATRIX_ROWS_BYTE_COUNT];
    /* Processed scan report */
    uint8 hidKeypressReport[HID_KEYPRESS_DATA_LENGTH];
    /* Accompanying processed button information */
    BUTTON_SCAN_T buttonInfo;
    
    /* Temporary storage variables */
    bool validKeyPress = FALSE;
    uint8 reportID;
    
    /* Persistent storage variables (across multiple calls to this function) */
    static BUTTON_TYPE lastKeyType = BUTTON_UNKNOWN;
    static uint8 lastNumConsumerKeys = 0;

    /* Check if the interrupt is a button press or something else */
    if(PIO_INTERRUPT_REASON & BUTTON_VALID)
    {
        /* It is, so initialise the HID report as all zeros. */
        MemSet(hidKeypressReport, 0x00, HID_KEYPRESS_DATA_LENGTH);

        /* Copy raw key data from the PIO controller shared memory */
        readKeyData(keywords, SCAN_MATRIX_ROWS_BYTE_COUNT);

        /* Process this key data */
        keyscanProcessScanReport(keywords, hidKeypressReport, &buttonInfo);

#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
        /* IR is handled during key data processing, so we only need to do 
         * something if we are controlling the BLE host (i.e. not in IR mode).
         */
        if(localData.controlledDevice == IRCONTROL_HOST)                
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
        {
            /* Work out which type of button has changed state */
            switch(buttonInfo.pressedButtonType)
            {
#if defined(SPEECH_TX_PRESENT) && !defined(AUDIO_BUTTON_PIO)
                case BUTTON_AUDIO:
                    /* PTT button must have been pressed (releases are handled
                     * in audio mode rather than here in key-scan mode), so
                     * reset the last button report to all zeros and enter audio
                     * transmission mode.
                     */
                    MemSet(localData.latest_button_report, 0, HID_KEYPRESS_DATA_LENGTH);
                    hwHandleAudioButtonPress(TRUE);
                    break;
                    
#endif /* SPEECH_TX_PRESENT */
                default:
                    /* A "normal" button changed state, check if the HID report has changed */
                    if (   (MemCmp(hidKeypressReport, localData.latest_button_report, HID_KEYPRESS_DATA_LENGTH))
                           /* Catch keys with same value (but on different HID pages) */
                        || (lastKeyType != buttonInfo.pressedButtonType))
                    {
                        /* Required action depends on if other keys are currently held down */
                        switch (lastKeyType)
                        {
                            case BUTTON_CONSUMER:
                                /* There is already a Consumer button held down */
                                if (   (buttonInfo.numPressedConsumerKeys == 0)
                                    || (buttonInfo.numPressedConsumerKeys != lastNumConsumerKeys))
                                {
                                    /* The Consumer key was released or an extra one was pressed,
                                     * so a HID report needs to be sent.
                                     */
                                    reportID = HID_CONSUMER_REPORT_ID;
                                    validKeyPress = TRUE;
                                }
                                break;
                            
                            default:
                                /* No key was previously held down. Send the new key press
                                 * report as is, no extra conditioning is required.
                                 */
                                reportID = HID_CONSUMER_REPORT_ID;
                                validKeyPress = TRUE;
                                break;
                        }
                        
                        if (validKeyPress && notificationNowIsAppropriate(reportID))
                        {
                            /* Send the HID report */
                            HidSendInputReport(reportID, hidKeypressReport, FALSE);
                            /* Update persistent states for next time */
                            lastKeyType = buttonInfo.pressedButtonType;
                            lastNumConsumerKeys = buttonInfo.numPressedConsumerKeys;
                        }
                        
                        /* Store the current new button report in appropriate variable */
                        MemCopy(localData.latest_button_report, hidKeypressReport, HID_KEYPRESS_DATA_LENGTH);
        
                        /* If the remote has disconnected from the Central, reconnect now. */
                        WakeRemoteIfRequired();
                    }
                    break;
                    
            }   /* End switch(buttonInfo.pressedButtonType) */
            
        }   /* End if(localData.controlledDevice == IRCONTROL_HOST) */
        
        PIO_CLEAR_INTERRUPT(BUTTON_VALID);
    }

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      notificationNowIsAppropriate
 *
 *  DESCRIPTION
 *      This function checks whether sending a notification at this time is 
 *      possible and desirable (i.e., that the Central has enabled the 
 *      notification).
 *
 *  RETURNS/MODIFIES
 *      TRUE/FALSE - indicates whether the notification should be queued
 *          for transmission.
 *---------------------------------------------------------------------------*/
static bool notificationNowIsAppropriate(uint8 report_id)
{
    return ((localData.state & STATE_CONNECTED_NON_AUDIO) ||
            (  (localData.state < STATE_CONNECTED_IDLE) &&
               (localData.bonded))) &&
            HidIsNotifyEnabledOnReportId(report_id);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      readKeyData
 *
 *  DESCRIPTION
 *      Reads 8-bit key data from the PIO controller shared memory. The PIO
 *      controller scans through the key matrix one row at a time, comparing the
 *      value to the previous value for that row. Each row is 8 bits long, 1 bit
 *      for each column (up to 8 - not all columns have to be used). If any row
 *      has changed, then an interrupt is generated. The PIO controller uses the
 *      shared memory to store each of these rows in turn as it works through
 *      them, so when an interrupt is received we can just look at this data to
 *      see which key changed. A high bit indicates a pressed key, low bits are
 *      keys not currently pressed down. The PIO controller alternates between 2
 *      banks of memory for this purpose, so that the current key data is not
 *      overwritten whilst performing the next scan, and so that it has a record
 *      of the current keys to compare next set to.
 *
 *  PARAMETERS
 *      data        Pointer to target memory which will be written to.
 *      dataSize    Size of data to read and copy, in bytes.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *---------------------------------------------------------------------------*/
static void readKeyData(uint8* data, uint16 dataSize)
{
    uint16* srcMemory = PIO_DATA_BUFFER_START;

    /* First bit tells which bank is being used for the data. Mask off the all
     * other bits and check the first bit to see the bank being used.
     */
    if(PIO_VALID_DATA_BANK & USE_SECOND_DATA_BANK)
    {
        /* use second bank */
        srcMemory += (dataSize >> 1);   /* word -> byte conversion */
    }

    /* copy and unpack the data */
    MemCopyUnPack(data, srcMemory, dataSize);
}


/*=============================================================================
 *  Public function definitions
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      hwHandlePIOControllerEvent
 *
 *  DESCRIPTION
 *      This function handles the pio controller event.
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/
extern void hwHandlePIOControllerEvent(void)
{

    if (PIO_INTERRUPT_REASON & (  BUTTON_VALID
                                | WHEEL_VALID
                               )
       )
    {
        handleKeypadEvent();
    }
}

#if defined AUDIO_BUTTON_PIO
/*----------------------------------------------------------------------------*
 *  NAME
 *      hwConfigureAudioButtonPio
 *
 *  DESCRIPTION
 *      Configures the audio button pio.
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/
extern void hwConfigureAudioButtonPio(void)
{
    /* set up audio button */
    PioSetMode(AUDIO_BUTTON_PIO, pio_mode_user);
    PioSetDir(AUDIO_BUTTON_PIO, FALSE);
    PioSetPullModes((0x01UL << AUDIO_BUTTON_PIO), pio_mode_strong_pull_up);
    /* set the event handler to the audio start button */
    PioSetEventMask((0x01UL << AUDIO_BUTTON_PIO), pio_event_mode_both);

}

#endif /* AUDIO_BUTTON_PIO */
/*----------------------------------------------------------------------------*
 *  NAME
 *      hwSetControllerForKeyscan
 *
 *  DESCRIPTION
 *      Sets the 8051 PIO controller to scanning the keyboard matrix.
 *
 *---------------------------------------------------------------------------*/
extern void hwSetControllerForKeyscan(bool interruptController,bool forceSlowClock)
{
    if (forceSlowClock)
    {
        /* Reset PIO controller clock to 32kHz */
        PioCtrlrClock(FALSE);
    }

    if(interruptController)
    {
        /* Interrupt the PIO controller and set it to "key-scanning" */
        *(uint16*)PIO_CONTROL_WORD = PIO_CONTROLLER_KEYSCAN;
        PioCtrlrInterrupt();
    }

    /* Allow deep sleep */
    SleepModeChange(sleep_mode_deep);
}

#if defined(EXCLUSIVE_I2C_AND_KEYSCAN)||defined(IR_PROTOCOL_IRDB)
/*----------------------------------------------------------------------------*
 *  NAME
 *      hwPauseKeyscan
 *
 *  DESCRIPTION
 *      Pause the 8051 PIO controller key-scanning routine, without
 *      changing the PIO controller mode.
 *
 *---------------------------------------------------------------------------*/
extern void hwPauseKeyscan(void)
{
    /* Ensure that the PIO controller is scanning keys right now */
    if(*(uint16*)PIO_CONTROL_WORD == PIO_CONTROLLER_KEYSCAN)
    {
        /* Pause the key-scanning routine */
        PIO_XAP_TO_CTLR_SEMAPHORE = PIO_CONTROLLER_IDLE;
        
        /* Wait for the controller to stop manipulating the key-scan matrix */
        while(PIO_CTRL_TO_XAP_SEMAPHORE == PIO_CONTROLLER_KEYSCAN)
        {
            TimeDelayUSec(1);
        }
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      hwContinueKeyscan
 *
 *  DESCRIPTION
 *      Resume the 8051 PIO controller key-scanning routine, without
 *      changing the PIO controller mode.
 *
 *---------------------------------------------------------------------------*/
extern void hwContinueKeyscan(void)
{
    if(*(uint16*)PIO_CONTROL_WORD == PIO_CONTROLLER_KEYSCAN){
        PIO_XAP_TO_CTLR_SEMAPHORE = PIO_CONTROLLER_KEYSCAN;
    }
}

#endif /* EXCLUSIVE_I2C_AND_KEYSCAN || IR_PROTOCOL_IRDB */
/*----------------------------------------------------------------------------*
 *  NAME
 *      hwSetControllerForIdle
 *
 *  DESCRIPTION
 *      Sets the 8051 PIO controller to idle.
 *
 *---------------------------------------------------------------------------*/
extern void hwSetControllerIdle()
{
    /* Interrupt the PIO controller and set it to "idle" */
    *(uint16*)PIO_CONTROL_WORD = PIO_CONTROLLER_IDLE;
    *(uint16*)PIO_XAP_TO_CTLR_SEMAPHORE = PIO_CONTROLLER_IDLE;
    while(*(uint16*)PIO_INTERRUPT_REASON);
    
    /* Allow deep sleep */
    SleepModeChange(sleep_mode_deep);
}
