/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * Tests for zero vectorization.
 */
public class Main {

  /// CHECK-START: void Main.staticallyAligned(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<One>>,<<AddI:i\d+>>]          loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>   Add [<<Get>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Phi>>,<<Add>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.staticallyAligned(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Vl:i\d+>>    IntConstant 2                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]                            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Nrm:i\d+>>   Add [<<Phi>>,<<One>>]                                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Nrm>>]          alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Repl>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Nrm>>,<<Add>>] alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<Vl>>]                                    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                ArrayGet
  /// CHECK-NOT:                ArraySet
  static void staticallyAligned(int[] a) {
    // Starts at offset 12 (hidden) + 1 * 4 relative to base alignment.
    // So no peeling, aligned vector, no cleanup.
    for (int i = 1; i < 9; i++) {
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyAlignedN(int[]) loop_optimization (before)
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Par:l\d+>>   NullCheck                            loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<One>>,<<AddI:i\d+>>]          loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>   Add [<<Get>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Phi>>,<<Add>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.staticallyAlignedN(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Vl:i\d+>>    IntConstant 2                        loop:none
  /// CHECK-DAG: <<Par:l\d+>>   NullCheck                            loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]                            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Nrm:i\d+>>   Add [<<Phi>>,<<One>>]                                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Nrm>>]          alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Repl>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Nrm>>,<<Add>>] alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<Vl>>]                                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<PhiC:i\d+>>  Phi [<<Phi>>,<<AddIC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<NrmC:i\d+>>  Add [<<PhiC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<NrmC>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<NrmC>>,<<AddC>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddIC>>      Add [<<PhiC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyAlignedN(int[] a) {
    // Starts at offset 12 (hidden) + 1 * 4 relative to base alignment.
    // So no peeling, aligned vector, cleanup.
    for (int i = 1; i < a.length; i++) {
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyMisaligned(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>   Add [<<Get>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Phi>>,<<Add>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.staticallyMisaligned(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Vl:i\d+>>    IntConstant 2                        loop:none
  /// CHECK-DAG: <<PhiP:i\d+>>  Phi [<<Zero>>,<<AddIP:i\d+>>]        loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<PhiP>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<PhiP>>,<<AddP>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddIP>>      Add [<<PhiP>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<PhiP>>,<<AddI:i\d+>>]                            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Phi>>]          alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Repl>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Phi>>,<<Add>>] alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<Vl>>]                                    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                ArrayGet
  /// CHECK-NOT:                ArraySet
  static void staticallyMisaligned(int[] a) {
    // Starts at offset 12 (hidden) + 0 * 4 relative to base alignment.
    // Yes, Art runtime misaligns the most common access pattern :-(
    // Static peeling to the rescue, aligned vector, no cleanup.
    for (int i = 0; i < 9; i++) {
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyMisalignedN(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                       loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                       loop:none
  /// CHECK-DAG: <<Par:l\d+>>   NullCheck                           loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>   Add [<<Get>>,<<One>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<One>>]               loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.staticallyMisalignedN(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Vl:i\d+>>    IntConstant 2                        loop:none
  /// CHECK-DAG: <<Par:l\d+>>   NullCheck                            loop:none
  /// CHECK-DAG: <<PhiP:i\d+>>  Phi [<<Zero>>,<<AddIP:i\d+>>]        loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<GetP:i\d+>>  ArrayGet [<<Par>>,<<PhiP>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP:i\d+>>  Add [<<GetP>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<PhiP>>,<<AddP>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddIP>>      Add [<<PhiP>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<PhiP>>,<<AddI:i\d+>>]                            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Phi>>]          alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Repl>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Phi>>,<<Add>>] alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<Vl>>]                                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<PhiC:i\d+>>  Phi [<<Phi>>,<<AddIC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>  ArrayGet [<<Par>>,<<PhiC>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>  Add [<<GetC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<PhiC>>,<<AddC>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddIC>>      Add [<<PhiC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyMisalignedN(int[] a) {
    // Starts at offset 12 (hidden) + 0 * 4 relative to base alignment.
    // Yes, Art runtime misaligns the most common access pattern :-(
    // Static peeling to the rescue, aligned vector, cleanup.
    for (int i = 0; i < a.length; i++) {
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyUnknownAligned(int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Off:i\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Nrm:i\d+>>   Add [<<Off>>,<<Phi>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Nrm>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>   Add [<<Get>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Nrm>>,<<Add>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.staticallyUnknownAligned(int[], int) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Off:i\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Vl:i\d+>>    IntConstant 2                        loop:none
  /// CHECK-DAG: <<PhiP:i\d+>>  Phi [<<Zero>>,<<AddIP:i\d+>>]        loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<NrmP:i\d+>>  Add [<<PhiP>>,<<Off>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<NrmP>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<NrmP>>,<<AddP>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddIP>>      Add [<<PhiP>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<PhiP>>,<<AddI:i\d+>>]                            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Nrm:i\d+>>   Add [<<Phi>>,<<Off>>]                                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Nrm>>]          alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Repl>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Nrm>>,<<Add>>] alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<Vl>>]                                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<PhiC:i\d+>>  Phi [<<Phi>>,<<AddIC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<NrmC:i\d+>>  Add [<<PhiC>>,<<Off>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>  ArrayGet [<<Par>>,<<NrmC>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>  Add [<<GetC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<NrmC>>,<<AddC>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddIC>>      Add [<<PhiC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyUnknownAligned(int[] a, int off) {
    // Starts at an unknown offset due to parameter off.
    // Dynamic peeling to the rescue, aligned vector, cleanup.
    for (int i = 0; i < 9; i++) {
      a[off + i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyUnknownAlignedN(int[], int, int) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Off:i\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Nrm:i\d+>>   Add [<<Off>>,<<Phi>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Nrm>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>   Add [<<Get>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Nrm>>,<<Add>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<One>>]                loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM: void Main.staticallyUnknownAlignedN(int[], int, int) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Off:i\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Vl:i\d+>>    IntConstant 2                        loop:none
  /// CHECK-DAG: <<PhiP:i\d+>>  Phi [<<Zero>>,<<AddIP:i\d+>>]        loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<NrmP:i\d+>>  Add [<<PhiP>>,<<Off>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<NrmP>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<NrmP>>,<<AddP>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddIP>>      Add [<<PhiP>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<PhiP>>,<<AddI:i\d+>>]                            loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Nrm:i\d+>>   Add [<<Phi>>,<<Off>>]                                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Nrm>>]          alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>   VecAdd [<<Load>>,<<Repl>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Nrm>>,<<Add>>] alignment:ALIGN(8,0) loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi>>,<<Vl>>]                                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<PhiC:i\d+>>  Phi [<<Phi>>,<<AddIC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<NrmC:i\d+>>  Add [<<PhiC>>,<<Off>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>  ArrayGet [<<Par>>,<<NrmC>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>  Add [<<GetC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<NrmC>>,<<AddC>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddIC>>      Add [<<PhiC>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyUnknownAlignedN(int[] a, int off, int n) {
    // Starts at an unknown offset due to parameter off.
    // Dynamic peeling to the rescue, aligned vector, cleanup.
    for (int i = 0; i < n; i++) {
      a[off + i] += 1;
    }
  }

  //
  // Test drivers.
  //

  private static void test1() {
    int[] a = new int[9];
    staticallyAligned(a);
    for (int i = 0; i < a.length; i++) {
      int e = i > 0 ? 1 : 0;
      expectEquals(e, a[i]);
    }
  }

  private static void test2() {
    for (int n = 0; n <= 71; n++) {
      int[] a = new int[n];
      staticallyAlignedN(a);
      for (int i = 0; i < a.length; i++) {
        int e = i > 0 ? 1 : 0;
        expectEquals(e, a[i]);
      }
    }
  }

  private static void test3() {
    int[] a = new int[9];
    staticallyMisaligned(a);
    for (int i = 0; i < a.length; i++) {
      expectEquals(1, a[i]);
    }
  }

  private static void test4() {
    for (int n = 0; n <= 71; n++) {
      int[] a = new int[n];
      staticallyMisalignedN(a);
      for (int i = 0; i < a.length; i++) {
        expectEquals(1, a[i]);
      }
    }
  }

  private static void test5() {
    for (int off = 0; off <= 8; off++) {
      int[] a = new int[17];
      staticallyUnknownAligned(a, off);
      for (int i = 0; i < a.length; i++) {
        int e = (off <= i && i < off + 9) ? 1 : 0;
        expectEquals(e, a[i]);
      }
    }
  }

  private static void test6() {
    for (int off = 0; off <= 8; off++) {
      for (int n = 0; n <= 9; n++) {
        int[] a = new int[17];
        staticallyUnknownAlignedN(a, off, n);
        for (int i = 0; i < a.length; i++) {
          int e = (off <= i && i < off + n) ? 1 : 0;
          expectEquals(e, a[i]);
        }
      }
    }
  }

  public static void main(String[] args) {
    test1();
    test2();
    test4();
    test5();
    test6();
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
