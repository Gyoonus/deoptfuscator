%default { "helper":"UndefinedConstHandler" }
    /* const/class vAA, type@BBBB */
    /* const/method-handle vAA, method_handle@BBBB */
    /* const/method-type vAA, proto@BBBB */
    /* const/string vAA, string@@BBBB */
    .extern $helper
    EXPORT_PC
    lhu     a0, 2(rPC)                  # a0 <- BBBB
    srl     a1, rINST, 8                # a1 <- AA
    daddu   a2, rFP, OFF_FP_SHADOWFRAME
    move    a3, rSELF
    jal     $helper                     # (index, tgt_reg, shadow_frame, self)
    PREFETCH_INST 2                     # load rINST
    bnez    v0, MterpPossibleException  # let reference interpreter deal with it.
    ADVANCE 2                           # advance rPC
    GET_INST_OPCODE v0                  # extract opcode from rINST
    GOTO_OPCODE v0                      # jump to next instruction
