    /* fill-array-data vAA, +BBBBBBBB */
    EXPORT_PC
    movslq  2(rPC), %rcx                    # rcx <- ssssssssBBBBbbbb
    leaq    (rPC,%rcx,2), OUT_ARG1          # OUT_ARG1 <- PC + ssssssssBBBBbbbb*2
    GET_VREG OUT_32_ARG0, rINSTq            # OUT_ARG0 <- vAA (array object)
    call    SYMBOL(MterpFillArrayData)      # (obj, payload)
    testb   %al, %al                        # 0 means an exception is thrown
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 3
