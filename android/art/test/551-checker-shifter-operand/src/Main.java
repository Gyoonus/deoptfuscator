/*
 * Copyright (C) 2015 The Android Open Source Project
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

  public static void assertByteEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertShortEquals(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

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

  // Non-inlinable type-casting helpers.
  static  char $noinline$byteToChar   (byte v) { if (doThrow) throw new Error(); return  (char)v; }
  static short $noinline$byteToShort  (byte v) { if (doThrow) throw new Error(); return (short)v; }
  static   int $noinline$byteToInt    (byte v) { if (doThrow) throw new Error(); return   (int)v; }
  static  long $noinline$byteToLong   (byte v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$charToByte   (char v) { if (doThrow) throw new Error(); return  (byte)v; }
  static short $noinline$charToShort  (char v) { if (doThrow) throw new Error(); return (short)v; }
  static   int $noinline$charToInt    (char v) { if (doThrow) throw new Error(); return   (int)v; }
  static  long $noinline$charToLong   (char v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$shortToByte (short v) { if (doThrow) throw new Error(); return  (byte)v; }
  static  char $noinline$shortToChar (short v) { if (doThrow) throw new Error(); return  (char)v; }
  static   int $noinline$shortToInt  (short v) { if (doThrow) throw new Error(); return   (int)v; }
  static  long $noinline$shortToLong (short v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$intToByte     (int v) { if (doThrow) throw new Error(); return  (byte)v; }
  static  char $noinline$intToChar     (int v) { if (doThrow) throw new Error(); return  (char)v; }
  static short $noinline$intToShort    (int v) { if (doThrow) throw new Error(); return (short)v; }
  static  long $noinline$intToLong     (int v) { if (doThrow) throw new Error(); return  (long)v; }
  static  byte $noinline$longToByte   (long v) { if (doThrow) throw new Error(); return  (byte)v; }
  static  char $noinline$longToChar   (long v) { if (doThrow) throw new Error(); return  (char)v; }
  static short $noinline$longToShort  (long v) { if (doThrow) throw new Error(); return (short)v; }
  static   int $noinline$longToInt    (long v) { if (doThrow) throw new Error(); return   (int)v; }

  /**
   * Basic test merging a bitfield move operation (here a type conversion) into
   * the shifter operand.
   */

  /// CHECK-START-ARM: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm (before)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:       <<tmp:j\d+>>         TypeConversion [<<b>>]
  /// CHECK:                            Sub [<<l>>,<<tmp>>]

  /// CHECK-START-ARM: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm (after)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:                            DataProcWithShifterOp [<<l>>,<<b>>] kind:Sub+SXTB

  /// CHECK-START-ARM: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        TypeConversion
  /// CHECK-NOT:                        Sub

  /// CHECK-START-ARM: long Main.$opt$noinline$translate(long, byte) disassembly (after)
  /// CHECK:                            subs r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK:                            sbc r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:       <<tmp:j\d+>>         TypeConversion [<<b>>]
  /// CHECK:                            Sub [<<l>>,<<tmp>>]

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:   <<l:j\d+>>           ParameterValue
  /// CHECK-DAG:   <<b:b\d+>>           ParameterValue
  /// CHECK:                            DataProcWithShifterOp [<<l>>,<<b>>] kind:Sub+SXTB

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        TypeConversion
  /// CHECK-NOT:                        Sub

  /// CHECK-START-ARM64: long Main.$opt$noinline$translate(long, byte) disassembly (after)
  /// CHECK:                            sub x{{\d+}}, x{{\d+}}, w{{\d+}}, sxtb

  public static long $opt$noinline$translate(long l, byte b) {
    if (doThrow) throw new Error();
    long tmp = (long)b;
    return l - tmp;
  }


  /**
   * Test that we do not merge into the shifter operand when the left and right
   * inputs are the the IR.
   */

  /// CHECK-START-ARM: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm (before)
  /// CHECK:       <<a:i\d+>>           ParameterValue
  /// CHECK:       <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<tmp:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<tmp>>,<<tmp>>]

  /// CHECK-START-ARM: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm (after)
  /// CHECK-DAG:   <<a:i\d+>>           ParameterValue
  /// CHECK-DAG:   <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<Shl:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<Shl>>,<<Shl>>]

  /// CHECK-START-ARM: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<a:i\d+>>           ParameterValue
  /// CHECK:       <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<tmp:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<tmp>>,<<tmp>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:   <<a:i\d+>>           ParameterValue
  /// CHECK-DAG:   <<Const2:i\d+>>      IntConstant 2
  /// CHECK:       <<Shl:i\d+>>         Shl [<<a>>,<<Const2>>]
  /// CHECK:                            Add [<<Shl>>,<<Shl>>]

  /// CHECK-START-ARM64: int Main.$opt$noinline$sameInput(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  public static int $opt$noinline$sameInput(int a) {
    if (doThrow) throw new Error();
    int tmp = a << 2;
    return tmp + tmp;
  }

  /**
   * Check that we perform the merge for multiple uses.
   */

  /// CHECK-START-ARM: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm (before)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:       <<Const23:i\d+>>     IntConstant 23
  /// CHECK:       <<tmp:i\d+>>         Shl [<<arg>>,<<Const23>>]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]

  /// CHECK-START-ARM: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm (after)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23

  /// CHECK-START-ARM: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        Shl
  /// CHECK-NOT:                        Add

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (before)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:       <<Const23:i\d+>>     IntConstant 23
  /// CHECK:       <<tmp:i\d+>>         Shl [<<arg>>,<<Const23>>]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]
  /// CHECK:                            Add [<<tmp>>,{{i\d+}}]

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (after)
  /// CHECK:       <<arg:i\d+>>         ParameterValue
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23
  /// CHECK:                            DataProcWithShifterOp [{{i\d+}},<<arg>>] kind:Add+LSL shift:23

  /// CHECK-START-ARM64: int Main.$opt$noinline$multipleUses(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Shl
  /// CHECK-NOT:                        Add

  public static int $opt$noinline$multipleUses(int arg) {
    if (doThrow) throw new Error();
    int tmp = arg << 23;
    switch (arg) {
      case 1:  return (arg | 1) + tmp;
      case 2:  return (arg | 2) + tmp;
      case 3:  return (arg | 3) + tmp;
      case 4:  return (arg | 4) + tmp;
      case (1 << 20):  return (arg | 5) + tmp;
      default: return 0;
    }
  }

  /**
   * Logical instructions cannot take 'extend' operations into the shift
   * operand, so test that only the shifts are merged.
   */

  /// CHECK-START-ARM: void Main.$opt$noinline$testAnd(long, long) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$noinline$testAnd(long, long) disassembly (after)
  /// CHECK:                            and lsl
  /// CHECK:                            sbfx
  /// CHECK:                            asr{{s?}}
  /// CHECK:                            and{{s?}}

  /// CHECK-START-ARM64: void Main.$opt$noinline$testAnd(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testAnd(long, long) disassembly (after)
  /// CHECK:                            and lsl
  /// CHECK:                            sxtb
  /// CHECK:                            and

  static void $opt$noinline$testAnd(long a, long b) {
    if (doThrow) throw new Error();
    assertLongEquals((a & $noinline$LongShl(b, 5)) | (a & $noinline$longToByte(b)),
                     (a & (b << 5)) | (a & (byte)b));
  }

  /// CHECK-START-ARM: void Main.$opt$noinline$testOr(int, int) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$noinline$testOr(int, int) disassembly (after)
  /// CHECK:                            orr asr
  /// CHECK:                            ubfx
  /// CHECK:                            orr{{s?}}

  /// CHECK-START-ARM64: void Main.$opt$noinline$testOr(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testOr(int, int) disassembly (after)
  /// CHECK:                            orr asr
  /// CHECK:                            uxth
  /// CHECK:                            orr

  static void $opt$noinline$testOr(int a, int b) {
    if (doThrow) throw new Error();
    assertIntEquals((a | $noinline$IntShr(b, 6)) | (a | $noinline$intToChar(b)),
                    (a | (b >> 6)) | (a | (char)b));
  }

  /// CHECK-START-ARM: void Main.$opt$noinline$testXor(long, long) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$noinline$testXor(long, long) disassembly (after)
  /// CHECK:                            eor lsr
  /// CHECK:                            asr{{s?}}
  /// CHECK:                            eor{{s?}}

  /// CHECK-START-ARM64: void Main.$opt$noinline$testXor(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testXor(long, long) disassembly (after)
  /// CHECK:                            eor lsr
  /// CHECK:                            sxtw
  /// CHECK:                            eor

  static void $opt$noinline$testXor(long a, long b) {
    if (doThrow) throw new Error();
    assertLongEquals((a ^ $noinline$LongUshr(b, 7)) | (a ^ $noinline$longToInt(b)),
                     (a ^ (b >>> 7)) | (a ^ (int)b));
  }

  /// CHECK-START-ARM: void Main.$opt$noinline$testNeg(int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                            DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testNeg(int) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$noinline$testNeg(int) disassembly (after)
  /// CHECK:                            neg lsl
  /// CHECK:                            sxth
  /// CHECK:                            neg

  static void $opt$noinline$testNeg(int a) {
    if (doThrow) throw new Error();
    assertIntEquals(-$noinline$IntShl(a, 8) | -$noinline$intToShort(a),
                    (-(a << 8)) | (-(short)a));
  }

  /**
   * The functions below are used to compare the result of optimized operations
   * to non-optimized operations.
   * On the left-hand side we use a non-inlined function call to ensure the
   * optimization does not occur. The checker tests ensure that the optimization
   * does occur on the right-hand.
   */

  /// CHECK-START-ARM: void Main.$opt$validateExtendByteInt1(int, byte) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendByteInt1(int, byte) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendByteInt1(int, byte) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendByteInt1(int a, byte b) {
    assertIntEquals(a + $noinline$byteToChar (b), a +  (char)b);
    // Conversions byte->short and short->int are implicit; nothing to merge.
    assertIntEquals(a + $noinline$byteToShort(b), a + (short)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendByteInt2(int, byte) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendByteInt2(int, byte) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  public static void $opt$validateExtendByteInt2(int a, byte b) {
    // The conversion to `int` has been optimized away, so there is nothing to merge.
    assertIntEquals (a + $noinline$byteToInt (b), a +  (int)b);
    // There is an environment use for `(long)b`, preventing the merge.
    assertLongEquals(a + $noinline$byteToLong(b), a + (long)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendByteLong(long, byte) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$validateExtendByteLong(long, byte) instruction_simplifier_arm (after)
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  /// CHECK-START-ARM64: void Main.$opt$validateExtendByteLong(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendByteLong(long, byte) instruction_simplifier_arm64 (after)
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendByteLong(long a, byte b) {
    // In each of the following tests, there will be a merge on the LHS.

    // The first test has an explicit byte->char conversion on RHS,
    // followed by a conversion that is merged with the Add.
    assertLongEquals(a + $noinline$byteToChar (b), a +  (char)b);
    // Since conversions byte->short and byte->int are implicit, the RHS
    // for the two tests below is the same and one is eliminated by GVN.
    // The other is then merged to a shifter operand instruction.
    assertLongEquals(a + $noinline$byteToShort(b), a + (short)b);
    assertLongEquals(a + $noinline$byteToInt  (b), a +  (int)b);
  }

  public static void $opt$validateExtendByte(long a, byte b) {
    $opt$validateExtendByteInt1((int)a, b);
    $opt$validateExtendByteInt2((int)a, b);
    $opt$validateExtendByteLong(a, b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendCharInt1(int, char) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendCharInt1(int, char) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendCharInt1(int, char) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendCharInt1(int a, char b) {
    assertIntEquals(a + $noinline$charToByte (b), a +  (byte)b);
    assertIntEquals(a + $noinline$charToShort(b), a + (short)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendCharInt2(int, char) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendCharInt2(int, char) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  public static void $opt$validateExtendCharInt2(int a, char b) {
    // The conversion to `int` has been optimized away, so there is nothing to merge.
    assertIntEquals (a + $noinline$charToInt (b), a +  (int)b);
    // There is an environment use for `(long)b` and the implicit `(long)a`, preventing the merge.
    assertLongEquals(a + $noinline$charToLong(b), a + (long)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendCharLong(long, char) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$validateExtendCharLong(long, char) instruction_simplifier_arm (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  /// CHECK-START-ARM64: void Main.$opt$validateExtendCharLong(long, char) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendCharLong(long, char) instruction_simplifier_arm64 (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendCharLong(long a, char b) {
    // The first two tests have a type conversion.
    assertLongEquals(a + $noinline$charToByte (b), a +  (byte)b);
    assertLongEquals(a + $noinline$charToShort(b), a + (short)b);
    // On ARM64 this test does not because the conversion to `int` is optimized away.
    assertLongEquals(a + $noinline$charToInt  (b), a +   (int)b);
  }

  public static void $opt$validateExtendChar(long a, char b) {
    $opt$validateExtendCharInt1((int)a, b);
    $opt$validateExtendCharInt2((int)a, b);
    $opt$validateExtendCharLong(a, b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendShortInt1(int, short) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendShortInt1(int, short) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendShortInt1(int, short) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendShortInt1(int a, short b) {
    assertIntEquals(a + $noinline$shortToByte (b), a + (byte)b);
    assertIntEquals(a + $noinline$shortToChar (b), a + (char)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendShortInt2(int, short) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendShortInt2(int, short) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        DataProcWithShifterOp

  public static void $opt$validateExtendShortInt2(int a, short b) {
    // The conversion to `int` has been optimized away, so there is nothing to merge.
    assertIntEquals (a + $noinline$shortToInt  (b), a +  (int)b);
    // There is an environment use for `(long)b` and the implicit `(long)a`, preventing the merge.
    assertLongEquals(a + $noinline$shortToLong (b), a + (long)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendShortLong(long, short) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$validateExtendShortLong(long, short) instruction_simplifier_arm (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  /// CHECK-START-ARM64: void Main.$opt$validateExtendShortLong(long, short) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendShortLong(long, short) instruction_simplifier_arm64 (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendShortLong(long a, short b) {
    // The first two tests have a type conversion.
    assertLongEquals(a + $noinline$shortToByte(b), a + (byte)b);
    assertLongEquals(a + $noinline$shortToChar(b), a + (char)b);
    // On ARM64 this test does not because the conversion to `int` is optimized away.
    assertLongEquals(a + $noinline$shortToInt (b), a +  (int)b);
  }

  public static void $opt$validateExtendShort(long a, short b) {
    $opt$validateExtendShortInt1((int)a, b);
    $opt$validateExtendShortInt2((int)a, b);
    $opt$validateExtendShortLong(a, b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendInt(long, int) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$validateExtendInt(long, int) instruction_simplifier_arm (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  /// CHECK-START-ARM64: void Main.$opt$validateExtendInt(long, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendInt(long, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendInt(long a, int b) {
    // All tests have a conversion to `long`. The first three tests also have a
    // conversion from `int` to the specified type. For each test the conversion
    // to `long` is merged into the shifter operand.
    assertLongEquals(a + $noinline$intToByte (b), a +  (byte)b);
    assertLongEquals(a + $noinline$intToChar (b), a +  (char)b);
    assertLongEquals(a + $noinline$intToShort(b), a + (short)b);
    assertLongEquals(a + $noinline$intToLong (b), a +  (long)b);
  }

  /// CHECK-START-ARM: void Main.$opt$validateExtendLong(long, long) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$validateExtendLong(long, long) instruction_simplifier_arm (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  /// CHECK-START-ARM64: void Main.$opt$validateExtendLong(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateExtendLong(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK:                            TypeConversion
  /// CHECK-NOT:                        TypeConversion

  public static void $opt$validateExtendLong(long a, long b) {
    // Each test has two conversions, from `long` and then back to `long`. The
    // conversions to `long` are merged.
    assertLongEquals(a + $noinline$longToByte (b), a +  (byte)b);
    assertLongEquals(a + $noinline$longToChar (b), a +  (char)b);
    assertLongEquals(a + $noinline$longToShort(b), a + (short)b);
    assertLongEquals(a + $noinline$longToInt  (b), a +   (int)b);
  }


  static int $noinline$IntShl(int b, int c) {
    if (doThrow) throw new Error();
    return b << c;
  }
  static int $noinline$IntShr(int b, int c) {
    if (doThrow) throw new Error();
    return b >> c;
  }
  static int $noinline$IntUshr(int b, int c) {
    if (doThrow) throw new Error();
    return b >>> c;
  }


  // Each test line below should see one merge.
  //
  /// CHECK-START: void Main.$opt$validateShiftInt(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK-NOT:                        Shl
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK-NOT:                        Shl
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK-NOT:                        UShr
  //
  // Note: simplification after inlining removes `b << 32`, `b >> 32` and `b >>> 32`.
  //
  /// CHECK-START: void Main.$opt$validateShiftInt(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK-NOT:                        Shl
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK-NOT:                        Shl
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK-NOT:                        UShr
  //
  // Note: simplification followed by GVN exposes the common subexpressions between shifts with larger distance
  //       `b << 62`, `b << 63` etc. and the equivalent smaller distances.
  //
  /// CHECK-START: void Main.$opt$validateShiftInt(int, int) GVN (after)
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK:                            Shl
  /// CHECK-NOT:                        Shl
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK:                            Shr
  /// CHECK-NOT:                        Shl
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK:                            UShr
  /// CHECK-NOT:                        UShr
  //
  /// CHECK-START-ARM: void Main.$opt$validateShiftInt(int, int) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM: void Main.$opt$validateShiftInt(int, int) instruction_simplifier_arm (after)
  /// CHECK-NOT:                        Shl
  /// CHECK-NOT:                        Shr
  /// CHECK-NOT:                        UShr

  /// CHECK-START-ARM64: void Main.$opt$validateShiftInt(int, int) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateShiftInt(int, int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Shl
  /// CHECK-NOT:                        Shr
  /// CHECK-NOT:                        UShr

  public static void $opt$validateShiftInt(int a, int b) {
    assertIntEquals(a + $noinline$IntShl(b, 1),   a + (b <<  1));
    assertIntEquals(a + $noinline$IntShl(b, 6),   a + (b <<  6));
    assertIntEquals(a + $noinline$IntShl(b, 7),   a + (b <<  7));
    assertIntEquals(a + $noinline$IntShl(b, 8),   a + (b <<  8));
    assertIntEquals(a + $noinline$IntShl(b, 14),  a + (b << 14));
    assertIntEquals(a + $noinline$IntShl(b, 15),  a + (b << 15));
    assertIntEquals(a + $noinline$IntShl(b, 16),  a + (b << 16));
    assertIntEquals(a + $noinline$IntShl(b, 30),  a + (b << 30));
    assertIntEquals(a + $noinline$IntShl(b, 31),  a + (b << 31));
    assertIntEquals(a + $noinline$IntShl(b, 32),  a + (b << $opt$inline$IntConstant32()));
    assertIntEquals(a + $noinline$IntShl(b, 62),  a + (b << $opt$inline$IntConstant62()));
    assertIntEquals(a + $noinline$IntShl(b, 63),  a + (b << $opt$inline$IntConstant63()));

    assertIntEquals(a - $noinline$IntShr(b, 1),   a - (b >>  1));
    assertIntEquals(a - $noinline$IntShr(b, 6),   a - (b >>  6));
    assertIntEquals(a - $noinline$IntShr(b, 7),   a - (b >>  7));
    assertIntEquals(a - $noinline$IntShr(b, 8),   a - (b >>  8));
    assertIntEquals(a - $noinline$IntShr(b, 14),  a - (b >> 14));
    assertIntEquals(a - $noinline$IntShr(b, 15),  a - (b >> 15));
    assertIntEquals(a - $noinline$IntShr(b, 16),  a - (b >> 16));
    assertIntEquals(a - $noinline$IntShr(b, 30),  a - (b >> 30));
    assertIntEquals(a - $noinline$IntShr(b, 31),  a - (b >> 31));
    assertIntEquals(a - $noinline$IntShr(b, 32),  a - (b >> $opt$inline$IntConstant32()));
    assertIntEquals(a - $noinline$IntShr(b, 62),  a - (b >> $opt$inline$IntConstant62()));
    assertIntEquals(a - $noinline$IntShr(b, 63),  a - (b >> $opt$inline$IntConstant63()));

    assertIntEquals(a ^ $noinline$IntUshr(b, 1),   a ^ (b >>>  1));
    assertIntEquals(a ^ $noinline$IntUshr(b, 6),   a ^ (b >>>  6));
    assertIntEquals(a ^ $noinline$IntUshr(b, 7),   a ^ (b >>>  7));
    assertIntEquals(a ^ $noinline$IntUshr(b, 8),   a ^ (b >>>  8));
    assertIntEquals(a ^ $noinline$IntUshr(b, 14),  a ^ (b >>> 14));
    assertIntEquals(a ^ $noinline$IntUshr(b, 15),  a ^ (b >>> 15));
    assertIntEquals(a ^ $noinline$IntUshr(b, 16),  a ^ (b >>> 16));
    assertIntEquals(a ^ $noinline$IntUshr(b, 30),  a ^ (b >>> 30));
    assertIntEquals(a ^ $noinline$IntUshr(b, 31),  a ^ (b >>> 31));
    assertIntEquals(a ^ $noinline$IntUshr(b, 32),  a ^ (b >>> $opt$inline$IntConstant32()));
    assertIntEquals(a ^ $noinline$IntUshr(b, 62),  a ^ (b >>> $opt$inline$IntConstant62()));
    assertIntEquals(a ^ $noinline$IntUshr(b, 63),  a ^ (b >>> $opt$inline$IntConstant63()));
  }

  // Hiding constants outside the range [0, 32) used for int shifts from Jack.
  // (Jack extracts only the low 5 bits.)
  public static int $opt$inline$IntConstant32() { return 32; }
  public static int $opt$inline$IntConstant62() { return 62; }
  public static int $opt$inline$IntConstant63() { return 63; }


  static long $noinline$LongShl(long b, long c) {
    if (doThrow) throw new Error();
    return b << c;
  }
  static long $noinline$LongShr(long b, long c) {
    if (doThrow) throw new Error();
    return b >> c;
  }
  static long $noinline$LongUshr(long b, long c) {
    if (doThrow) throw new Error();
    return b >>> c;
  }

  // Each test line below should see one merge.
  /// CHECK-START-ARM: void Main.$opt$validateShiftLong(long, long) instruction_simplifier_arm (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  // On ARM shifts by 1 are not merged.
  /// CHECK-START-ARM: void Main.$opt$validateShiftLong(long, long) instruction_simplifier_arm (after)
  /// CHECK:                            Shl
  /// CHECK-NOT:                        Shl
  /// CHECK:                            Shr
  /// CHECK-NOT:                        Shr
  /// CHECK:                            UShr
  /// CHECK-NOT:                        UShr

  /// CHECK-START-ARM64: void Main.$opt$validateShiftLong(long, long) instruction_simplifier_arm64 (after)
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK:                            DataProcWithShifterOp
  /// CHECK-NOT:                        DataProcWithShifterOp

  /// CHECK-START-ARM64: void Main.$opt$validateShiftLong(long, long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:                        Shl
  /// CHECK-NOT:                        Shr
  /// CHECK-NOT:                        UShr

  public static void $opt$validateShiftLong(long a, long b) {
    assertLongEquals(a + $noinline$LongShl(b, 1),   a + (b <<  1));
    assertLongEquals(a + $noinline$LongShl(b, 6),   a + (b <<  6));
    assertLongEquals(a + $noinline$LongShl(b, 7),   a + (b <<  7));
    assertLongEquals(a + $noinline$LongShl(b, 8),   a + (b <<  8));
    assertLongEquals(a + $noinline$LongShl(b, 14),  a + (b << 14));
    assertLongEquals(a + $noinline$LongShl(b, 15),  a + (b << 15));
    assertLongEquals(a + $noinline$LongShl(b, 16),  a + (b << 16));
    assertLongEquals(a + $noinline$LongShl(b, 30),  a + (b << 30));
    assertLongEquals(a + $noinline$LongShl(b, 31),  a + (b << 31));
    assertLongEquals(a + $noinline$LongShl(b, 32),  a + (b << 32));
    assertLongEquals(a + $noinline$LongShl(b, 62),  a + (b << 62));
    assertLongEquals(a + $noinline$LongShl(b, 63),  a + (b << 63));

    assertLongEquals(a - $noinline$LongShr(b, 1),   a - (b >>  1));
    assertLongEquals(a - $noinline$LongShr(b, 6),   a - (b >>  6));
    assertLongEquals(a - $noinline$LongShr(b, 7),   a - (b >>  7));
    assertLongEquals(a - $noinline$LongShr(b, 8),   a - (b >>  8));
    assertLongEquals(a - $noinline$LongShr(b, 14),  a - (b >> 14));
    assertLongEquals(a - $noinline$LongShr(b, 15),  a - (b >> 15));
    assertLongEquals(a - $noinline$LongShr(b, 16),  a - (b >> 16));
    assertLongEquals(a - $noinline$LongShr(b, 30),  a - (b >> 30));
    assertLongEquals(a - $noinline$LongShr(b, 31),  a - (b >> 31));
    assertLongEquals(a - $noinline$LongShr(b, 32),  a - (b >> 32));
    assertLongEquals(a - $noinline$LongShr(b, 62),  a - (b >> 62));
    assertLongEquals(a - $noinline$LongShr(b, 63),  a - (b >> 63));

    assertLongEquals(a ^ $noinline$LongUshr(b, 1),   a ^ (b >>>  1));
    assertLongEquals(a ^ $noinline$LongUshr(b, 6),   a ^ (b >>>  6));
    assertLongEquals(a ^ $noinline$LongUshr(b, 7),   a ^ (b >>>  7));
    assertLongEquals(a ^ $noinline$LongUshr(b, 8),   a ^ (b >>>  8));
    assertLongEquals(a ^ $noinline$LongUshr(b, 14),  a ^ (b >>> 14));
    assertLongEquals(a ^ $noinline$LongUshr(b, 15),  a ^ (b >>> 15));
    assertLongEquals(a ^ $noinline$LongUshr(b, 16),  a ^ (b >>> 16));
    assertLongEquals(a ^ $noinline$LongUshr(b, 30),  a ^ (b >>> 30));
    assertLongEquals(a ^ $noinline$LongUshr(b, 31),  a ^ (b >>> 31));
    assertLongEquals(a ^ $noinline$LongUshr(b, 32),  a ^ (b >>> 32));
    assertLongEquals(a ^ $noinline$LongUshr(b, 62),  a ^ (b >>> 62));
    assertLongEquals(a ^ $noinline$LongUshr(b, 63),  a ^ (b >>> 63));
  }


  public static void main(String[] args) {
    assertLongEquals(10000L - 3L, $opt$noinline$translate(10000L, (byte)3));
    assertLongEquals(-10000L - -3L, $opt$noinline$translate(-10000L, (byte)-3));

    assertIntEquals(4096, $opt$noinline$sameInput(512));
    assertIntEquals(-8192, $opt$noinline$sameInput(-1024));

    assertIntEquals(((1 << 23) | 1), $opt$noinline$multipleUses(1));
    assertIntEquals(((1 << 20) | 5), $opt$noinline$multipleUses(1 << 20));

    long inputs[] = {
      -((1L <<  7) - 1L), -((1L <<  7)), -((1L <<  7) + 1L),
      -((1L << 15) - 1L), -((1L << 15)), -((1L << 15) + 1L),
      -((1L << 16) - 1L), -((1L << 16)), -((1L << 16) + 1L),
      -((1L << 31) - 1L), -((1L << 31)), -((1L << 31) + 1L),
      -((1L << 32) - 1L), -((1L << 32)), -((1L << 32) + 1L),
      -((1L << 63) - 1L), -((1L << 63)), -((1L << 63) + 1L),
      -42L, -314L, -2718281828L, -0x123456789L, -0x987654321L,
      -1L, -20L, -300L, -4000L, -50000L, -600000L, -7000000L, -80000000L,
      0L,
      1L, 20L, 300L, 4000L, 50000L, 600000L, 7000000L, 80000000L,
      42L,  314L,  2718281828L,  0x123456789L,  0x987654321L,
      (1L <<  7) - 1L, (1L <<  7), (1L <<  7) + 1L,
      (1L <<  8) - 1L, (1L <<  8), (1L <<  8) + 1L,
      (1L << 15) - 1L, (1L << 15), (1L << 15) + 1L,
      (1L << 16) - 1L, (1L << 16), (1L << 16) + 1L,
      (1L << 31) - 1L, (1L << 31), (1L << 31) + 1L,
      (1L << 32) - 1L, (1L << 32), (1L << 32) + 1L,
      (1L << 63) - 1L, (1L << 63), (1L << 63) + 1L,
      Long.MIN_VALUE, Long.MAX_VALUE
    };
    for (int i = 0; i < inputs.length; i++) {
      $opt$noinline$testNeg((int)inputs[i]);
      for (int j = 0; j < inputs.length; j++) {
        $opt$noinline$testAnd(inputs[i], inputs[j]);
        $opt$noinline$testOr((int)inputs[i], (int)inputs[j]);
        $opt$noinline$testXor(inputs[i], inputs[j]);

        $opt$validateExtendByte(inputs[i], (byte)inputs[j]);
        $opt$validateExtendChar(inputs[i], (char)inputs[j]);
        $opt$validateExtendShort(inputs[i], (short)inputs[j]);
        $opt$validateExtendInt(inputs[i], (int)inputs[j]);
        $opt$validateExtendLong(inputs[i], inputs[j]);

        $opt$validateShiftInt((int)inputs[i], (int)inputs[j]);
        $opt$validateShiftLong(inputs[i], inputs[j]);
      }
    }

  }
}
