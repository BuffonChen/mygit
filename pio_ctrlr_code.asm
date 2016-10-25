;*******************************************************************************
;    Copyright (C) Cambridge Silicon Radio Limited 2014
;
; FILE
;    pio_ctrlr_code.asm
;
;  DESCRIPTION
;    This file contains the low level 8051 assembly code for the keypad scanning.
;
;*******************************************************************************

; Various constants:
.equ locat        ,0x0000
.equ STACKBASE    ,0x20

.equ WAKEUP       ,0x9E ; Interrupt/wakeup main XAP system with transition from 1 to 0.
.equ XAP_INT_INFO ,0x04 ; Write to this location to tell the XAP why it has been woken.
                        ; WARNING: XAP_INT_INFO blocks (is the same address as) R4 in bank 0.

; junk variable:
.equ TMP          ,0x2f ; Note that this limits the effective stack size to 0xf bytes

.equ BUFFER_BASE  ,0x30 ; Pointer to valid dual port RAM with XAP
.equ SEM_FROM_XAP ,BUFFER_BASE    ; The semaphore from the XAP
.equ SEM_INTO_XAP ,SEM_FROM_XAP+2 ; The semaphore into the XAP
.equ KEYPTR_BASE  ,SEM_INTO_XAP+2 ; Pointer to shared dual port 0 RAM with XAP
.equ KEYPTR_BASE2 ,KEYPTR_BASE+4  ; Pointer to shared dual port 1 RAM with XAP

.equ KEY_MASK     ,0x1f

; Commands from the XAP
.equ CMD_IDLE          ,0x0  ; Do nothing
.equ CMD_DO_KEYSCAN    ,0x1  ; Do key-scanning

; Data-validity masks - indicate to the XAP the reason for the interrupt.
; This reason value is written into XAP_INT_INFO.
; Bit 0x1 is used to indicate which buffer holds the data for this interrupt.
.equ BUFFER_MASK     ,0x01    ; If set, the data is in buffer 2; else: buffer 1.
.equ BUTTONS_VALID   ,0x02    ; The XAP has been interrupted due to new button data.

; --- Register usage (when scanning keys): ---
; Bank 1, R0 - if set, wake the XAP
; Bank 1, R1 - points to current write location
; Bank 1, R2 (unused)
; Bank 1, R3 - indicates change in key matrix during scan (temporary)
; Bank 1, R4 (unused)
; Bank 1, R5 - temp button state
; Bank 1, R6 - the current valid buffer (0 or 1)
; Bank 1, R7 - points to start of current shared RAM location
    
    

KEYSCAN_INITIALISATION:
    ; Set the stack up
    mov     SP, #(STACKBASE)

    ; Switch to register bank 1
    mov     A, #PSW   ; Load current PSW value
    anl     A, #0xe7  ; Mask out the register bank control bits
    orl     A, #0x08  ; Switch to bank 1
    mov     PSW, A    ; Write back the new PSW value

    ; Initialise the semaphores between XAP and 8051 controller
    mov R1, #SEM_FROM_XAP
    mov  @R1, #CMD_DO_KEYSCAN ; LSB of word
    inc R1
    mov  @R1, #0              ; MSB of word

    mov R1, #SEM_INTO_XAP
    mov  @R1, #CMD_IDLE       ; LSB of word
    inc R1
    mov  @R1, #0              ; MSB of word

;
; Set all the ROW PIOs:
    setb    P2.7            ; Set PIO 0 high
    setb    P2.6            ; Set PIO 1 high
    setb    P2.5            ; Set PIO 2 high
;
; Set all the COLUMN PIOs:
    setb    P3.0
    setb    P3.1
    setb    P3.2
    setb    P3.3
    setb    P3.4
    
    mov     R3, #0              ; clear the temp "button pressed" record
    
; There are 2 copies of the scanned keys in the shared RAM between the
; PIO Controller and the XAP. They flip between one buffer and the other so that
; the first set of scan keys are not over written by the scanning of the 2nd 
; set of keys. The first word in the buffer defines the which buffer
; contains the latest scanned keys.
    
    ; Record that we will be using buffer 0 first.
    mov     R7, #KEYPTR_BASE
    mov     R6, #0
    
SCAN_LOOP:
    mov     R0, #0  ; clear the "wake up XAP now" flag
    mov     R5, #0  ; clear the temporary button state variable
    
;*******************************************************************************
    
    ; Check the flag from the XAP to be sure that key-scanning can go ahead
    mov  R1, #SEM_FROM_XAP    ; 1. Load the semaphore location into R1
    mov  A,  #CMD_DO_KEYSCAN  ; 2. Load A with the value of the "do keyscan" command
    anl  A,  @R1              ; 3. Compare (AND) "do keyscan" with whatever the XAP sent
    jnz  BUTTON_SCAN          ; 4. If there is something left, jump to scanning the keys...
    ajmp END_SCAN_LOOP ;Skip Scanning
    
