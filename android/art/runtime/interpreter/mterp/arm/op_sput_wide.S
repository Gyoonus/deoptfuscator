    /*
     * SPUT_WIDE handler wrapper.
     *
     */
    /* sput-wide vAA, field@BBBB */
    .extern MterpSet64Static
    EXPORT_PC
    FETCH   r0, 1                       @ r0<- field ref BBBB
    mov     r1, rINST, lsr #8           @ r1<- AA
    VREG_INDEX_TO_ADDR r1, r1
    ldr     r2, [rFP, #OFF_FP_METHOD]
    mov     r3, rSELF
    PREFETCH_INST 2                     @ Get next inst, but don't advance rPC
    bl      MterpSet64Static
    cmp     r0, #0                      @ 0 on success, -1 on failure
    bne     MterpException
    ADVANCE 2                           @ Past exception point - now advance rPC
    GET_INST_OPCODE ip                  @ extract opcode from rINST
    GOTO_OPCODE ip                      @ jump to next instruction
