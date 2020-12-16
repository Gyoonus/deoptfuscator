%default {"wide":"", "r1":"s1", "r2":"s2", "cond":"lt"}
    /*
     * Compare two floating-point values.  Puts 0, 1, or -1 into the
     * destination register based on the results of the comparison.
     */
    /* op vAA, vBB, vCC */
    FETCH w0, 1                         // w0<- CCBB
    lsr     w4, wINST, #8               // w4<- AA
    and     w2, w0, #255                // w2<- BB
    lsr     w3, w0, #8                  // w3<- CC
    GET_VREG$wide $r1, w2
    GET_VREG$wide $r2, w3
    fcmp $r1, $r2
    cset w0, ne
    cneg w0, w0, $cond
    FETCH_ADVANCE_INST 2                // advance rPC, load rINST
    GET_INST_OPCODE ip                  // extract opcode from rINST
    SET_VREG w0, w4                     // vAA<- w0
    GOTO_OPCODE ip                      // jump to next instruction
