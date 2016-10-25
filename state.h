/*******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 *  FILE
 *      state.h
 *
 *  DESCRIPTION
 *      Header file for the application states
 *
 ******************************************************************************/

#ifndef STATE_H
#define STATE_H

/*=============================================================================*
 *  SDK Header File
 *============================================================================*/

#include <ls_app_if.h>


/*=============================================================================
 *  Local Header Files
 *============================================================================*/
#include "remote_hw.h"


/*=============================================================================
 *  Public Data types
 *============================================================================*/
typedef enum
{
    /* Initial State */
    STATE_INIT = 0x0,             

    /* Transmitting directed advertisements.
     * These are used only when the remote side has written to the re-connection 
     * address characteristic during the last connection.
     */
    STATE_DIRECT_ADVERT = 0x01, 
    
    /* Transmitting undirected advertisements (at a fast rate) */
    STATE_FAST_ADVERT = 0x02,
    
    /* Transmitting undirected advertisements (at a slow rate) */
    STATE_SLOW_ADVERT = 0x03,

    /* Generic "advertising" state - can be passed as an argument to stateSet() 
     * and allows the state machine to determine the most appropriate advertising 
     * state.
     */
    STATE_ADVERTISING = 0x04,   
    
    /* Remote has established connection to the Central but is not currently 
     * sending data.
     */
    STATE_CONNECTED_IDLE = 0x20,
    
    /* Remote has established connection to the Central and is currently 
     * sending non-audio data.
     */
    STATE_CONNECTED_MOTION = 0x40,
    
    /* Remote has established connection to the Central and is currently 
     * sending audio data.
     */
    STATE_CONNECTED_AUDIO = 0x80,
    
    /* Convenience state used for checking for some sort of connection. Do NOT
     * pass as the parameter in stateSet().
     */
    STATE_CONNECTED = (STATE_CONNECTED_IDLE | STATE_CONNECTED_MOTION | STATE_CONNECTED_AUDIO),
    
    /* Convenience state used for checking for some sort of non-audio connection. 
     * Do NOT pass as the parameter in stateSet().
     */
    STATE_CONNECTED_NON_AUDIO = (STATE_CONNECTED_IDLE | STATE_CONNECTED_MOTION),
                                 
    /* Entered when disconnect is initiated by this device */
    STATE_DISCONNECTING = 0x05,  
    
    /* The remote control is idle, with no connections */
    STATE_IDLE = 0x06
    
} CURRENT_STATE_T;


/*=============================================================================
 *  Public function prototypes
 *============================================================================*/
extern void stateSet(CURRENT_STATE_T new_state);


/* Set state to STATE_DISCONNECTING with the specified error code for the 
 * disconnect
 */
extern void stateSetDisconnect(ls_err disconnect_reason);

#endif /* STATE_H */
