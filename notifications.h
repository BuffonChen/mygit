/*****************************************************************************
 Copyright (C) Cambridge Silicon Radio Limited 2012-2013

 Notifications and indications are held in a buffer, prior to being sent to 
 the remote device. This buffering is required as the lower layers can handle 
 only one notification/indication at a time, whereas the host may send through 
 messages that contain multiple notifications/indications.
******************************************************************************/

#ifndef _NOTIFICATIONS_H
#define _NOTIFICATIONS_H

/*============================================================================
 * Interface Header Files
 *============================================================================*/
#include <types.h>

/*============================================================================
 * Public Function Implementations
 *============================================================================*/
/* Buffer a notification to send to the remote device. */
extern bool notificationBufferItem(uint16 handle, uint16 dataLenInBytes, uint16 *data);
/* Force buffering a notification to send to the remote device (overwrites previously-buffered data). */
extern bool notificationForceBufferItem(uint16 handle, uint16 dataLenInBytes, uint16 *data);
/* Send the next notification from the buffer (if any). */
extern void notificationSendNext(void);
/* Get the number of buffer slots remaining for notifcations. */
extern uint16 notificationBufferRemaining(void);
/* Register the result of the last notification-send attempt. */
extern void notificationRegisterResult(bool transmitSucceeded);
/* Clear all data from the buffers */
extern void notificationDropAll(void);

#endif /* _NOTIFICATIONS_H */
