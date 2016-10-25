/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    motion.h
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/
#ifndef _MOTION_H
#define _MOTION_H

#include "configuration.h"


/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "remote.h"

/*=============================================================================
 *  Public definitions
 *============================================================================*/

/* Possible return codes when trying to obtain new motion data */
typedef enum {
    MOTION_NEW_DATA,    /* New data is available */
    MOTION_NO_DATA,     /* No new data is available */
    MOTION_WARM_UP      /* No new data is available because the devices are not ready */
} MOTION_T;

/*=============================================================================
 *  Public function definitions
 *============================================================================*/
 
/* Initialise the "motion" module */
extern void motionInit(void);

/* Create the next HID report containing motion data */
extern MOTION_T motionCreateNextReport(uint8 report_buffer[LARGEST_HID_REPORT_SIZE]);


/* Set the correct state for sending motion data */
extern void motionSetState(void);

/* Set the accelerometer/gyroscope into low-power state */
extern void motionSetLowPowerMode(void);

#endif /* _MOTION_H */