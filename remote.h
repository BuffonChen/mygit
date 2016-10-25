/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      remote.h  -  user application for a BTLE remote
 *
 *  DESCRIPTION
 *      Header file for a sample remote application.
 *
 ******************************************************************************/

#ifndef __REMOTE_H__
#define __REMOTE_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bluetooth.h>
#include <timer.h>

/*=============================================================================*
 *  Local Header File
 *============================================================================*/

#include "app_gatt.h"
#include "app_gatt_db.h"
#include "gap_conn_params.h"
#include "configuration.h"
#include "state.h"

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/
/* Maximum number of words in central device IRK */
#define MAX_WORDS_IRK                   (8)

/* Number of IRKs that application can store */
#define MAX_NUMBER_IRK_STORED           (1)

/* HID service may use different reports of different sizes.
 * This macro indicates the size of the largest possible report.
 */
#define LARGEST_HID_REPORT_SIZE         (1) /* Not actually used in this build version */
#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
/* Universal IR controlled devices */
#define IRCONTROL_HOST     (0)
#define IR_NEC_RC5_DEVICE  (1)
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
/* Convert a word-count to bytes, byte-count to words */
#define WORDS_TO_BYTES(_w_)     (_w_ << 1)
#define BYTES_TO_WORDS(_b_)     (((_b_) + 1) >> 1)

/* Default reason generated for disconnection events.
 * This is defined by the standards so should not be changed
 * (defined here as single point of truth)
 */
#define DEFAULT_DISCONNECTION_REASON  (ls_err_oetc_user)


/*=============================================================================*
 *  Public Data Types
 *============================================================================*/

/* Structure defined for Central device IRK. A device can store more than one
 * IRK for the same remote device. This structure can be extended to accommodate
 * more IRKs for the same remote device.
 */
typedef struct
{
    uint16  irk[MAX_WORDS_IRK];

} CENTRAL_DEVICE_IRK_T;

typedef struct
{
    /* The current application state. */
    CURRENT_STATE_T state;

    /* The "when to stop advertising" timer used in 'FAST_ADVERTISING' and
     * 'SLOW_ADVERTISING' states. 
     */
    timer_id advertising_tid;

    /* Timer to allow the remote device to re-encrypt a bonded link using
     * the old keys.
     */
    timer_id recrypt_tid;

    /* A counter to keep track of the number of times the application has tried 
     * to send L2CAP connection parameter update request. When this reaches 
     * MAX_NUM_CONN_PARAM_UPDATE_REQS, the application stops re-attempting to
     * update the connection parameters.
     */
    /* The connection-parameter-update mechanism is active */
    bool conn_param_counter_active;
    /* The number of times connection parameter update requests have been sent. */
    uint8 conn_param_update_count;
    /* The tick count (interval) between update requests. */
    uint8 conn_param_update_tick_count;
    
#if defined(DISCONNECT_ON_IDLE)
    /* The number of ticks received (in IDLE mode) on the way to disconnecting
     * from the central device.
     */
    uint8 disconnect_counter;
#endif /* DISCONNECT_ON_IDLE */

    /* Reason to use when disconnecting through the state machine. 
     * Unless set we will report Remote User Terminated Connection (15,0x0f)
     */
    ls_err disconnect_reason;

    /* The address of the Central to which this remote is connected. */
    TYPED_BD_ADDR_T con_bd_addr;

    /* Track the UCID as Clients connect and disconnect */
    uint16 st_ucid;

    /* Boolean flag to indicated whether the device is bonded */
    bool bonded;

    /* TYPED_BD_ADDR_T of the host to which remote is bonded. */
    TYPED_BD_ADDR_T bonded_bd_addr;

    /* Diversifier associated with the LTK of the bonded device */
    uint16 diversifier;

    /* Central Private Address Resolution IRK. This is used when the
     * central device uses a resolvable random address. 
     */
    CENTRAL_DEVICE_IRK_T central_device_irk;

    /* Boolean flag to indicate whether encryption is enabled with the bonded 
     * host
     */
    bool encrypt_enabled;

    /* Boolean flag set to indicate pairing button press */
    bool pairing_button_pressed;

    /* This variable stores information on the latest button-press activity
     * that may still need to be sent to the Central.
     */
    uint8 latest_button_report[HID_KEYPRESS_DATA_LENGTH];

#if defined(ACCELEROMETER_PRESENT) || defined(GYROSCOPE_PRESENT) || defined(SPEECH_TX_PRESENT)
    /* The latest HID motion data to be sent to the Central */
    uint8 latest_motion_report[LARGEST_HID_REPORT_SIZE];
#endif /* ACCELEROMETER_PRESENT || GYROSCOPE_PRESENT || SPEECH_TX_PRESENT */


    /* Timer to maintain a timer difference of 7.5ms between two input reports.
     */
    timer_id next_report_timer_id;
    uint16 actual_latency;
    uint16 actual_timeout;
    uint16 actual_interval;
    


    
    /* Flag to indicate whether transmission of notifications can happen
     * at this moment (or not).
     */
    bool blockNotifications;

#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
    /* Device controlled by the rc at the moment */
    uint8 controlledDevice;

#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
} LOCAL_DATA_T;

/*=============================================================================*
 *  Public Data
 *============================================================================*/

/* The data structure used by the remote application. */
extern LOCAL_DATA_T localData;

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

extern void RemoteDataInit(void);
extern void WakeRemoteIfRequired(void);

#endif /* __REMOTE_H__ */
