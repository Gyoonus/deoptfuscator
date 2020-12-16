/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

public class Main {

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /**
   * Test merging of `NOT+AND` into `BIC`.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAnd(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:i\d+>>          And [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAnd(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<NegOp:i\d+>>       BitwiseNegatedRight [<<Base>>,<<Mask>>] kind:And
  /// CHECK:                            Return [<<NegOp>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAnd(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Not
  /// CHECK-NOT:                        And

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAnd(int, int) disassembly (after)
  /// CHECK:                            bic w{{\d+}}, w{{\d+}}, w{{\d+}}


  /// CHECK-START-ARM:   int Main.$opt$noinline$notAnd(int, int) instruction_simplifier_arm (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:i\d+>>          And [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notAnd(int, int) instruction_simplifier_arm (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<NegOp:i\d+>>       BitwiseNegatedRight [<<Base>>,<<Mask>>] kind:And
  /// CHECK:                            Return [<<NegOp>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notAnd(int, int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        Not
  /// CHECK-NOT:                        And

  /// CHECK-START-ARM:   int Main.$opt$noinline$notAnd(int, int) disassembly (after)
  /// CHECK:                            bic r{{\d+}}, r{{\d+}}, r{{\d+}}

  public static int $opt$noinline$notAnd(int base, int mask) {
    if (doThrow) throw new Error();
    return base & ~mask;
  }

  /**
   * Test merging of `NOT+ORR` into `ORN`.
   */

  /// CHECK-START-ARM64: long Main.$opt$noinline$notOr(long, long) instruction_simplifier_arm64 (before)
  /// CHECK:       <<Base:j\d+>>        ParameterValue
  /// CHECK:       <<Mask:j\d+>>        ParameterValue
  /// CHECK:       <<Not:j\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:j\d+>>          Or [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM64: long Main.$opt$noinline$notOr(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:       <<Base:j\d+>>        ParameterValue
  /// CHECK:       <<Mask:j\d+>>        ParameterValue
  /// CHECK:       <<NegOp:j\d+>>       BitwiseNegatedRight [<<Base>>,<<Mask>>] kind:Or
  /// CHECK:                            Return [<<NegOp>>]

  /// CHECK-START-ARM64: long Main.$opt$noinline$notOr(long, long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Not
  /// CHECK-NOT:                        Or

  /// CHECK-START-ARM64: long Main.$opt$noinline$notOr(long, long) disassembly (after)
  /// CHECK:                            orn x{{\d+}}, x{{\d+}}, x{{\d+}}


  /// CHECK-START-ARM:   long Main.$opt$noinline$notOr(long, long) instruction_simplifier_arm (before)
  /// CHECK:       <<Base:j\d+>>        ParameterValue
  /// CHECK:       <<Mask:j\d+>>        ParameterValue
  /// CHECK:       <<Not:j\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:j\d+>>          Or [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM:   long Main.$opt$noinline$notOr(long, long) instruction_simplifier_arm (after)
  /// CHECK:       <<Base:j\d+>>        ParameterValue
  /// CHECK:       <<Mask:j\d+>>        ParameterValue
  /// CHECK:       <<NegOp:j\d+>>       BitwiseNegatedRight [<<Base>>,<<Mask>>] kind:Or
  /// CHECK:                            Return [<<NegOp>>]

  /// CHECK-START-ARM:   long Main.$opt$noinline$notOr(long, long) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        Not
  /// CHECK-NOT:                        Or

  /// CHECK-START-ARM:   long Main.$opt$noinline$notOr(long, long) disassembly (after)
  /// CHECK:                            orn r{{\d+}}, r{{\d+}}, r{{\d+}}

  public static long $opt$noinline$notOr(long base, long mask) {
    if (doThrow) throw new Error();
    return base | ~mask;
  }

