/*
 * WARNING! This file is auto-generated by the CSR remote-control code generator application.
 * Changes made to this file will be overwritten if the generator application is re-used.
 */

#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

/* These are all the definitions for this build variant.
 * That is to say, if these definitions were applied to the original
 * source code, the result would be the same as this processed code.
 */
#define SUPPORT_CSR_OTAU
#define USE_CS_BLOCK

/* Enable the following define if loading large IR databases over HID. */
/* #define ENABLE_IGNORE_CL_ON_OUTPUT_HID */


#if defined(MOTION_DATA_HILLCREST_FORMAT) && (!defined(ACCELEROMETER_PRESENT) && !defined(GYROSCOPE_PRESENT))
#error "Airmouse support requires both an accelerometer and a gyroscope"
#endif
/* -- end definitions block -- */

/* 
 * Manufacturer/device/version information, for sending over HID
 * 
 * NOTE:
 *  Brackets should not be used around these macro values, as the parser which
 *  creates .c and .h files from .db files does not understand brackets and will
 *  raise syntax errors.
 */
#define PRODUCT_ID                  0xc080
#define PRODUCT_VER                 0x0001
#define VENDOR_ID                   0x1d5a
#define CONF_DEVICE_NAME            'R', 'e', 'm', 'o', 't', 'e'

/*
 * Misc application configuration items
 */

/* This device disconnects from the Central after a period of inactivity */
#define DISCONNECT_ON_IDLE          (1)
/* Parameters for InvenSense gesture detection */
#define GESTURE_SWIPE_MIN_DIST      (500)
#define GESTURE_SWIPE_MAX_NOISE     (300)

/* 
 * PIOs and related information
 */

/* The key-matrix PIOs, used by the PIO controller */
/* All the PIOs under the auspice of the PIO controller: */
#define PIO_CONTROLLER_BIT_MASK		(0x1fe00000UL)
/* The PIOs used specifically in the key-scan matrix: */
#define KEY_MATRIX_PIO_BIT_MASK		(0x1fe00000UL)

/* PIO INITIALISATION */
#define PIOS_TO_PULL_HIGH           (0xe01ffdffUL)  /* These PIOs specifically need to be pulled high */
#define PIOS_TO_PULL_LOW            (0x00000200UL)  /* These PIOs specifically need to be pulled low */

/* Some definitions related to the PIO controller code */
#define USE_SECOND_DATA_BANK        (0x01)
#define BUTTON_VALID                (0x02)
#define WHEEL_VALID                 (0x04)
#define AUDIO_VALID                 (0x08)
#define AUDIO_BUTTON_RELEASE_VALID  (0x10)

/******************************************************************************
 * Macros related to the clearing paired-device information
 ******************************************************************************/

/* One of the buttons on the key-scan matrix can be assigned to be the "clear pairing" button.
 * The key is identified by the HID code assigned to it and by default is configured to be HID
 * code 0x41 (the OK button on the Remote Control Development Board default configuration),
 * unless a specific button is assigned for this function in RCAG.
 *
 * To enable this feature, uncomment the following line:
 */
#define CLEAR_PAIRING_KEY           (0x0223)

/* When not connected, press and hold the button for several seconds to clear the pairing information.
 * The exact period is defined here:
 */
#define CLEAR_PAIRING_TIMER         (2 * SECOND)

/* If your remote control has a dedicated re-pairing button where no HID code is required,
 * it is still necessary to assign a HID code to the button (even though the code will not be transmitted).
 * Where the button is dual-purpose and the HID code is to be transmitted, set this flag; the flag
 * should be deleted otherwise (the HID code is not then transmitted).
 */
#define CLEAR_PAIRING_DUAL_PURPOSE

/******************************************************************************
 * Macros related to the HID service. These should not normally be changed.
 ******************************************************************************/

/* Inputs report IDs */
#define HID_CONSUMER_REPORT_ID              (1)
#define HID_KEYBOARD_REPORT_ID              (2)
#define HID_MOTION_REPORT_ID                (9)
#define HID_MOUSE_REPORT_ID                 (6)
#define HID_AUDIO_INPUT_REPORT_ID           (30)

/* The length of the keypress data in bytes. */
#define HID_KEYPRESS_DATA_LENGTH            (2)

/* Parser version (UINT16) - Version number of the base USB HID specification
 * Format - 0xJJMN (JJ - Major Version Number, M - Minor Version 
 *                  Number and N - Sub-minor version number)
 * NOTE:
 *  Brackets should not be used around these macro values, as the parser does
 *  not understand brackets and will raise syntax errors.
 */
#define HID_FLAG_CLASS_SPEC_RELEASE         0x0213

/* Country Code (UINT8)- Identifies the country for which the hardware is 
 * localized. If the hardware is not localized the value shall be zero.
 */
#define HID_FLAG_COUNTRY_CODE               0x40

#define REMOTE_WAKEUP_SUPPORTED             0x01
#define NORMALLY_CONNECTABLE                0x02

/* If the application supports normally connectable feature, bitwise or the
 * HID_INFO_FLAGS value with NORMALLY_CONNECTABLE
 */
#define HID_INFO_FLAGS                      REMOTE_WAKEUP_SUPPORTED


#endif /* _CONFIGURATION_H */