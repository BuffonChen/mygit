/*****************************************************************************
 Copyright (C) Cambridge Silicon Radio Limited 2012-2013
 
 Notifications and indications are held in a buffer, prior to being sent to 
 the remote device. This buffering is required as the lower layers can handle 
 only one notification/indication at a time, whereas the host may send through 
 messages that contain multiple notifications/indications.
 
 The buffering is implemented as a static array, using a ring-buffer mechanism.
******************************************************************************/

/*============================================================================
 * Interface Header Files
 *============================================================================*/
#include <gatt.h>
#include <mem.h>
#include <timer.h>

/*============================================================================
 * Local Header Files
 *============================================================================*/

#include "notifications.h"
#include "remote.h"
#include "configuration.h"

/*============================================================================
 * Private Definitions
 *============================================================================*/
/* The number of notifications that can be buffered. */
#define MAX_BUFFERED_NOTIFICATIONS      20
/* The maximum length of a notification. */
#define MAX_NOTIFICATION_DATA_LEN_BYTES 20

/* A macro to determine the number of items in the buffer, taking the ring-buffer
 * structure into account.
 */
#define NUM_BUFFERED_ITEMS(_r_, _w_)    ((_r_ <= _w_) ? (_w_ - _r_) : ((MAX_BUFFERED_NOTIFICATIONS - _r_) + _w_))

/* A macro to increment a count to the next position, taking the ring-buffer
 * structure into account.
 */
#define SET_NEXT_POSITION(_v_) {    \
    if (_v_ < (MAX_BUFFERED_NOTIFICATIONS - 1)) _v_ = _v_ + 1;  \
    else _v_ = 0;   \
    }
    
/* A macro to decrement a count to the previous position, taking the ring-buffer
 * structure into account.
 */
#define SET_PREV_POSITION(_v_) {    \
    if (_v_ > 0) _v_ = _v_ - 1;  \
    else _v_ = (MAX_BUFFERED_NOTIFICATIONS - 1);   \
    }

/* The possible internal states of this module. */
typedef enum {
    ALL_QUIET,                  /* No notification/indication response is outstanding */
    NOTIFICATION_OUTSTANDING    /* A notification has been sent and the response is outstanding */
} NOTIFICATION_STATE;

/* This is the structure that defines each element of the notification buffer array. */
typedef struct {
    uint16 handle;
    uint16 dataLenInBytes;
    uint8  notification[MAX_NOTIFICATION_DATA_LEN_BYTES];
} NOTIFICATION_ITEM;

/*============================================================================
 * Private Data
 *============================================================================*/
/* Declare the notification buffer. */
static NOTIFICATION_ITEM notificationBuffer[MAX_BUFFERED_NOTIFICATIONS];

/* This is the position that the next notification (to be buffered) should be written to. */
static uint16 notificationWritePosition = 0;
/* This is the position that the next notification (to be sent) should be read from. */
static uint16 notificationReadPosition = 0;
/* This is the current state of this module. */
static NOTIFICATION_STATE currentState = ALL_QUIET;
/* Indicates that all data should be dropped the next time the transmission result is received. */
static bool dropOnNextRegistration = FALSE;

/*============================================================================
 * Private Function Implementations
 *============================================================================*/
/*----------------------------------------------------------------------------
 *  NAME
 *      sendNextNotification
 *
 *  DESCRIPTION
 *      If there are notifications waiting to be sent, then
 *      this function sends the next notification to the remote device.
 *---------------------------------------------------------------------------*/
static void sendNextNotification(void)
{
    /* If there are notifications in the buffer waiting to be sent... */
    if((currentState == ALL_QUIET) &&
       (NUM_BUFFERED_ITEMS(notificationReadPosition, notificationWritePosition) > 0))
    {
        /* ... try to send the next message in the buffer: */
        GattCharValueNotification(localData.st_ucid, 
                                  notificationBuffer[notificationReadPosition].handle, 
                                  notificationBuffer[notificationReadPosition].dataLenInBytes,
                                  notificationBuffer[notificationReadPosition].notification);
        
        currentState = NOTIFICATION_OUTSTANDING;
    }
}

/****************************************************************************
 *  NAME
 *      bufferItem
 *
 *  DESCRIPTION
 *      Stores a notification in the local buffer, ready to send to the remote
 *      device. If there is no notification currently outstanding, then this
 *      function initiates sending the new notification.
 ****************************************************************************/
