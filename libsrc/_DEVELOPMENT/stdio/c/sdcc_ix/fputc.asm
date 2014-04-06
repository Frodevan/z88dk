
; int fputc(int c, FILE *stream)

INCLUDE "clib_cfg.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
IF __CLIB_OPT_MULTITHREAD
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

XDEF _fputc

_fputc:

   pop af
   pop de
   pop bc
   
   push bc
   push de
   push af
   
   push ix
   
   ld ixl,c
   ld ixh,b
   
   call asm_fputc
   
   pop ix
   ret
   
   INCLUDE "stdio/z80/asm_fputc.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ELSE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

XDEF _fputc

LIB _fputc_unlocked

_fputc:

   jp _fputc_unlocked
   
   INCLUDE "stdio/z80/asm_fputc.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ENDIF
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
