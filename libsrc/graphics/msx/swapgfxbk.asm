;
;       Page the graphics bank in/out - used by all gfx functions
;       Doesn't really page on the MSX.
;
;
;	$Id: swapgfxbk.asm,v 1.7 2017-01-02 22:57:58 aralbrec Exp $
;

		SECTION   code_clib
                PUBLIC    swapgfxbk
                PUBLIC    _swapgfxbk
		EXTERN	pixeladdress

		PUBLIC	swapgfxbk1
      PUBLIC   _swapgfxbk1

.swapgfxbk
._swapgfxbk
		ret

.swapgfxbk1
._swapgfxbk1
                ret
