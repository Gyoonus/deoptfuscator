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

  /// CHECK-START-ARM: int Main.and254(int) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #254
  /// CHECK:                and {{r\d+}}, {{r\d+}}, #0xfe

  public static int and254(int arg) {
    return arg & 254;
  }

  /// CHECK-START-ARM: int Main.and255(int) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #255
  /// CHECK:                ubfx {{r\d+}}, {{r\d+}}, #0, #8

  public static int and255(int arg) {
    return arg & 255;
  }

  /// CHECK-START-ARM: int Main.and511(int) disassembly (after)
  /// CHECK:                ubfx {{r\d+}}, {{r\d+}}, #0, #9

  public static int and511(int arg) {
    return arg & 511;
  }

  /// CHECK-START-ARM: int Main.andF00D(int) disassembly (after)
  /// CHECK:                mov {{r\d+}}, #61453
  /// CHECK:                and{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static int andF00D(int arg) {
    return arg & 0xF00D;
  }

  /// CHECK-START-ARM: int Main.andNot15(int) disassembly (after)
  /// CHECK-NOT:            mvn {{r\d+}}, #15
  /// CHECK:                bic {{r\d+}}, {{r\d+}}, #0xf

  public static int andNot15(int arg) {
    return arg & ~15;
  }

  /// CHECK-START-ARM: int Main.or255(int) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #255
  /// CHECK:                orr {{r\d+}}, {{r\d+}}, #0xff

  public static int or255(int arg) {
    return arg | 255;
  }

  /// CHECK-START-ARM: int Main.or511(int) disassembly (after)
  /// CHECK:                mov {{r\d+}}, #511
  /// CHECK:                orr{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static int or511(int arg) {
    return arg | 511;
  }

  /// CHECK-START-ARM: int Main.orNot15(int) disassembly (after)
  /// CHECK-NOT:            mvn {{r\d+}}, #15
  /// CHECK:                orn {{r\d+}}, {{r\d+}}, #0xf

  public static int orNot15(int arg) {
    return arg | ~15;
  }

  /// CHECK-START-ARM: int Main.xor255(int) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #255
  /// CHECK:                eor {{r\d+}}, {{r\d+}}, #0xff

  public static int xor255(int arg) {
    return arg ^ 255;
  }

  /// CHECK-START-ARM: int Main.xor511(int) disassembly (after)
  /// CHECK:                mov {{r\d+}}, #511
  /// CHECK:                eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static int xor511(int arg) {
    return arg ^ 511;
  }

  /// CHECK-START-ARM: int Main.xorNot15(int) disassembly (after)
  /// CHECK:                mvn {{r\d+}}, #15
  /// CHECK:                eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static int xorNot15(int arg) {
    return arg ^ ~15;
  }

  /// CHECK-START-ARM: long Main.and255(long) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #255
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}
  /// CHECK-DAG:            and {{r\d+}}, {{r\d+}}, #0xff
  /// CHECK-DAG:            mov{{s?}} {{r\d+}}, #0
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}

  public static long and255(long arg) {
    return arg & 255L;
  }

  /// CHECK-START-ARM: long Main.and511(long) disassembly (after)
  /// CHECK:                ubfx {{r\d+}}, {{r\d+}}, #0, #9
  /// CHECK-NEXT:           mov{{s?}} {{r\d+}}, #0
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}

  public static long and511(long arg) {
    return arg & 511L;
  }

  /// CHECK-START-ARM: long Main.andF00D(long) disassembly (after)
  /// CHECK:                mov {{r\d+}}, #61453
  /// CHECK-NEXT:           mov{{s?}} {{r\d+}}, #0
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}
  /// CHECK-NOT:            ubfx
  /// CHECK:                and{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NEXT:           and{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}
  /// CHECK-NOT:            ubfx

  public static long andF00D(long arg) {
    return arg & 0xF00DL;
  }

  /// CHECK-START-ARM: long Main.andNot15(long) disassembly (after)
  /// CHECK-NOT:            mvn {{r\d+}}, #15
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}
  /// CHECK:                bic {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}

  public static long andNot15(long arg) {
    return arg & ~15L;
  }

  /// CHECK-START-ARM: long Main.and0xfffffff00000000f(long) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #15
  /// CHECK-NOT:            mvn {{r\d+}}, #15
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}
  /// CHECK-DAG:            and {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-DAG:            bic {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-NOT:            and{{(\.w)?}}
  /// CHECK-NOT:            bic{{(\.w)?}}

  public static long and0xfffffff00000000f(long arg) {
    return arg & 0xfffffff00000000fL;
  }

  /// CHECK-START-ARM: long Main.or255(long) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #255
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn
  /// CHECK:                orr {{r\d+}}, {{r\d+}}, #0xff
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn

  public static long or255(long arg) {
    return arg | 255L;
  }

  /// CHECK-START-ARM: long Main.or511(long) disassembly (after)
  /// CHECK:                mov {{r\d+}}, #511
  /// CHECK-NEXT:           mov{{s?}} {{r\d+}}, #0
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn
  /// CHECK:                orr{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NEXT:           orr{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn

  public static long or511(long arg) {
    return arg | 511L;
  }

  /// CHECK-START-ARM: long Main.orNot15(long) disassembly (after)
  /// CHECK-NOT:            mvn {{r\d+}}, #15
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn
  /// CHECK-DAG:            orn {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-DAG:            mvn {{r\d+}}, #0
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn

  public static long orNot15(long arg) {
    return arg | ~15L;
  }

  /// CHECK-START-ARM: long Main.or0xfffffff00000000f(long) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #15
  /// CHECK-NOT:            mvn {{r\d+}}, #15
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn
  /// CHECK-DAG:            orr {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-DAG:            orn {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-NOT:            orr{{(\.w)?}}
  /// CHECK-NOT:            orn

  public static long or0xfffffff00000000f(long arg) {
    return arg | 0xfffffff00000000fL;
  }

  /// CHECK-START-ARM: long Main.xor255(long) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #255
  /// CHECK-NOT:            eor{{(\.w)?}}
  /// CHECK:                eor {{r\d+}}, {{r\d+}}, #0xff
  /// CHECK-NOT:            eor{{(\.w)?}}

  public static long xor255(long arg) {
    return arg ^ 255L;
  }

  /// CHECK-START-ARM: long Main.xor511(long) disassembly (after)
  /// CHECK:                mov {{r\d+}}, #511
  /// CHECK-NEXT:           mov{{s?}} {{r\d+}}, #0
  /// CHECK-NOT:            eor{{(\.w)?}}
  /// CHECK:                eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NEXT:           eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:            eor{{(\.w)?}}

  public static long xor511(long arg) {
    return arg ^ 511L;
  }

  /// CHECK-START-ARM: long Main.xorNot15(long) disassembly (after)
  /// CHECK-DAG:            mvn {{r\d+}}, #15
  /// CHECK-DAG:            mov {{r\d+}}, #4294967295
  /// CHECK-NOT:            eor{{(\.w)?}}
  /// CHECK-DAG:            eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-DAG:            eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:            eor{{(\.w)?}}

  public static long xorNot15(long arg) {
    return arg ^ ~15L;
  }

  // Note: No support for partial long constant embedding.
  /// CHECK-START-ARM: long Main.xor0xfffffff00000000f(long) disassembly (after)
  /// CHECK-DAG:            mov{{s?}} {{r\d+}}, #15
  /// CHECK-DAG:            mvn {{r\d+}}, #15
  /// CHECK-NOT:            eor{{(\.w)?}}
  /// CHECK-DAG:            eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-DAG:            eor{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:            eor{{(\.w)?}}

  public static long xor0xfffffff00000000f(long arg) {
    return arg ^ 0xfffffff00000000fL;
  }

  /// CHECK-START-ARM: long Main.xor0xf00000000000000f(long) disassembly (after)
  /// CHECK-NOT:            movs {{r\d+}}, #15
  /// CHECK-NOT:            mov.w {{r\d+}}, #-268435456
  /// CHECK-NOT:            eor{{(\.w)?}}
  /// CHECK-DAG:            eor {{r\d+}}, {{r\d+}}, #0xf
  /// CHECK-DAG:            eor {{r\d+}}, {{r\d+}}, #0xf0000000
  /// CHECK-NOT:            eor{{(\.w)?}}

  public static long xor0xf00000000000000f(long arg) {
    return arg ^ 0xf00000000000000fL;
  }

  /// CHECK-START-ARM: long Main.shl1(long) disassembly (after)
  /// CHECK:                lsls{{(\.w)?}} {{r\d+}}, {{r\d+}}, #1
  /// CHECK:                adc{{(\.w)?}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  /// CHECK-START-ARM: long Main.shl1(long) disassembly (after)
  /// CHECK-NOT:            lsl{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  /// CHECK-START-X86: long Main.shl1(long) disassembly (after)
  /// CHECK:                add
  /// CHECK:                adc

  /// CHECK-START-X86: long Main.shl1(long) disassembly (after)
  /// CHECK-NOT:            shl

  public static long shl1(long arg) {
    return arg << 1;
  }

  /// CHECK-START-ARM: long Main.shl2(long) disassembly (after)
  /// CHECK:                lsl{{s?|\.w}} <<oh:r\d+>>, {{r\d+}}, #2
  /// CHECK:                orr <<oh>>, <<low:r\d+>>, lsr #30
  /// CHECK:                lsl{{s?|\.w}} {{r\d+}}, <<low>>, #2

  /// CHECK-START-ARM: long Main.shl2(long) disassembly (after)
  /// CHECK-NOT:            lsl{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shl2(long arg) {
    return arg << 2;
  }

  /// CHECK-START-ARM: long Main.shl31(long) disassembly (after)
  /// CHECK:                lsl{{s?|\.w}} <<oh:r\d+>>, {{r\d+}}, #31
  /// CHECK:                orr <<oh>>, <<low:r\d+>>, lsr #1
  /// CHECK:                lsl{{s?|\.w}} {{r\d+}}, <<low>>, #31

  /// CHECK-START-ARM: long Main.shl31(long) disassembly (after)
  /// CHECK-NOT:            lsl{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shl31(long arg) {
    return arg << 31;
  }

  /// CHECK-START-ARM: long Main.shl32(long) disassembly (after)
  /// CHECK-DAG:            mov{{s?}} {{r\d+}}, {{r\d+}}
  /// CHECK-DAG:            mov{{s?|\.w}} {{r\d+}}, #0

  /// CHECK-START-ARM: long Main.shl32(long) disassembly (after)
  /// CHECK-NOT:            lsl{{s?|\.w}}

  public static long shl32(long arg) {
    return arg << 32;
  }

  /// CHECK-START-ARM: long Main.shl33(long) disassembly (after)
  /// CHECK-DAG:            lsl{{s?|\.w}} {{r\d+}}, <<high:r\d+>>, #1
  /// CHECK-DAG:            mov{{s?|\.w}} {{r\d+}}, #0

  /// CHECK-START-ARM: long Main.shl33(long) disassembly (after)
  /// CHECK-NOT:            lsl{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shl33(long arg) {
    return arg << 33;
  }

  /// CHECK-START-ARM: long Main.shl63(long) disassembly (after)
  /// CHECK-DAG:            lsl{{s?|\.w}} {{r\d+}}, <<high:r\d+>>, #31
  /// CHECK-DAG:            mov{{s?|\.w}} {{r\d+}}, #0

  /// CHECK-START-ARM: long Main.shl63(long) disassembly (after)
  /// CHECK-NOT:            lsl{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shl63(long arg) {
    return arg << 63;
  }

  /// CHECK-START-ARM: long Main.shr1(long) disassembly (after)
  /// CHECK:                asrs{{(\.w)?}} {{r\d+}}, {{r\d+}}, #1
  /// CHECK:                rrx {{r\d+}}, {{r\d+}}

  /// CHECK-START-ARM: long Main.shr1(long) disassembly (after)
  /// CHECK-NOT:            asr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shr1(long arg) {
    return arg >> 1;
  }

  /// CHECK-START-ARM: long Main.shr2(long) disassembly (after)
  /// CHECK:                lsr{{s?|\.w}} <<ol:r\d+>>, {{r\d+}}, #2
  /// CHECK:                orr <<ol>>, <<high:r\d+>>, lsl #30
  /// CHECK-DAG:            asr{{s?|\.w}} {{r\d+}}, <<high>>, #2

  /// CHECK-START-ARM: long Main.shr2(long) disassembly (after)
  /// CHECK-NOT:            asr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shr2(long arg) {
    return arg >> 2;
  }

  /// CHECK-START-ARM: long Main.shr31(long) disassembly (after)
  /// CHECK:                lsr{{s?|\.w}} <<ol:r\d+>>, {{r\d+}}, #31
  /// CHECK:                orr <<ol>>, <<high:r\d+>>, lsl #1
  /// CHECK:                asr{{s?|\.w}} {{r\d+}}, <<high>>, #31

  /// CHECK-START-ARM: long Main.shr31(long) disassembly (after)
  /// CHECK-NOT:            asr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shr31(long arg) {
    return arg >> 31;
  }

  /// CHECK-START-ARM: long Main.shr32(long) disassembly (after)
  /// CHECK-DAG:            asr{{s?|\.w}} {{r\d+}}, <<high:r\d+>>, #31
  /// CHECK-DAG:            mov{{s?}} {{r\d+}}, <<high>>

  /// CHECK-START-ARM: long Main.shr32(long) disassembly (after)
  /// CHECK-NOT:            asr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}
  /// CHECK-NOT:            lsr{{s?|\.w}}

  public static long shr32(long arg) {
    return arg >> 32;
  }

  /// CHECK-START-ARM: long Main.shr33(long) disassembly (after)
  /// CHECK-DAG:            asr{{s?|\.w}} {{r\d+}}, <<high:r\d+>>, #1
  /// CHECK-DAG:            asr{{s?|\.w}} {{r\d+}}, <<high>>, #31

  /// CHECK-START-ARM: long Main.shr33(long) disassembly (after)
  /// CHECK-NOT:            asr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shr33(long arg) {
    return arg >> 33;
  }

  /// CHECK-START-ARM: long Main.shr63(long) disassembly (after)
  /// CHECK-DAG:            asr{{s?|\.w}} {{r\d+}}, <<high:r\d+>>, #31
  /// CHECK-DAG:            asr{{s?|\.w}} {{r\d+}}, <<high>>, #31

  /// CHECK-START-ARM: long Main.shr63(long) disassembly (after)
  /// CHECK-NOT:            asr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long shr63(long arg) {
    return arg >> 63;
  }

  /// CHECK-START-ARM: long Main.ushr1(long) disassembly (after)
  /// CHECK:                lsrs{{|.w}} {{r\d+}}, {{r\d+}}, #1
  /// CHECK:                rrx {{r\d+}}, {{r\d+}}

  /// CHECK-START-ARM: long Main.ushr1(long) disassembly (after)
  /// CHECK-NOT:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long ushr1(long arg) {
    return arg >>> 1;
  }

  /// CHECK-START-ARM: long Main.ushr2(long) disassembly (after)
  /// CHECK:                lsr{{s?|\.w}} <<ol:r\d+>>, {{r\d+}}, #2
  /// CHECK:                orr <<ol>>, <<high:r\d+>>, lsl #30
  /// CHECK-DAG:            lsr{{s?|\.w}} {{r\d+}}, <<high>>, #2

  /// CHECK-START-ARM: long Main.ushr2(long) disassembly (after)
  /// CHECK-NOT:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long ushr2(long arg) {
    return arg >>> 2;
  }

  /// CHECK-START-ARM: long Main.ushr31(long) disassembly (after)
  /// CHECK:                lsr{{s?|\.w}} <<ol:r\d+>>, {{r\d+}}, #31
  /// CHECK:                orr <<ol>>, <<high:r\d+>>, lsl #1
  /// CHECK:                lsr{{s?|\.w}} {{r\d+}}, <<high>>, #31

  /// CHECK-START-ARM: long Main.ushr31(long) disassembly (after)
  /// CHECK-NOT:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long ushr31(long arg) {
    return arg >>> 31;
  }

  /// CHECK-START-ARM: long Main.ushr32(long) disassembly (after)
  /// CHECK-DAG:            mov{{s?}} {{r\d+}}, {{r\d+}}
  /// CHECK-DAG:            mov{{s?|\.w}} {{r\d+}}, #0

  /// CHECK-START-ARM: long Main.ushr32(long) disassembly (after)
  /// CHECK-NOT:            lsr{{s?|\.w}}

  public static long ushr32(long arg) {
    return arg >>> 32;
  }

  /// CHECK-START-ARM: long Main.ushr33(long) disassembly (after)
  /// CHECK-DAG:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, #1
  /// CHECK-DAG:            mov{{s?|\.w}} {{r\d+}}, #0

  /// CHECK-START-ARM: long Main.ushr33(long) disassembly (after)
  /// CHECK-NOT:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long ushr33(long arg) {
    return arg >>> 33;
  }

  /// CHECK-START-ARM: long Main.ushr63(long) disassembly (after)
  /// CHECK-DAG:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, #31
  /// CHECK-DAG:            mov{{s?|\.w}} {{r\d+}}, #0

  /// CHECK-START-ARM: long Main.ushr63(long) disassembly (after)
  /// CHECK-NOT:            lsr{{s?|\.w}} {{r\d+}}, {{r\d+}}, {{r\d+}}

  public static long ushr63(long arg) {
    return arg >>> 63;
  }

  /**
   * ARM/ARM64: Test that the `-1` constant is not synthesized in a register and that we
   * instead simply switch between `add` and `sub` instructions with the
   * constant embedded.
   * We need two uses (or more) of the constant because the compiler always
   * defers to immediate value handling to VIXL when it has only one use.
   */

  /// CHECK-START-ARM64: long Main.addM1(long) register (after)
  /// CHECK:     <<Arg:j\d+>>       ParameterValue
  /// CHECK:     <<ConstM1:j\d+>>   LongConstant -1
  /// CHECK-NOT:                    ParallelMove
  /// CHECK:                        Add [<<Arg>>,<<ConstM1>>]
  /// CHECK:                        Sub [<<Arg>>,<<ConstM1>>]

  /// CHECK-START-ARM64: long Main.addM1(long) disassembly (after)
  /// CHECK:                        sub x{{\d+}}, x{{\d+}}, #0x1
  /// CHECK:                        add x{{\d+}}, x{{\d+}}, #0x1

  /// CHECK-START-ARM: long Main.addM1(long) register (after)
  /// CHECK:     <<Arg:j\d+>>       ParameterValue
  /// CHECK:     <<ConstM1:j\d+>>   LongConstant -1
  /// CHECK-NOT:                    ParallelMove
  /// CHECK:                        Add [<<Arg>>,<<ConstM1>>]
  /// CHECK:                        Sub [<<Arg>>,<<ConstM1>>]

  /// CHECK-START-ARM: long Main.addM1(long) disassembly (after)
  /// CHECK:     <<Arg:j\d+>>       ParameterValue
  /// CHECK:     <<ConstM1:j\d+>>   LongConstant -1
  /// CHECK:                        Add [<<Arg>>,<<ConstM1>>]
  /// CHECK-NEXT:                   {{adds|subs}} r{{\d+}}, #{{4294967295|1}}
  /// CHECK-NEXT:                   adc r{{\d+}}, r{{\d+}}, #4294967295
  /// CHECK:                        Sub [<<Arg>>,<<ConstM1>>]
  /// CHECK-NEXT:                   adds r{{\d+}}, #1
  /// CHECK-NEXT:                   adc r{{\d+}}, #0

  public static long addM1(long arg) {
    return (arg + (-1)) | (arg - (-1));
  }

  /**
   * ARM: Test that some long constants are not synthesized in a register for add-long.
   * Also test some negative cases where we do synthetize constants in registers.
   */

  /// CHECK-START-ARM: long Main.addLongConstants(long) disassembly (after)
  /// CHECK:     <<Arg:j\d+>>       ParameterValue
  /// CHECK-DAG: <<ConstA:j\d+>>    LongConstant 4486007727657233
  /// CHECK-DAG: <<ConstB:j\d+>>    LongConstant 4486011735248896
  /// CHECK-DAG: <<ConstC:j\d+>>    LongConstant -1071856711330889728
  /// CHECK-DAG: <<ConstD:j\d+>>    LongConstant 17587891077120
  /// CHECK-DAG: <<ConstE:j\d+>>    LongConstant -8808977924096
  /// CHECK-DAG: <<ConstF:j\d+>>    LongConstant 17587891077121
  /// CHECK-DAG: <<ConstG:j\d+>>    LongConstant 4095
  /// CHECK:                        Add [<<Arg>>,<<ConstA>>]
  /// CHECK-NEXT:                   adds r{{\d+}}, r{{\d+}}, #286331153
  /// CHECK-NEXT:                   adc r{{\d+}}, r{{\d+}}, #1044480
  /// CHECK:                        Add [<<Arg>>,<<ConstB>>]
  /// CHECK-NEXT:                   subs r{{\d+}}, r{{\d+}}, #1044480
  /// CHECK-NEXT:                   adc r{{\d+}}, r{{\d+}}, #1044480
  /// CHECK:                        Add [<<Arg>>,<<ConstC>>]
  /// CHECK-NEXT:                   subs r{{\d+}}, r{{\d+}}, #16711680
  /// CHECK-NEXT:                   sbc r{{\d+}}, r{{\d+}}, #249561088
  /// CHECK:                        Add [<<Arg>>,<<ConstD>>]
  // There may or may not be a MOV here.
  /// CHECK:                        add r{{\d+}}, r{{\d+}}, #4095
  /// CHECK:                        Add [<<Arg>>,<<ConstE>>]
  // There may or may not be a MOV here.
  /// CHECK:                        sub r{{\d+}}, r{{\d+}}, #2051
  /// CHECK:                        Add [<<Arg>>,<<ConstF>>]
  /// CHECK-NEXT:                   adds{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:                   adc{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK:                        Add [<<Arg>>,<<ConstG>>]
  /// CHECK-NEXT:                   adds{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:                   adc{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}

  public static long addLongConstants(long arg) {
    return
        // Modified immediates.
        (arg + 0x000ff00011111111L) ^  // 4486007727657233
        // Modified immediates high and -low.
        (arg + 0x000ff000fff01000L) ^  // 4486011735248896
        // Modified immediates ~high and -low.
        (arg + 0xf11fffffff010000L) ^  // -1071856711330889728
        // Low word 0 (no carry), high is imm12.
        (arg + 0x00000fff00000000L) ^  // 17587891077120
        // Low word 0 (no carry), -high is imm12.
        (arg + 0xfffff7fd00000000L) ^  // -8808977924096
        // Cannot embed imm12 in ADC/SBC for high word.
        (arg + 0x00000fff00000001L) ^  // 17587891077121
        // Cannot embed imm12 in ADDS/SUBS for low word (need to set flags).
        (arg + 0x0000000000000fffL) ^  // 4095
        arg;
  }

  /**
   * ARM: Test that some long constants are not synthesized in a register for add-long.
   * Also test some negative cases where we do synthetize constants in registers.
   */

  /// CHECK-START-ARM: long Main.subLongConstants(long) disassembly (after)
  /// CHECK:     <<Arg:j\d+>>       ParameterValue
  /// CHECK-DAG: <<ConstA:j\d+>>    LongConstant 4486007727657233
  /// CHECK-DAG: <<ConstB:j\d+>>    LongConstant 4486011735248896
  /// CHECK-DAG: <<ConstC:j\d+>>    LongConstant -1071856711330889728
  /// CHECK-DAG: <<ConstD:j\d+>>    LongConstant 17587891077120
  /// CHECK-DAG: <<ConstE:j\d+>>    LongConstant -8808977924096
  /// CHECK-DAG: <<ConstF:j\d+>>    LongConstant 17587891077121
  /// CHECK-DAG: <<ConstG:j\d+>>    LongConstant 4095
  /// CHECK:                        Sub [<<Arg>>,<<ConstA>>]
  /// CHECK-NEXT:                   subs r{{\d+}}, r{{\d+}}, #286331153
  /// CHECK-NEXT:                   sbc r{{\d+}}, r{{\d+}}, #1044480
  /// CHECK:                        Sub [<<Arg>>,<<ConstB>>]
  /// CHECK-NEXT:                   adds r{{\d+}}, r{{\d+}}, #1044480
  /// CHECK-NEXT:                   sbc r{{\d+}}, r{{\d+}}, #1044480
  /// CHECK:                        Sub [<<Arg>>,<<ConstC>>]
  /// CHECK-NEXT:                   adds r{{\d+}}, r{{\d+}}, #16711680
  /// CHECK-NEXT:                   adc r{{\d+}}, r{{\d+}}, #249561088
  /// CHECK:                        Sub [<<Arg>>,<<ConstD>>]
  // There may or may not be a MOV here.
  /// CHECK:                        sub r{{\d+}}, r{{\d+}}, #4095
  /// CHECK:                        Sub [<<Arg>>,<<ConstE>>]
  // There may or may not be a MOV here.
  /// CHECK:                        add r{{\d+}}, r{{\d+}}, #2051
  /// CHECK:                        Sub [<<Arg>>,<<ConstF>>]
  /// CHECK-NEXT:                   subs{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:                   sbc{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK:                        Sub [<<Arg>>,<<ConstG>>]
  /// CHECK-NEXT:                   subs{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:                   sbc{{(\.w)?}} r{{\d+}}, r{{\d+}}, r{{\d+}}

  public static long subLongConstants(long arg) {
    return
        // Modified immediates.
        (arg - 0x000ff00011111111L) ^  // 4486007727657233
        // Modified immediates high and -low.
        (arg - 0x000ff000fff01000L) ^  // 4486011735248896
        // Modified immediates ~high and -low.
        (arg - 0xf11fffffff010000L) ^  // -1071856711330889728
        // Low word 0 (no carry), high is imm12.
        (arg - 0x00000fff00000000L) ^  // 17587891077120
        // Low word 0 (no carry), -high is imm12.
        (arg - 0xfffff7fd00000000L) ^  // -8808977924096
        // Cannot embed imm12 in ADC/SBC for high word.
        (arg - 0x00000fff00000001L) ^  // 17587891077121
        // Cannot embed imm12 in ADDS/SUBS for low word (need to set flags).
        (arg - 0x0000000000000fffL) ^  // 4095
        arg;
  }

  public static void main(String[] args) {
    int arg = 0x87654321;
    assertIntEquals(and254(arg), 0x20);
    assertIntEquals(and255(arg), 0x21);
    assertIntEquals(and511(arg), 0x121);
    assertIntEquals(andF00D(arg), 0x4001);
    assertIntEquals(andNot15(arg), 0x87654320);
    assertIntEquals(or255(arg), 0x876543ff);
    assertIntEquals(or511(arg), 0x876543ff);
    assertIntEquals(orNot15(arg), 0xfffffff1);
    assertIntEquals(xor255(arg), 0x876543de);
    assertIntEquals(xor511(arg), 0x876542de);
    assertIntEquals(xorNot15(arg), 0x789abcd1);

    long longArg = 0x1234567887654321L;
    assertLongEquals(and255(longArg), 0x21L);
    assertLongEquals(and511(longArg), 0x121L);
    assertLongEquals(andF00D(longArg), 0x4001L);
    assertLongEquals(andNot15(longArg), 0x1234567887654320L);
    assertLongEquals(and0xfffffff00000000f(longArg), 0x1234567000000001L);
    assertLongEquals(or255(longArg), 0x12345678876543ffL);
    assertLongEquals(or511(longArg), 0x12345678876543ffL);
    assertLongEquals(orNot15(longArg), 0xfffffffffffffff1L);
    assertLongEquals(or0xfffffff00000000f(longArg), 0xfffffff88765432fL);
    assertLongEquals(xor255(longArg), 0x12345678876543deL);
    assertLongEquals(xor511(longArg), 0x12345678876542deL);
    assertLongEquals(xorNot15(longArg), 0xedcba987789abcd1L);
    assertLongEquals(xor0xfffffff00000000f(longArg), 0xedcba9888765432eL);
    assertLongEquals(xor0xf00000000000000f(longArg), 0xe23456788765432eL);

    assertLongEquals(14L, addM1(7));

    assertLongEquals(shl1(longArg), 0x2468acf10eca8642L);
    assertLongEquals(shl2(longArg), 0x48d159e21d950c84L);
    assertLongEquals(shl31(longArg), 0x43b2a19080000000L);
    assertLongEquals(shl32(longArg), 0x8765432100000000L);
    assertLongEquals(shl33(longArg), 0x0eca864200000000L);
    assertLongEquals(shl63(longArg), 0x8000000000000000L);
    assertLongEquals(shl1(~longArg), 0xdb97530ef13579bcL);
    assertLongEquals(shl2(~longArg), 0xb72ea61de26af378L);
    assertLongEquals(shl31(~longArg), 0xbc4d5e6f00000000L);
    assertLongEquals(shl32(~longArg), 0x789abcde00000000L);
    assertLongEquals(shl33(~longArg), 0xf13579bc00000000L);
    assertLongEquals(shl63(~longArg), 0x0000000000000000L);

    assertLongEquals(shr1(longArg), 0x091a2b3c43b2a190L);
    assertLongEquals(shr2(longArg), 0x048d159e21d950c8L);
    assertLongEquals(shr31(longArg), 0x000000002468acf1L);
    assertLongEquals(shr32(longArg), 0x0000000012345678L);
    assertLongEquals(shr33(longArg), 0x00000000091a2b3cL);
    assertLongEquals(shr63(longArg), 0x0000000000000000L);
    assertLongEquals(shr1(~longArg), 0xf6e5d4c3bc4d5e6fL);
    assertLongEquals(shr2(~longArg), 0xfb72ea61de26af37L);
    assertLongEquals(shr31(~longArg), 0xffffffffdb97530eL);
    assertLongEquals(shr32(~longArg), 0xffffffffedcba987L);
    assertLongEquals(shr33(~longArg), 0xfffffffff6e5d4c3L);
    assertLongEquals(shr63(~longArg), 0xffffffffffffffffL);

    assertLongEquals(ushr1(longArg), 0x091a2b3c43b2a190L);
    assertLongEquals(ushr2(longArg), 0x048d159e21d950c8L);
    assertLongEquals(ushr31(longArg), 0x000000002468acf1L);
    assertLongEquals(ushr32(longArg), 0x0000000012345678L);
    assertLongEquals(ushr33(longArg), 0x00000000091a2b3cL);
    assertLongEquals(ushr63(longArg), 0x0000000000000000L);
    assertLongEquals(ushr1(~longArg), 0x76e5d4c3bc4d5e6fL);
    assertLongEquals(ushr2(~longArg), 0x3b72ea61de26af37L);
    assertLongEquals(ushr31(~longArg), 0x00000001db97530eL);
    assertLongEquals(ushr32(~longArg), 0x00000000edcba987L);
    assertLongEquals(ushr33(~longArg), 0x0000000076e5d4c3L);
    assertLongEquals(ushr63(~longArg), 0x0000000000000001L);

    // Test -1, 0, +1 and arbitrary constants just before and after overflow
    // on low word in subexpressions of addLongConstants()/subLongConstants(),
    // so that we check that we carry the overflow correctly to the high word.
    // For example
    //    0x111eeeeeeee+0x000ff00011111111 = 0x000ff111ffffffff (carry=0),
    //    0x111eeeeeeef+0x000ff00011111111 = 0x000ff11200000000 (carry=1).
    assertLongEquals(0xf11ff7fdee1e1111L, addLongConstants(0xffffffffffffffffL));
    assertLongEquals(0xee0080211e00eefL, addLongConstants(0x0L));
    assertLongEquals(0xee0080211e01111L, addLongConstants(0x1L));
    assertLongEquals(0xedff81c12201113L, addLongConstants(0x111eeeeeeeeL));
    assertLongEquals(0xedff81feddfeef1L, addLongConstants(0x111eeeeeeefL));
    assertLongEquals(0xedff83e11c1f111L, addLongConstants(0x222000fefffL));
    assertLongEquals(0xedff83fee3e0eefL, addLongConstants(0x222000ff000L));
    assertLongEquals(0xedff805edfe1111L, addLongConstants(0x33300feffffL));
    assertLongEquals(0xedff80412000eefL, addLongConstants(0x33300ff0000L));
    assertLongEquals(0xee0080211e00eefL, subLongConstants(0xffffffffffffffffL));
    assertLongEquals(0xf11ff7fdee1e1111L, subLongConstants(0x0L));
    assertLongEquals(0xf11ff7fc11e1eef3L, subLongConstants(0x1L));
    assertLongEquals(0xee0080412201113L, subLongConstants(0x44411111111L));
    assertLongEquals(0xee0080412201111L, subLongConstants(0x44411111112L));
    assertLongEquals(0xee0080e11c1f111L, subLongConstants(0x555fff01000L));
    assertLongEquals(0xee0080e11c1eef3L, subLongConstants(0x555fff01001L));
    assertLongEquals(0xee0080dedfe1111L, subLongConstants(0x666ff010000L));
    assertLongEquals(0xee0080dedffeef3L, subLongConstants(0x666ff010001L));
  }
}
