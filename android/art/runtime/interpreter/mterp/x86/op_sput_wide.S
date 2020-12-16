/*
 * SPUT_WIDE handler wrapper.
 *
 */
    /* sput-wide vAA, field@BBBB */
    .extern MterpSet64Static
    EXPORT_PC
    movzwl  2(rPC), %eax
    movl    %eax, OUT_ARG0(%esp)            # field ref BBBB
    leal    VREG_ADDRESS(rINST), %eax
    movl    %eax, OUT_ARG1(%esp)            # &fp[AA]
    movl    OFF_FP_METHOD(rFP), %eax
    movl    %eax, OUT_ARG2(%esp)            # referrer
    movl    rSELF, %ecx
    movl    %ecx, OUT_ARG3(%esp)            # self
    call    SYMBOL(MterpSet64Static)
    testb   %al, %al
    jnz     MterpException
    RESTORE_IBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
