/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      remote_gatt.h
 *
 *  DESCRIPTION
 *      Header file for remote GATT-related routines
 *
 ******************************************************************************/

#ifndef __REMOTE_GATT_H__
#define __REMOTE_GATT_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/
extern void GattHandleAccessInd(GATT_ACCESS_IND_T *p_ind);
extern bool IsAddressResolvableRandom(TYPED_BD_ADDR_T *addr);
extern bool IsAddressNonResolvableRandom(TYPED_BD_ADDR_T *addr);


#endif /* __REMOTE_GATT_H__ */