  /**
   * Test merging of `NOT+EOR` into `EON`.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$notXor(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:i\d+>>          Xor [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notXor(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<NegOp:i\d+>>       BitwiseNegatedRight [<<Base>>,<<Mask>>] kind:Xor
  /// CHECK:                            Return [<<NegOp>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notXor(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Not
  /// CHECK-NOT:                        Xor

  /// CHECK-START-ARM64: int Main.$opt$noinline$notXor(int, int) disassembly (after)
  /// CHECK:                            eon w{{\d+}}, w{{\d+}}, w{{\d+}}


  /// CHECK-START-ARM:   int Main.$opt$noinline$notXor(int, int) instruction_simplifier_arm (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:i\d+>>          Xor [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notXor(int, int) instruction_simplifier_arm (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op:i\d+>>          Xor [<<Base>>,<<Not>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notXor(int, int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        BitwiseNegatedRight

  public static int $opt$noinline$notXor(int base, int mask) {
    if (doThrow) throw new Error();
    return base ^ ~mask;
  }

  /**
   * Check that transformation is done when the argument is a constant.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAndConstant(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Constant:i\d+>>    IntConstant
  /// CHECK:       <<Not:i\d+>>         Not [<<Base>>]
  /// CHECK:       <<Op:i\d+>>          And [<<Not>>,<<Constant>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAndConstant(int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Constant:i\d+>>    IntConstant
  /// CHECK:       <<NegOp:i\d+>>       BitwiseNegatedRight [<<Constant>>,<<Base>>] kind:And
  /// CHECK:                            Return [<<NegOp>>]


  /// CHECK-START-ARM:   int Main.$opt$noinline$notAndConstant(int) instruction_simplifier_arm (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Constant:i\d+>>    IntConstant
  /// CHECK:       <<Not:i\d+>>         Not [<<Base>>]
  /// CHECK:       <<Op:i\d+>>          And [<<Not>>,<<Constant>>]
  /// CHECK:                            Return [<<Op>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notAndConstant(int) instruction_simplifier_arm (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Constant:i\d+>>    IntConstant
  /// CHECK:       <<NegOp:i\d+>>       BitwiseNegatedRight [<<Constant>>,<<Base>>] kind:And
  /// CHECK:                            Return [<<NegOp>>]

  public static int $opt$noinline$notAndConstant(int mask) {
    if (doThrow) throw new Error();
    return 0xf & ~mask;
  }

  /**
   * Check that no transformation is done when Not has multiple uses.
   */

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAndMultipleUses(int, int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<One:i\d+>>         IntConstant
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op1:i\d+>>         And [<<Not>>,<<One>>]
  /// CHECK:       <<Op2:i\d+>>         And [<<Base>>,<<Not>>]
  /// CHECK:       <<Add:i\d+>>         Add [<<Op1>>,<<Op2>>]
  /// CHECK:                            Return [<<Add>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAndMultipleUses(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<One:i\d+>>         IntConstant
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op1:i\d+>>         And [<<Not>>,<<One>>]
  /// CHECK:       <<Op2:i\d+>>         And [<<Base>>,<<Not>>]
  /// CHECK:       <<Add:i\d+>>         Add [<<Op1>>,<<Op2>>]
  /// CHECK:                            Return [<<Add>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$notAndMultipleUses(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        BitwiseNegatedRight


  /// CHECK-START-ARM:   int Main.$opt$noinline$notAndMultipleUses(int, int) instruction_simplifier_arm (before)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<One:i\d+>>         IntConstant
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op1:i\d+>>         And [<<Not>>,<<One>>]
  /// CHECK:       <<Op2:i\d+>>         And [<<Base>>,<<Not>>]
  /// CHECK:       <<Add:i\d+>>         Add [<<Op1>>,<<Op2>>]
  /// CHECK:                            Return [<<Add>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notAndMultipleUses(int, int) instruction_simplifier_arm (after)
  /// CHECK:       <<Base:i\d+>>        ParameterValue
  /// CHECK:       <<Mask:i\d+>>        ParameterValue
  /// CHECK:       <<One:i\d+>>         IntConstant
  /// CHECK:       <<Not:i\d+>>         Not [<<Mask>>]
  /// CHECK:       <<Op1:i\d+>>         And [<<Not>>,<<One>>]
  /// CHECK:       <<Op2:i\d+>>         And [<<Base>>,<<Not>>]
  /// CHECK:       <<Add:i\d+>>         Add [<<Op1>>,<<Op2>>]
  /// CHECK:                            Return [<<Add>>]

  /// CHECK-START-ARM:   int Main.$opt$noinline$notAndMultipleUses(int, int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        BitwiseNegatedRight

  public static int $opt$noinline$notAndMultipleUses(int base, int mask) {
    if (doThrow) throw new Error();
    int tmp = ~mask;
    return (tmp & 0x1) + (base & tmp);
  }

  /**
   * Check that no transformation is done when both inputs are Not's.
   */

  // We don't check the instructions before the pass, since if De Morgan's laws
  // have been applied then Not/Not/Or is replaced by And/Not.

  /// CHECK-START-ARM64: int Main.$opt$noinline$deMorganOr(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        BitwiseNegatedRight

  /// CHECK-START-ARM:   int Main.$opt$noinline$deMorganOr(int, int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        BitwiseNegatedRight

  public static int $opt$noinline$deMorganOr(int a, int b) {
    if (doThrow) throw new Error();
    return ~a | ~b;
  }

  public static void main(String[] args) {
    assertIntEquals(0xe,   $opt$noinline$notAnd(0xf, 0x1));
    assertLongEquals(~0x0, $opt$noinline$notOr(0xf, 0x1));
    assertIntEquals(~0xe,  $opt$noinline$notXor(0xf, 0x1));
    assertIntEquals(0xe,  $opt$noinline$notAndConstant(0x1));
    assertIntEquals(0xe,   $opt$noinline$notAndMultipleUses(0xf, 0x1));
    assertIntEquals(~0x1,  $opt$noinline$deMorganOr(0x3, 0x1));
  }
}
