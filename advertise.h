/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    advertise.h
 *
 *  DESCRIPTION
 *    Header file for advertising functions.
 *
 ******************************************************************************/
#ifndef ADVERTISE_H
#define ADVERTISE_H

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/
#include <types.h>
#include <gap_types.h>

/*============================================================================*
 *  Public definitions
 *============================================================================*/

/* This constant is used in the main server app to define some arrays so it
 * should always be large enough to hold the advertisement data.
 */
#define MAX_ADV_DATA_LEN                       (31)

/* Length of Tx Power prefixed with 'Tx Power' AD Type */
#define TX_POWER_VALUE_LENGTH                  (2)

/* Acceptable shortened device name length that can be sent in advData */
#define SHORTENED_DEV_NAME_LEN                 (8)

/*=============================================================================
 *  Public Function Prototypes
 *============================================================================*/

extern void AdvStart(bool fast_connection, gap_mode_connect connect_mode);
extern void AdvStop(void);

#endif /* ADVERTISE_H */