static bool bufferItem(uint16 handle, uint16 dataLenInBytes, uint16 *data, bool forceBuffering)
{
    bool itemBuffered = FALSE;  /* Assume that the buffering will fail */
    
    /* If there is no buffer space remaining but buffering is forced, overwrite the
     * previous entry
     */
    if((notificationBufferRemaining() <= 1) &&
       (forceBuffering))
    {
        SET_PREV_POSITION(notificationWritePosition);
    }
    
    /* If there is buffer space remaining... */
    if(notificationBufferRemaining() > 1)
    {
        /* ... copy this notification into the buffer. */
        notificationBuffer[notificationWritePosition].handle = handle;
        notificationBuffer[notificationWritePosition].dataLenInBytes = dataLenInBytes;
        
        if(dataLenInBytes > 1)
        {
            MemCopy(notificationBuffer[notificationWritePosition].notification, data, dataLenInBytes);
        }
        else
        {
            notificationBuffer[notificationWritePosition].notification[0] = ((*data) & 0xff);
        }
        
        /* Increment the buffer item count. */
        SET_NEXT_POSITION(notificationWritePosition);
        
        /* Return that the item was buffered successfully. */
        itemBuffered = TRUE;
    }
    /* else: not enough space to buffer this item */
    
    /* If we are not currently scheduled to send a notification, trigger that now. */
    notificationSendNext();
    
    return itemBuffered;
}

/*============================================================================
 * Public Function Implementations
 *============================================================================*/
/****************************************************************************
 *  NAME
 *      notificationForceBufferItem
 *
 *  DESCRIPTION
 *      Stores a notification in the local buffer, ready to send to the remote
 *      device. If there is no notification currently outstanding, then this
 *      function initiates sending the new notification.
 ****************************************************************************/
extern bool notificationForceBufferItem(uint16 handle, uint16 dataLenInBytes, uint16 *data)
{
    return bufferItem(handle, dataLenInBytes, data, TRUE);
}

/****************************************************************************
 *  NAME
 *      notificationBufferItem
 *
 *  DESCRIPTION
 *      Stores a notification in the local buffer, ready to send to the remote
 *      device. If there is no notification currently outstanding, then this
 *      function initiates sending the new notification.
 ****************************************************************************/
extern bool notificationBufferItem(uint16 handle, uint16 dataLenInBytes, uint16 *data)
{
    return bufferItem(handle, dataLenInBytes, data, FALSE);
}

/****************************************************************************
 *  NAME
 *      notificationSendNext
 *
 *  DESCRIPTION
 *      This function is called when a GATT_CHAR_VAL_IND_CFM is received from
 *      the firmware. If there is anything in the local notification buffer,
 *      then this function initiates transmission.
 ****************************************************************************/
extern void notificationSendNext(void)
{
    /* If there is no notification currently scheduled to be sent... */
    if((currentState == ALL_QUIET) &&
       (localData.blockNotifications == FALSE))
    {
        sendNextNotification();
    }
}

/****************************************************************************
 *  NAME
 *      notificationBufferRemaining
 *
 *  DESCRIPTION
 *      Return the number of empty notification buffer positions.
 ****************************************************************************/
extern uint16 notificationBufferRemaining(void)
{
    /* Return the number of empty buffer positions. */
    return (MAX_BUFFERED_NOTIFICATIONS - NUM_BUFFERED_ITEMS(notificationReadPosition, notificationWritePosition));
}

/****************************************************************************
 *  NAME
 *      notificationRegisterResult
 *
 *  DESCRIPTION
 *      This function is called when a GATT_CHAR_VAL_IND_CFM is received from
 *      the firmware. If the transmission was successful, then the next buffered
 *      notification (if any) is readied for transmission.
 ****************************************************************************/
extern void notificationRegisterResult(bool transmitSucceeded)
{
    if(transmitSucceeded)
    {
        /* Success! Move the read position on one place. */
        SET_NEXT_POSITION(notificationReadPosition);
    }

    currentState = ALL_QUIET;
    
    if(dropOnNextRegistration)
    {
        notificationDropAll();
        dropOnNextRegistration = FALSE;
    }
}

/****************************************************************************
 *  NAME
 *      notificationDropAll
 *
 *  DESCRIPTION
 *      Drop all data from the notification buffers. Typically called on
 *      disconnection.
 ****************************************************************************/
extern void notificationDropAll(void)
{
    if(currentState == ALL_QUIET)
    {
        notificationWritePosition = 0;
        notificationReadPosition = 0;
    }
    else
    {
        dropOnNextRegistration = TRUE;
    }
}
