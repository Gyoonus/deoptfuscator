%include "arm/unopWide.S" {"instr":"bl      d2l_doconv"}

%break
/*
 * Convert the double in r0/r1 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
d2l_doconv:
    ubfx    r2, r1, #20, #11            @ grab the exponent
    movw    r3, #0x43e
    cmp     r2, r3                      @ MINLONG < x > MAXLONG?
    bhs     d2l_special_cases
    b       __aeabi_d2lz                @ tail call to convert double to long
d2l_special_cases:
    movw    r3, #0x7ff
    cmp     r2, r3
    beq     d2l_maybeNaN                @ NaN?
d2l_notNaN:
    adds    r1, r1, r1                  @ sign bit to carry
    mov     r0, #0xffffffff             @ assume maxlong for lsw
    mov     r1, #0x7fffffff             @ assume maxlong for msw
    adc     r0, r0, #0
    adc     r1, r1, #0                  @ convert maxlong to minlong if exp negative
    bx      lr                          @ return
d2l_maybeNaN:
    orrs    r3, r0, r1, lsl #12
    beq     d2l_notNaN                  @ if fraction is non-zero, it's a NaN
    mov     r0, #0
    mov     r1, #0
    bx      lr                          @ return 0 for NaN
