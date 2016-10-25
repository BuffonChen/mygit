/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    remote_hw.h
 *
 *  DESCRIPTION
 *    Header file for hardware routines of the remote control application
 *
 ******************************************************************************/
#ifndef _REMOTE_HW_H_
#define _REMOTE_HW_H_

#include <pio_ctrlr.h>
#include <gatt_uuid.h>

/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "configuration.h"


#if defined(IR_PROTOCOL_RC5) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_IRDB)
#include "key_scan.h"
#endif /* IR_PROTOCOL_RC5 || IR_PROTOCOL_NEC || IR_PROTOCOL_IRDB */

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Macros to allow read-access to the PIO-controller registers (first bank) */
#define PIO_CONTROLLER_READ_R0  (uint8)WORD_LSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 0)))
#define PIO_CONTROLLER_READ_R1  (uint8)WORD_MSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 0)))
#define PIO_CONTROLLER_READ_R2  (uint8)WORD_LSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 1)))
#define PIO_CONTROLLER_READ_R3  (uint8)WORD_MSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 1)))
#define PIO_CONTROLLER_READ_R4  (uint8)WORD_LSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 2)))
#define PIO_CONTROLLER_READ_R5  (uint8)WORD_MSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 2)))
#define PIO_CONTROLLER_READ_R6  (uint8)WORD_LSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 3)))
#define PIO_CONTROLLER_READ_R7  (uint8)WORD_MSB((*(uint16*)(PIO_CONTROLLER_RAM_START + 3)))

/* This is the address of the data bank (shared memory buffer for keys and audio) */
#define PIO_DATA_BANK_START     (PIO_CONTROLLER_RAM_START + 24) /* 24 = 0x30 bytes -> words */
#define PIO_DATA_BANK_READ      PIO_CONTROLLER_READ_R4

/* This is the address that indicates which bank holds data valid for this interrupt */
#define PIO_VALID_DATA_BANK     (PIO_DATA_BANK_READ)

/* This is the address that indicates the reason for the interrupt.
 * The masks are defined in configuration.h
 */
#define PIO_INTERRUPT_REASON    (PIO_DATA_BANK_READ)

/* Clear an interrupt (held in R4) */
#define PIO_CLEAR_INTERRUPT(_i_)    (*((uint16*)(PIO_CONTROLLER_RAM_START + 2)) &= (~_i_))

/* This is the XAP-to-8051 semaphore location */
#define PIO_XAP_TO_CTLR_SEMAPHORE   (*(uint16*)(PIO_DATA_BANK_START))

/* This is the 8051-to-XAP semaphore location */
#define PIO_CTRL_TO_XAP_SEMAPHORE   (*(uint16*)(PIO_DATA_BANK_START+1))

/* This is the address at which key-scan data starts */
#define PIO_DATA_BUFFER_START   (PIO_DATA_BANK_START + 2)   
                        /* 2-word offset to allow for the control semaphore */

/* This is the address at which audio data starts */
#define PIO_AUDIO_BUFFER_START  (PIO_DATA_BANK_START)


/* This is the address to which the XAP must write in order to control
 * the behaviour of the PIO-controller (switch from key-scan to audio,
 * for example). It is R6, bank 0.
 */
#define PIO_CONTROL_WORD        (PIO_CONTROLLER_RAM_START + 3)

/* This flag in the control byte 0 is set if the IR waveform has a carrier 
   frequency. If this is cleared the IR waveform will consist of edges. */
#define IR_CARRIER_MODE (1 << 2)
/*=============================================================================
 *  Public function prototypes
 *============================================================================*/
/* This function handles interrupts from the 8051 PIO controller */
extern void hwHandlePIOControllerEvent(void);

/* Configure the 8051 PIO controller to do key-scanning */
extern void hwSetControllerForKeyscan(bool interruptController,bool forceSlowClock);

/* Configure the 8051 PIO controller to transmit IR command */

extern void hwSetControllerIdle(void);
#if defined(EXCLUSIVE_I2C_AND_KEYSCAN)||defined(IR_PROTOCOL_IRDB)
/* Pause key-scanning, without changing the mode of the PIO controller. 
 * Do not call this function unless the 8051 controller is in
 * key-scan mode.
 */
extern void hwPauseKeyscan(void);

/* Resume key-scanning, without changing the mode of the PIO controller. 
 * Do not call this function unless the 8051 controller is in
 * key-scan mode.
 */
extern void hwContinueKeyscan(void);

#endif /* EXCLUSIVE_I2C_AND_KEYSCAN || IR_PROTOCOL_IRDB*/



#endif /* _REMOTE_HW_H_ */
