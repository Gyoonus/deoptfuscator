%default { "gt_bias":"0" }
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * for: cmpl-float, cmpg-float
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8
    GET_VREG_F(ft0, a2)
    GET_VREG_F(ft1, a3)
#ifdef MIPS32REVGE6
    cmp.eq.s  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if $gt_bias
    cmp.lt.s  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.s  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.s    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if $gt_bias
    c.olt.s   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.s   fcc0, ft1, ft0
    li        rTEMP, 1
    bc1t      fcc0, 1f                     # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#endif
1:
    GET_OPA(rOBJ)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(rTEMP, rOBJ, t0)         #  vAA <- rTEMP
