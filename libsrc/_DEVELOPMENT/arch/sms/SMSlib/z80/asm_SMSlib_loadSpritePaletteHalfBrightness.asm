; **************************************************
; SMSlib - C programming library for the SMS/GG
; ( part of devkitSMS - github.com/sverx/devkitSMS )
; **************************************************

INCLUDE "SMSlib_private.inc"

SECTION code_clib
SECTION code_SMSlib

PUBLIC asm_SMSlib_loadSpritePaletteHalfBrightness

asm_SMSlib_loadSpritePaletteHalfBrightness:

   ; void SMS_loadSpritePaletteHalfBrightness (void *palette)
   ;
   ; enter : hl = void *palette
   ;
   ; uses  : af, b, hl
   
   di
   
   ld a,+(SMS_CRAMAddress + 0x10)&0xff
   out (VDPControlPort),a
   
   ld a,+(SMS_CRAMAddress + 0x10)/256
   out (VDPControlPort),a
   
   ei
   
   ld b,16

loop:

   ld a,(hl)
   inc hl
   
   rrca
   and 0x15
   
   out (VDPDataPort),a

   djnz loop
   ret
