; void wherey()
; 09.2017 stefano

SECTION code_clib
PUBLIC wherey
PUBLIC _wherey

EXTERN __console_x

.wherey
._wherey

	ld	a,(__console_y)
	ld	l,a
	ld	h,0
	ret
