%include "arm/unopWider.S" {"instr":"bl      f2l_doconv"}

%break
/*
 * Convert the float in r0 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
f2l_doconv:
    ubfx    r2, r0, #23, #8             @ grab the exponent
    cmp     r2, #0xbe                   @ MININT < x > MAXINT?
    bhs     f2l_special_cases
    b       __aeabi_f2lz                @ tail call to convert float to long
f2l_special_cases:
    cmp     r2, #0xff                   @ NaN or infinity?
    beq     f2l_maybeNaN
f2l_notNaN:
    adds    r0, r0, r0                  @ sign bit to carry
    mov     r0, #0xffffffff             @ assume maxlong for lsw
    mov     r1, #0x7fffffff             @ assume maxlong for msw
    adc     r0, r0, #0
    adc     r1, r1, #0                  @ convert maxlong to minlong if exp negative
    bx      lr                          @ return
f2l_maybeNaN:
    lsls    r3, r0, #9
    beq     f2l_notNaN                  @ if fraction is non-zero, it's a NaN
    mov     r0, #0
    mov     r1, #0
    bx      lr                          @ return 0 for NaN
