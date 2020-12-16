    /* const/4 vA, #+B */
    sbfx    w1, wINST, #12, #4          // w1<- sssssssB
    ubfx    w0, wINST, #8, #4           // w0<- A
    FETCH_ADVANCE_INST 1                // advance xPC, load wINST
    GET_INST_OPCODE ip                  // ip<- opcode from xINST
    SET_VREG w1, w0                     // fp[A]<- w1
    GOTO_OPCODE ip                      // execute next instruction
