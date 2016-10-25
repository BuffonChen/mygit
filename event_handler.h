/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    event_handler.c
 *
 *  DESCRIPTION
 *    This file handler funtions for different events
 *
 ******************************************************************************/
#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

/*=============================================================================
 *  SDK Header Files
 *============================================================================*/
#include <gatt.h>

/*=============================================================================
 *  Local Header Files
 *============================================================================*/

/*=============================================================================
 *  Private Function Prototypes
 *============================================================================*/

#if defined(DISCONNECT_ON_IDLE)
/* This function handles reseting the idle timer. */
extern void handleResetIdleTimer(void);
#endif /* DISCONNECT_ON_IDLE */
/* This function handles the "background" tick being received from the FW. */
extern void handleBackgroundTickInd(void);
/* This function handles creating the timer used to trigger transmission of motion data. */
extern void handleCreateReportTimer(void);
/* This function handles clearing existing pairing information. */
extern void handleClearPairing(void);
/* This function handles the signal GATT_ADD_DB_CFM. */
extern void handleSignalGattAddDBCfm(GATT_ADD_DB_CFM_T *p_event_data);
/* This function handles the signal GATT_CONNECT_CFM. */
extern void handleSignalGattConnectCfm(GATT_CONNECT_CFM_T* event_data);
/* This function handles the signal GATT_CANCEL_CONNECT_CFM. */
extern void handleSignalGattCancelConnectCfm(void);
/* This function handles the signal GATT_ACCESS_IND. */
extern void handleSignalGattAccessInd(GATT_ACCESS_IND_T *event_data);
/* This function handles the signal LM_EV_DISCONNECT_COMPLETE. */
extern void handleSignalLmEvDisconnectComplete(HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data);
/* This function handles the signal LM_EV_ENCRYPTION_CHANGE. */
extern void handleSignalLMEncryptionChange(LM_EVENT_T *event_data);
/* This function handles the signal SM_KEYS_IND. */
extern void handleSignalSmKeysInd(SM_KEYS_IND_T *event_data);
/* This function handles the signal SM_SIMPLE_PAIRING_COMPLETE_IND. */
extern void handleSignalSmSimplePairingCompleteInd(SM_SIMPLE_PAIRING_COMPLETE_IND_T *event_data);
/* This function handles the signal SM_PAIRING_AUTH_IND. */
extern void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *event_data);
/* This function handles the signal LS_CONNECTION_PARAM_UPDATE_CFM. */
extern void handleSignalLsConnParamUpdateCfm(LS_CONNECTION_PARAM_UPDATE_CFM_T *event_data);
/* This function handles the signal LS_CONNECTION_PARAM_UPDATE_IND. */
extern void handleSignalLsConnParamUpdateInd(LS_CONNECTION_PARAM_UPDATE_IND_T *event_data);
/* This function handles the signal LM_EV_CONNECTION_UPDATE. */
extern void handleConnectionUpdateInd(LM_EV_CONNECTION_UPDATE_T *event_data);
/* This function handles the signal SM_DIV_APPROVE_IND. */
extern void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data);
/* This function handles the signal LS_RADIO_EVENT_IND. */
extern void handleSignalLsRadioEventInd(void);
/* This function handles the signals GATT_NOT_CHAR_VAL_IND and GATT_IND_CHAR_VAL_IND. */
extern void handleCharValIndCfm(GATT_CHAR_VAL_IND_CFM_T *cfm);
#endif /* EVENT_HANDLER_H */