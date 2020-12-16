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
  public static int FIBCOUNT = 64;
  public static int[] fibs;

  /// CHECK-START-X86_64: int Main.test() disassembly (after)
  /// CHECK-DAG:   <<Zero:i\d+>>        IntConstant 0
  //
  /// CHECK:                            If
  /// CHECK-NEXT:                       cmp
  /// CHECK-NEXT:                       jle/ng
  //
  /// CHECK-DAG:   <<Fibs:l\d+>>        StaticFieldGet
  /// CHECK-DAG:                        NullCheck [<<Fibs>>]
  /// CHECK-NOT:                        jmp
  /// CHECK-DAG:   <<FibsAtZero:i\d+>>  ArrayGet [<<Fibs>>,<<Zero>>]
  /// CHECK-DAG:                        Return [<<FibsAtZero>>]
  //
  // Checks that there is no conditional jump over a `jmp`
  // instruction. The `ArrayGet` instruction is in the next block.
  //
  // Note that the `StaticFieldGet` HIR instruction above (captured as
  // `Fibs`) can produce a `jmp` x86-64 instruction when read barriers
  // are enabled (to jump into the read barrier slow path), which is
  // different from the `jmp` in the `CHECK-NOT` assertion.
  public static int test() {
    for (int i = 1; ; i++) {
      if (i >= FIBCOUNT) {
        return fibs[0];
      }
      fibs[i] = (i + fibs[(i - 1)]);
    }
  }

  public static void main(String[] args) {
    fibs = new int[FIBCOUNT];
    fibs[0] = 1;
    test();
  }
}
