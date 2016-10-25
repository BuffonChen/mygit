/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      nvm_access.h
 *
 *  DESCRIPTION
 *      Header definitions for NVM usage.
 *
 ******************************************************************************/
#ifndef __NVM_ACCESS_H__
#define __NVM_ACCESS_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <status.h>
#include <nvm.h>

/* Magic value to check the validity of NVM region used by the application */
#define NVM_SANITY_MAGIC                    (0x1357)

/* NVM offset for NVM sanity word */
#define NVM_OFFSET_SANITY_WORD              (0)

/* NVM offset for bonded flag */
#define NVM_OFFSET_BONDED_FLAG              (NVM_OFFSET_SANITY_WORD + 1)

/* NVM offset for bonded device bluetooth address */
#define NVM_OFFSET_BONDED_ADDR              (NVM_OFFSET_BONDED_FLAG + \
                                             sizeof(localData.bonded))

/* NVM offset for diversifier */
#define NVM_OFFSET_SM_DIV                   (NVM_OFFSET_BONDED_ADDR + \
                                             sizeof(localData.bonded_bd_addr))

/* NVM offset for IRK */
#define NVM_OFFSET_SM_IRK                   (NVM_OFFSET_SM_DIV + \
                                             sizeof(localData.diversifier))

#if defined(IR_PROTOCOL_IRDB) || defined(IR_PROTOCOL_NEC) || defined(IR_PROTOCOL_RC5)
/* NVM offset for IR controlled device */
#define NVM_OFFSET_IR_CONTROLLED_DEVICE    (NVM_OFFSET_SM_IRK + \
                                             sizeof(localData.controlledDevice))

/* Number of words of NVM used by core application. Memory used by supported 
 * services is not taken into consideration here.
 */
#define N_APP_USED_NVM_WORDS                (NVM_OFFSET_IR_CONTROLLED_DEVICE + \
                                             MAX_WORDS_IRK)
#else
/* Number of words of NVM used by core application. Memory used by supported 
 * services is not taken into consideration here.
 */
#define N_APP_USED_NVM_WORDS                (NVM_OFFSET_SM_DIV + \
                                             MAX_WORDS_IRK)
#endif /* IR_PROTOCOL_IRDB || IR_PROTOCOL_NEC || IR_PROTOCOL_RC5 */
/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* Read words from the NVM store after preparing the NVM to be readable */
extern sys_status Nvm_Read(uint16* buffer, uint16 length, uint16 offset);

/* Write words to the NVM store after preparing the NVM to be writable */
extern sys_status Nvm_Write(uint16* buffer, uint16 length, uint16 offset);

/* Disable the NVM (to save power) */
extern void Nvm_Disable(void);

#endif /* __NVM_ACCESS_H__ */