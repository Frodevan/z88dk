;------------------------------------------------------------------------------
; z80asm library
; Substitute for z80 rrd instruction
; Doesn't emulate the flags correctly
; initial z80 version by aralbrec 06.2007
; Copyright (C) Paulo Custodio, 2011-2020
; License: http://www.perlfoundation.org/artistic_license_2_0
; Repository: https://github.com/z88dk/z88dk
;------------------------------------------------------------------------------
      SECTION  code_crt0_sccz80
      PUBLIC   __z80asm__rrd

__z80asm__rrd:

      jr    nc, dorrd

      call  dorrd
      scf   
      ret   

dorrd:

IF __CPU_INTEL__              ; a = xi, (hl) = jk --> a = xk, (hl) = ij
      push  de
      push  hl

      ld    l, (hl)
      ld    h, 0              ; hl = 00jk

      ld    d, a              ; d = xi
      and   0xf0
      ld    e, a              ; e = x0
      ld    a, d
      and   0x0f
      ld    d, a              ; d = a = 0i

      ld    a, l              ; a = jk
      and   0x0f              ; a = 0k
      add   a, e              ; a = xk
      ld    e, a              ; e = xk

      add   hl, hl
      add   hl, hl
      add   hl, hl
      add   hl, hl            ; e = xk, a = xk, hl = 0jk0

      ld    a, d
      add   a, a
      add   a, a
      add   a, a
      add   a, a              ; a = i0
      add   a, h              ; a = ij

      pop   hl
      ld    (hl), a           ; (hl) = ij

      ld    a, e              ; a = xk
      pop   de
ELSE  
      srl   a
      rr    (hl)

      rra   
      rr    (hl)

      rra   
      rr    (hl)

      rra   
      rr    (hl)              ; a = [bits(HL):210, 0, bits(A):7654], carry = bit 3 of (HL)

      rra   
      rra   
      rra   
      rra   
      rra   

ENDIF 

      or    a
      ret   
