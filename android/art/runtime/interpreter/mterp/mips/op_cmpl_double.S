%default { "gt_bias":"0" }
    /*
     * Compare two floating-point values. Puts 0(==), 1(>), or -1(<)
     * into the destination register based on the comparison results.
     *
     * For: cmpl-double, cmpg-double
     */
    /* op vAA, vBB, vCC */

    FETCH(a0, 1)                           #  a0 <- CCBB
    and       rOBJ, a0, 255                #  rOBJ <- BB
    srl       t0, a0, 8                    #  t0 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[BB]
    EAS2(t0, rFP, t0)                      #  t0 <- &fp[CC]
    LOAD64_F(ft0, ft0f, rOBJ)
    LOAD64_F(ft1, ft1f, t0)
#ifdef MIPS32REVGE6
    cmp.eq.d  ft2, ft0, ft1
    li        rTEMP, 0
    bc1nez    ft2, 1f                      # done if vBB == vCC (ordered)
    .if $gt_bias
    cmp.lt.d  ft2, ft0, ft1
    li        rTEMP, -1
    bc1nez    ft2, 1f                      # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    cmp.lt.d  ft2, ft1, ft0
    li        rTEMP, 1
    bc1nez    ft2, 1f                      # done if vBB > vCC (ordered)
    li        rTEMP, -1                    # vBB < vCC or unordered
    .endif
#else
    c.eq.d    fcc0, ft0, ft1
    li        rTEMP, 0
    bc1t      fcc0, 1f                     # done if vBB == vCC (ordered)
    .if $gt_bias
    c.olt.d   fcc0, ft0, ft1
    li        rTEMP, -1
    bc1t      fcc0, 1f                     # done if vBB < vCC (ordered)
    li        rTEMP, 1                     # vBB > vCC or unordered
    .else
    c.olt.d   fcc0, ft1, ft0
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
