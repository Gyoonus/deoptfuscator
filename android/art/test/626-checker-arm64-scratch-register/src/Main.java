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

  boolean b00;
  boolean b01;
  boolean b02;
  boolean b03;
  boolean b04;
  boolean b05;
  boolean b06;
  boolean b07;
  boolean b08;
  boolean b09;
  boolean b10;
  boolean b11;
  boolean b12;
  boolean b13;
  boolean b14;
  boolean b15;
  boolean b16;
  boolean b17;
  boolean b18;
  boolean b19;
  boolean b20;
  boolean b21;
  boolean b22;
  boolean b23;
  boolean b24;
  boolean b25;
  boolean b26;
  boolean b27;
  boolean b28;
  boolean b29;
  boolean b30;
  boolean b31;
  boolean b32;
  boolean b33;
  boolean b34;
  boolean b35;
  boolean b36;

  boolean conditionA;
  boolean conditionB;
  boolean conditionC;

  /// CHECK-START-ARM64: void Main.test() register (after)
  /// CHECK: begin_block
  /// CHECK:   name "B0"
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:   successors "<<ThenBlock:B\d+>>" "<<ElseBlock:B\d+>>"
  /// CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Main.conditionB
  /// CHECK:                       If [<<CondB>>]
  /// CHECK:  end_block
  /// CHECK: begin_block
  /// CHECK:   name "<<ElseBlock>>"
  /// CHECK:                      ParallelMove moves:[40(sp)->d0,24(sp)->32(sp),28(sp)->36(sp),d0->d3,d3->d4,d2->d5,d4->d6,d5->d7,d6->d18,d7->d19,d18->d20,d19->d21,d20->d22,d21->d23,d22->d10,d23->d11,16(sp)->24(sp),20(sp)->28(sp),d10->d14,d11->d12,d12->d13,d13->d1,d14->d2,32(sp)->16(sp),36(sp)->20(sp)]
  /// CHECK: end_block

  /// CHECK-START-ARM64: void Main.test() disassembly (after)
  /// CHECK: begin_block
  /// CHECK:   name "B0"
  /// CHECK:       <<This:l\d+>>  ParameterValue
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:   successors "<<ThenBlock:B\d+>>" "<<ElseBlock:B\d+>>"
  /// CHECK:       <<CondB:z\d+>>  InstanceFieldGet [<<This>>] field_name:Main.conditionB
  /// CHECK:                       If [<<CondB>>]
  /// CHECK:  end_block
  /// CHECK: begin_block
  /// CHECK:   name "<<ElseBlock>>"
  /// CHECK:                      ParallelMove moves:[invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid,invalid->invalid]
  /// CHECK:                        fmov d31, d2
  /// CHECK:                        ldr s2, [sp, #36]
  /// CHECK:                        ldr w16, [sp, #16]
  /// CHECK:                        str w16, [sp, #36]
  /// CHECK:                        str s14, [sp, #16]
  /// CHECK:                        ldr s14, [sp, #28]
  /// CHECK:                        str s1, [sp, #28]
  /// CHECK:                        ldr s1, [sp, #32]
  /// CHECK:                        str s31, [sp, #32]
  /// CHECK:                        ldr s31, [sp, #20]
  /// CHECK:                        str s31, [sp, #40]
  /// CHECK:                        str s12, [sp, #20]
  /// CHECK:                        fmov d12, d11
  /// CHECK:                        fmov d11, d10
  /// CHECK:                        fmov d10, d23
  /// CHECK:                        fmov d23, d22
  /// CHECK:                        fmov d22, d21
  /// CHECK:                        fmov d21, d20
  /// CHECK:                        fmov d20, d19
  /// CHECK:                        fmov d19, d18
  /// CHECK:                        fmov d18, d7
  /// CHECK:                        fmov d7, d6
  /// CHECK:                        fmov d6, d5
  /// CHECK:                        fmov d5, d4
  /// CHECK:                        fmov d4, d3
  /// CHECK:                        fmov d3, d13
  /// CHECK:                        ldr s13, [sp, #24]
  /// CHECK:                        str s3, [sp, #24]
  /// CHECK:                        ldr s3, pc+{{\d+}} (addr {{0x[0-9a-f]+}}) (100)
  /// CHECK: end_block

  public void test() {
    String r = "";

    // For the purpose of this regression test, the order of
    // definition of these float variable matters.  Likewise with the
    // order of the instructions where these variables are used below.
    // Reordering these lines make make the original (b/32545705)
    // issue vanish.
    float f17 = b17 ? 0.0f : 1.0f;
    float f16 = b16 ? 0.0f : 1.0f;
    float f18 = b18 ? 0.0f : 1.0f;
    float f19 = b19 ? 0.0f : 1.0f;
    float f20 = b20 ? 0.0f : 1.0f;
    float f21 = b21 ? 0.0f : 1.0f;
    float f15 = b15 ? 0.0f : 1.0f;
    float f00 = b00 ? 0.0f : 1.0f;
    float f22 = b22 ? 0.0f : 1.0f;
    float f23 = b23 ? 0.0f : 1.0f;
    float f24 = b24 ? 0.0f : 1.0f;
    float f25 = b25 ? 0.0f : 1.0f;
    float f26 = b26 ? 0.0f : 1.0f;
    float f27 = b27 ? 0.0f : 1.0f;
    float f29 = b29 ? 0.0f : 1.0f;
    float f28 = b28 ? 0.0f : 1.0f;
    float f01 = b01 ? 0.0f : 1.0f;
    float f02 = b02 ? 0.0f : 1.0f;
    float f03 = b03 ? 0.0f : 1.0f;
    float f04 = b04 ? 0.0f : 1.0f;
    float f05 = b05 ? 0.0f : 1.0f;
    float f07 = b07 ? 0.0f : 1.0f;
    float f06 = b06 ? 0.0f : 1.0f;
    float f30 = b30 ? 0.0f : 1.0f;
    float f31 = b31 ? 0.0f : 1.0f;
    float f32 = b32 ? 0.0f : 1.0f;
    float f33 = b33 ? 0.0f : 1.0f;
    float f34 = b34 ? 0.0f : 1.0f;
    float f36 = b36 ? 0.0f : 1.0f;
    float f35 = b35 ? 0.0f : 1.0f;
    float f08 = b08 ? 0.0f : 1.0f;
    float f09 = b09 ? 0.0f : 1.0f;
    float f10 = b10 ? 0.0f : 1.0f;
    float f11 = b11 ? 0.0f : 1.0f;
    float f12 = b12 ? 0.0f : 1.0f;
    float f14 = b14 ? 0.0f : 1.0f;
    float f13 = b13 ? 0.0f : 1.0f;

    if (conditionA) {
      f16 /= 1000.0f;
      f17 /= 1000.0f;
      f18 /= 1000.0f;
      f19 /= 1000.0f;
      f20 /= 1000.0f;
      f21 /= 1000.0f;
      f15 /= 1000.0f;
      f08 /= 1000.0f;
      f09 /= 1000.0f;
      f10 /= 1000.0f;
      f11 /= 1000.0f;
      f12 /= 1000.0f;
      f30 /= 1000.0f;
      f31 /= 1000.0f;
      f32 /= 1000.0f;
      f33 /= 1000.0f;
      f34 /= 1000.0f;
      f01 /= 1000.0f;
      f02 /= 1000.0f;
      f03 /= 1000.0f;
      f04 /= 1000.0f;
      f05 /= 1000.0f;
      f23 /= 1000.0f;
      f24 /= 1000.0f;
      f25 /= 1000.0f;
      f26 /= 1000.0f;
      f27 /= 1000.0f;
      f22 /= 1000.0f;
      f00 /= 1000.0f;
      f14 /= 1000.0f;
      f13 /= 1000.0f;
      f36 /= 1000.0f;
      f35 /= 1000.0f;
      f07 /= 1000.0f;
      f06 /= 1000.0f;
      f29 /= 1000.0f;
      f28 /= 1000.0f;
    }
    // The parallel move that used to exhaust the ARM64 parallel move
    // resolver's scratch register pool (provided by VIXL) was in the
    // "else" branch of the following condition generated by ART's
    // compiler.
    if (conditionB) {
      f16 /= 100.0f;
      f17 /= 100.0f;
      f18 /= 100.0f;
      f19 /= 100.0f;
      f20 /= 100.0f;
      f21 /= 100.0f;
      f15 /= 100.0f;
      f08 /= 100.0f;
      f09 /= 100.0f;
      f10 /= 100.0f;
      f11 /= 100.0f;
      f12 /= 100.0f;
      f30 /= 100.0f;
      f31 /= 100.0f;
      f32 /= 100.0f;
      f33 /= 100.0f;
      f34 /= 100.0f;
      f01 /= 100.0f;
      f02 /= 100.0f;
      f03 /= 100.0f;
      f04 /= 100.0f;
      f05 /= 100.0f;
      f23 /= 100.0f;
      f24 /= 100.0f;
      f25 /= 100.0f;
      f26 /= 100.0f;
      f27 /= 100.0f;
      f22 /= 100.0f;
      f00 /= 100.0f;
      f14 /= 100.0f;
      f13 /= 100.0f;
      f36 /= 100.0f;
      f35 /= 100.0f;
      f07 /= 100.0f;
      f06 /= 100.0f;
      f29 /= 100.0f;
      f28 /= 100.0f;
    }
    if (conditionC) {
      f16 /= 12.0f;
      f17 /= 12.0f;
      f18 /= 12.0f;
      f19 /= 12.0f;
      f20 /= 12.0f;
      f21 /= 12.0f;
      f15 /= 12.0f;
      f08 /= 12.0f;
      f09 /= 12.0f;
      f10 /= 12.0f;
      f11 /= 12.0f;
      f12 /= 12.0f;
      f30 /= 12.0f;
      f31 /= 12.0f;
      f32 /= 12.0f;
      f33 /= 12.0f;
      f34 /= 12.0f;
      f01 /= 12.0f;
      f02 /= 12.0f;
      f03 /= 12.0f;
      f04 /= 12.0f;
      f05 /= 12.0f;
      f23 /= 12.0f;
      f24 /= 12.0f;
      f25 /= 12.0f;
      f26 /= 12.0f;
      f27 /= 12.0f;
      f22 /= 12.0f;
      f00 /= 12.0f;
      f14 /= 12.0f;
      f13 /= 12.0f;
      f36 /= 12.0f;
      f35 /= 12.0f;
      f07 /= 12.0f;
      f06 /= 12.0f;
      f29 /= 12.0f;
      f28 /= 12.0f;
    }
    float s = 0.0f;
    s = ((float) Math.round(100.0f * s)) / 100.0f;
    String res = s + r;
  }

  public static void main(String[] args) {
    Main main = new Main();
    main.test();
    System.out.println("passed");
  }
}
