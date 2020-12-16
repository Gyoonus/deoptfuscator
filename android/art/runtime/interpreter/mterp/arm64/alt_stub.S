/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    ldr    xIBASE, [xSELF, #THREAD_CURRENT_IBASE_OFFSET]            // refresh IBASE.
    adr    lr, artMterpAsmInstructionStart + (${opnum} * 128)       // Addr of primary handler.
    mov    x0, xSELF
    add    x1, xFP, #OFF_FP_SHADOWFRAME
    mov    x2, xPC
    b      MterpCheckBefore     // (self, shadow_frame, dex_pc_ptr) Note: tail call.
