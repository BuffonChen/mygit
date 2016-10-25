/*******************************************************************************
 *    Copyright (C) Cambridge Silicon Radio Limited 2013
 *
 * FILE
 *    appearance.h
 *
 *  DESCRIPTION
 *    This file defines macros for commonly used appearance values, which are 
 *    defined by BT SIG.
 *
 ******************************************************************************/

#ifndef __APPEARANCE_H__
#define __APPEARANCE_H__

/*=============================================================================*
 *         Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser 
 * which creates .c and .h files from .db file doesn't understand  brackets 
 * and will raise syntax errors.
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/characteristics/
 * Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.
 * appearance.xml
 */

/* Keyboard appearance value */
#define APPEARANCE_KEYBOARD_VALUE           0x03C1

/* Mouse appearance value */
#define APPEARANCE_MOUSE_VALUE              0x03C2

/* Remote Control appearance value */
#define APPEARANCE_REMOTE_VALUE             0x0180

#endif /* __APPEARANCE_H__ */