BUTTON_SCAN:
    ; Button scanning. Drive each output PIO (row) low in turn;
    ; each time, read in all the "column" lines.
    
    ; At this point, key-scanning is going ahead. Indicate this to
    ; the XAP by setting the appropriate semaphore.
    mov R1, #SEM_INTO_XAP     ; Load the semaphore location into R1
    mov  @R1, #CMD_DO_KEYSCAN ; Indicate the current activity
    
    ; Reset R1 to the start of the buffer (from R7)
    mov     A, R7
    mov     R1, A

    ; Row 1 (PIO 23)
    clr     P2.7                ; Set row low
    mov     A, P3               ; Read in the columns
    setb    P2.7                ; Set row high
    lcall   INVERT_AND_STORE    ; Store the value for this row/column combination.
    
    ; Row 2 (PIO 22)
    clr     P2.6                ; Set row low
    mov     A, P3               ; Read in the columns
    setb    P2.6                ; Set row high
    lcall   INVERT_AND_STORE    ; Store the value for this row/column combination.
    
    ; Row 3 (PIO 21)
    clr     P2.5                ; Set row low
    mov     A, P3               ; Read in the columns
    setb    P2.5                ; Set row high
    lcall   INVERT_AND_STORE    ; Store the value for this row/column combination.
    
    ; Dummy entry, to make an even number of bytes
    mov     A, #KEY_MASK
    lcall   INVERT_AND_STORE    ; Store the value for this dummy row/column combination.
    
    ;***************************************************************************
    ; All the column changes are now OR'ed together into R3. If a bit is set
    ; in R3, then a button has been pressed.
    mov     A, R3       ; This is the current reading
    

    jz      NO_BUTTON_CHANGE    ; if: a key is neither pressed nor released (A is zero), 
                                ; then: do not wake the XAP this time round
    mov     R0, #BUTTONS_VALID  ; else: wake the XAP (after the wheel scan)
    mov     A, #0               ; Zero R3
    mov     R3, A               
    
    ;
    ; FALL THRU TO RECORDING BUTTON STATE
    ;
    
    ;***************************************************************************
NO_BUTTON_CHANGE:
    ; Do nothing.
    
    ;
    ; FALL THRU TO WAKING XAP
    ;
    
CHECK_WAKE_XAP:
    ; If R0 is set, we need to wake the XAP; otherwise, there is nothing
    ; to report, so let it sleep.
    mov     A, R0
    jz      END_SCAN_LOOP   ; if R0 is not set, jump to END_SCAN_LOOP
    
    lcall WAKE_XAP          ; If we are here, call the routine to wake the XAP
    
    ;
    ; FALL THRU TO SWITCH BUFFER
    ;
    
SWITCH_BUFFER:
    ; We have woken the XAP to process the new information. To avoid
    ; over-writing the data just supplied, switch now to the other data
    ; buffer.
    
    ; R7 holds the current data buffer. 
    ; If this is not KEYPTR_BASE2, then jump to SWITCH_TO_BUFFER2
    ; else: fall thru to SWITCH_TO_BUFFER1
    
    cjne     R7, #KEYPTR_BASE2, SWITCH_TO_BUFFER2
    
SWITCH_TO_BUFFER1:
     mov     R7, #KEYPTR_BASE   ; Record the new buffer start in R7.
     mov     R6, #0             ; R6 holds the buffer 0/1 record.
     ljmp    END_SCAN_LOOP      ; We are done here: jump to the end.
    
SWITCH_TO_BUFFER2:
     mov     R7, #KEYPTR_BASE2
     mov     R6, #BUFFER_MASK
     
    ; FALL THRU
    
END_SCAN_LOOP:
    ; Indicate to the XAP that the scanning of the keys has now finished
    mov      R1,  #SEM_INTO_XAP   ; Load the semaphore address into R1
    mov      @R1, #CMD_IDLE       ; Set the semaphore value
    ljmp     SCAN_LOOP       ; That's it - once again, from the top.


;*******************************************************************************
; SUBROUTINES BELOW THIS POINT
;*******************************************************************************
;
WAKE_XAP:
    ; A button has been pressed or the wheel has been turned.
    ; Wake up the XAP with an interrupt.
    
    mov     A, R6           ; Indicate to the XAP which buffer is valid
    orl     A, R0           ; and the reason for the interrupt.
    mov     XAP_INT_INFO, A
    
    mov     WAKEUP, #1      ; wake the XAP
    mov     WAKEUP, #0
    
    ret                     ; end sub-routine
    
;*******************************************************************************
INVERT_AND_STORE:
    ; Invert the "column" reading for the buttons and store the result (into R5)
    ; The column reading is in the accumulator.
    
    cpl     A               ; Invert the bits (low -> high), so that a button
                            ; press is indicated by a bit being set.
    anl     A, #(KEY_MASK)  ; Mask off the uninteresting stuff from the rest
                            ; of the I/O port.
    
    mov     R5, A           ; R5 = new reading for THIS column
    mov     A, @R1          ; R1 = old reading for THIS column
    xrl     A, R5           ; A = R1 ^ R5 (bitwise XOR, 0 if equal)
    orl     A, R3           ; R3 marks which rows have changed for ANY column:
    mov     R3, A           ; R3 = R3 | A (we really only care if it's 0 or not)
    mov     A, R5           ; Store new column reading into XAP shared memory:
    mov     @R1, A          ; R1 = R5 (old = new, for this column on next SCAN)
    
    inc     R1              ; Increment the memory pointer, ready for the next
                            ; COLUMN.
    
    ret                     ; end sub-routine
    
