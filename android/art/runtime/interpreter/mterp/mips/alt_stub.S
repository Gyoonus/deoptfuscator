/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (${opnum} * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)
