/*
 * SPUT_WIDE handler wrapper.
 *
 */
    /* sput-wide vAA, field@BBBB */
    .extern MterpSet64Static
    EXPORT_PC
    movzwq  2(rPC), OUT_ARG0                # field ref BBBB
    leaq    VREG_ADDRESS(rINSTq), OUT_ARG1  # &fp[AA]
    movq    OFF_FP_METHOD(rFP), OUT_ARG2    # referrer
    movq    rSELF, OUT_ARG3                 # self
    call    SYMBOL(MterpSet64Static)
    testb   %al, %al
    jnz     MterpException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
