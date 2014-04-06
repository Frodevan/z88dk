
; int vscanf(const char *format, void *arg)

INCLUDE "clib_cfg.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
IF __CLIB_OPT_MULTITHREAD
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

XDEF _vscanf

_vscanf:

   pop af
   pop de
   pop bc
   
   push bc
   push de
   push af
   
   push ix
   
   call asm_vscanf
   
   pop ix
   ret
   
   INCLUDE "stdio/z80/asm_vscanf.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ELSE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

XDEF _vscanf

LIB _vscanf_unlocked

_vscanf:

   jp _vscanf_unlocked
   
   INCLUDE "stdio/z80/asm_vscanf.asm"

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ENDIF
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
