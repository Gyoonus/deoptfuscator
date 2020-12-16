/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    REFRESH_IBASE
    dla     ra, artMterpAsmInstructionStart
    dla     t9, MterpCheckBefore
    move    a0, rSELF
    daddu   a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rPC
    daddu   ra, ra, (${opnum} * 128)            # Addr of primary handler.
    jalr    zero, t9                            # (self, shadow_frame, dex_pc_ptr) Note: tail call.
