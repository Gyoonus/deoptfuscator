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

//
// Test on (in)variant static field and array references in loops.
//
public class Main {

  private static Object anObject = new Object();
  private static Object anotherObject = new Object();

  //
  // Static fields.
  //

  private static boolean sZ;
  private static byte sB;
  private static char sC;
  private static short sS;
  private static int sI;
  private static long sJ;
  private static float sF;
  private static double sD;
  private static Object sL;

  //
  // Static arrays.
  //

  private static boolean[] sArrZ;
  private static byte[] sArrB;
  private static char[] sArrC;
  private static short[] sArrS;
  private static int[] sArrI;
  private static long[] sArrJ;
  private static float[] sArrF;
  private static double[] sArrD;
  private static Object[] sArrL;

  //
  // Loops on static arrays with invariant static field references.
  // The checker is used to ensure hoisting occurred.
  //

  /// CHECK-START: void Main.InvLoopZ() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopZ() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopZ() {
    for (int i = 0; i < sArrZ.length; i++) {
      sArrZ[i] = sZ;
    }
  }

  /// CHECK-START: void Main.InvLoopB() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopB() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopB() {
    for (int i = 0; i < sArrB.length; i++) {
      sArrB[i] = sB;
    }
  }

  /// CHECK-START: void Main.InvLoopC() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopC() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopC() {
    for (int i = 0; i < sArrC.length; i++) {
      sArrC[i] = sC;
    }
  }

  /// CHECK-START: void Main.InvLoopS() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopS() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopS() {
    for (int i = 0; i < sArrS.length; i++) {
      sArrS[i] = sS;
    }
  }

  /// CHECK-START: void Main.InvLoopI() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopI() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopI() {
    for (int i = 0; i < sArrI.length; i++) {
      sArrI[i] = sI;
    }
  }

  /// CHECK-START: void Main.InvLoopJ() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopJ() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopJ() {
    for (int i = 0; i < sArrJ.length; i++) {
      sArrJ[i] = sJ;
    }
  }

  /// CHECK-START: void Main.InvLoopF() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopF() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopF() {
    for (int i = 0; i < sArrF.length; i++) {
      sArrF[i] = sF;
    }
  }

  /// CHECK-START: void Main.InvLoopD() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopD() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopD() {
    for (int i = 0; i < sArrD.length; i++) {
      sArrD[i] = sD;
    }
  }

  /// CHECK-START: void Main.InvLoopL() licm (before)
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}
  /// CHECK-DAG: StaticFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopL() licm (after)
  /// CHECK-DAG: StaticFieldGet loop:none
  /// CHECK-DAG: StaticFieldGet loop:none

  private static void InvLoopL() {
    for (int i = 0; i < sArrL.length; i++) {
      sArrL[i] = sL;
    }
  }

  //
  // Loops on static arrays with variant static field references.
  // Incorrect hoisting is detected by incorrect outcome.
  //

  private static void VarLoopZ() {
    for (int i = 0; i < sArrZ.length; i++) {
      sArrZ[i] = sZ;
      if (i == 10)
        sZ = !sZ;
    }
  }

  private static void VarLoopB() {
    for (int i = 0; i < sArrB.length; i++) {
      sArrB[i] = sB;
      if (i == 10)
        sB++;
    }
  }

  private static void VarLoopC() {
    for (int i = 0; i < sArrC.length; i++) {
      sArrC[i] = sC;
      if (i == 10)
        sC++;
    }
  }

  private static void VarLoopS() {
    for (int i = 0; i < sArrS.length; i++) {
      sArrS[i] = sS;
      if (i == 10)
        sS++;
    }
  }

  private static void VarLoopI() {
    for (int i = 0; i < sArrI.length; i++) {
      sArrI[i] = sI;
      if (i == 10)
        sI++;
    }
  }

  private static void VarLoopJ() {
    for (int i = 0; i < sArrJ.length; i++) {
      sArrJ[i] = sJ;
      if (i == 10)
        sJ++;
    }
  }

  private static void VarLoopF() {
    for (int i = 0; i < sArrF.length; i++) {
      sArrF[i] = sF;
      if (i == 10)
        sF++;
    }
  }

  private static void VarLoopD() {
    for (int i = 0; i < sArrD.length; i++) {
      sArrD[i] = sD;
      if (i == 10)
        sD++;
    }
  }

  private static void VarLoopL() {
    for (int i = 0; i < sArrL.length; i++) {
      sArrL[i] = sL;
      if (i == 10)
        sL = anotherObject;
    }
  }

  //
  // Loops on static arrays with a cross-over reference.
  // Incorrect hoisting is detected by incorrect outcome.
  // In addition, the checker is used to detect no hoisting.
  //

  /// CHECK-START: void Main.CrossOverLoopZ() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopZ() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopZ() {
    sArrZ[20] = false;
    for (int i = 0; i < sArrZ.length; i++) {
      sArrZ[i] = !sArrZ[20];
    }
  }

  /// CHECK-START: void Main.CrossOverLoopB() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopB() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopB() {
    sArrB[20] = 11;
    for (int i = 0; i < sArrB.length; i++) {
      sArrB[i] = (byte)(sArrB[20] + 2);
    }
  }

  /// CHECK-START: void Main.CrossOverLoopC() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopC() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopC() {
    sArrC[20] = 11;
    for (int i = 0; i < sArrC.length; i++) {
      sArrC[i] = (char)(sArrC[20] + 2);
    }
  }

  /// CHECK-START: void Main.CrossOverLoopS() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopS() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopS() {
    sArrS[20] = 11;
    for (int i = 0; i < sArrS.length; i++) {
      sArrS[i] = (short)(sArrS[20] + 2);
    }
  }

  /// CHECK-START: void Main.CrossOverLoopI() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopI() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopI() {
    sArrI[20] = 11;
    for (int i = 0; i < sArrI.length; i++) {
      sArrI[i] = sArrI[20] + 2;
    }
  }

  /// CHECK-START: void Main.CrossOverLoopJ() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopJ() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopJ() {
    sArrJ[20] = 11;
    for (int i = 0; i < sArrJ.length; i++) {
      sArrJ[i] = sArrJ[20] + 2;
    }
  }

  /// CHECK-START: void Main.CrossOverLoopF() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopF() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopF() {
    sArrF[20] = 11;
    for (int i = 0; i < sArrF.length; i++) {
      sArrF[i] = sArrF[20] + 2;
    }
  }

  /// CHECK-START: void Main.CrossOverLoopD() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopD() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopD() {
    sArrD[20] = 11;
    for (int i = 0; i < sArrD.length; i++) {
      sArrD[i] = sArrD[20] + 2;
    }
  }

  /// CHECK-START: void Main.CrossOverLoopL() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.CrossOverLoopL() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void CrossOverLoopL() {
    sArrL[20] = anotherObject;
    for (int i = 0; i < sArrL.length; i++) {
      sArrL[i] = (sArrL[20] == anObject) ? anotherObject : anObject;
    }
  }

  //
  // False cross-over loops on static arrays with data types (I/F and J/D) that used
  // to be aliased in an older version of the compiler. This alias has been removed,
  // however, which enables hoisting the invariant array reference.
  //

  /// CHECK-START: void Main.FalseCrossOverLoop1() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.FalseCrossOverLoop1() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void FalseCrossOverLoop1() {
    sArrF[20] = -1;
    for (int i = 0; i < sArrI.length; i++) {
      sArrI[i] = (int) sArrF[20] - 2;
    }
  }

  /// CHECK-START: void Main.FalseCrossOverLoop2() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.FalseCrossOverLoop2() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void FalseCrossOverLoop2() {
    sArrI[20] = -2;
    for (int i = 0; i < sArrF.length; i++) {
      sArrF[i] = sArrI[20] - 2;
    }
  }

  /// CHECK-START: void Main.FalseCrossOverLoop3() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.FalseCrossOverLoop3() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void FalseCrossOverLoop3() {
    sArrD[20] = -3;
    for (int i = 0; i < sArrJ.length; i++) {
      sArrJ[i] = (long) sArrD[20] - 2;
    }
  }

  /// CHECK-START: void Main.FalseCrossOverLoop4() licm (before)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:{{B\d+}}
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  /// CHECK-START: void Main.FalseCrossOverLoop4() licm (after)
  /// CHECK-DAG: ArraySet loop:none
  /// CHECK-DAG: ArrayGet loop:none
  /// CHECK-DAG: ArraySet loop:{{B\d+}}

  private static void FalseCrossOverLoop4() {
    sArrJ[20] = -4;
    for (int i = 0; i < sArrD.length; i++) {
      sArrD[i] = sArrJ[20] - 2;
    }
  }

  //
  // Main driver and testers.
  //

  public static void main(String[] args) {
    DoStaticTests();
    System.out.println("passed");
  }

  private static void DoStaticTests() {
    // Type Z.
    sZ = true;
    sArrZ = new boolean[100];
    InvLoopZ();
    for (int i = 0; i < sArrZ.length; i++) {
      expectEquals(true, sArrZ[i]);
    }
    VarLoopZ();
    for (int i = 0; i < sArrZ.length; i++) {
      expectEquals(i <= 10, sArrZ[i]);
    }
    CrossOverLoopZ();
    for (int i = 0; i < sArrZ.length; i++) {
      expectEquals(i <= 20, sArrZ[i]);
    }
    // Type B.
    sB = 1;
    sArrB = new byte[100];
    InvLoopB();
    for (int i = 0; i < sArrB.length; i++) {
      expectEquals(1, sArrB[i]);
    }
    VarLoopB();
    for (int i = 0; i < sArrB.length; i++) {
      expectEquals(i <= 10 ? 1 : 2, sArrB[i]);
    }
    CrossOverLoopB();
    for (int i = 0; i < sArrB.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrB[i]);
    }
    // Type C.
    sC = 2;
    sArrC = new char[100];
    InvLoopC();
    for (int i = 0; i < sArrC.length; i++) {
      expectEquals(2, sArrC[i]);
    }
    VarLoopC();
    for (int i = 0; i < sArrC.length; i++) {
      expectEquals(i <= 10 ? 2 : 3, sArrC[i]);
    }
    CrossOverLoopC();
    for (int i = 0; i < sArrC.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrC[i]);
    }
    // Type S.
    sS = 3;
    sArrS = new short[100];
    InvLoopS();
    for (int i = 0; i < sArrS.length; i++) {
      expectEquals(3, sArrS[i]);
    }
    VarLoopS();
    for (int i = 0; i < sArrS.length; i++) {
      expectEquals(i <= 10 ? 3 : 4, sArrS[i]);
    }
    CrossOverLoopS();
    for (int i = 0; i < sArrS.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrS[i]);
    }
    // Type I.
    sI = 4;
    sArrI = new int[100];
    InvLoopI();
    for (int i = 0; i < sArrI.length; i++) {
      expectEquals(4, sArrI[i]);
    }
    VarLoopI();
    for (int i = 0; i < sArrI.length; i++) {
      expectEquals(i <= 10 ? 4 : 5, sArrI[i]);
    }
    CrossOverLoopI();
    for (int i = 0; i < sArrI.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrI[i]);
    }
    // Type J.
    sJ = 5;
    sArrJ = new long[100];
    InvLoopJ();
    for (int i = 0; i < sArrJ.length; i++) {
      expectEquals(5, sArrJ[i]);
    }
    VarLoopJ();
    for (int i = 0; i < sArrJ.length; i++) {
      expectEquals(i <= 10 ? 5 : 6, sArrJ[i]);
    }
    CrossOverLoopJ();
    for (int i = 0; i < sArrJ.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrJ[i]);
    }
    // Type F.
    sF = 6.0f;
    sArrF = new float[100];
    InvLoopF();
    for (int i = 0; i < sArrF.length; i++) {
      expectEquals(6, sArrF[i]);
    }
    VarLoopF();
    for (int i = 0; i < sArrF.length; i++) {
      expectEquals(i <= 10 ? 6 : 7, sArrF[i]);
    }
    CrossOverLoopF();
    for (int i = 0; i < sArrF.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrF[i]);
    }
    // Type D.
    sD = 7.0;
    sArrD = new double[100];
    InvLoopD();
    for (int i = 0; i < sArrD.length; i++) {
      expectEquals(7.0, sArrD[i]);
    }
    VarLoopD();
    for (int i = 0; i < sArrD.length; i++) {
      expectEquals(i <= 10 ? 7 : 8, sArrD[i]);
    }
    CrossOverLoopD();
    for (int i = 0; i < sArrD.length; i++) {
      expectEquals(i <= 20 ? 13 : 15, sArrD[i]);
    }
    // Type L.
    sL = anObject;
    sArrL = new Object[100];
    InvLoopL();
    for (int i = 0; i < sArrL.length; i++) {
      expectEquals(anObject, sArrL[i]);
    }
    VarLoopL();
    for (int i = 0; i < sArrL.length; i++) {
      expectEquals(i <= 10 ? anObject : anotherObject, sArrL[i]);
    }
    CrossOverLoopL();
    for (int i = 0; i < sArrL.length; i++) {
      expectEquals(i <= 20 ? anObject : anotherObject, sArrL[i]);
    }
    // False cross-over.
    FalseCrossOverLoop1();
    for (int i = 0; i < sArrI.length; i++) {
      expectEquals(-3, sArrI[i]);
    }
    FalseCrossOverLoop2();
    for (int i = 0; i < sArrF.length; i++) {
      expectEquals(-4, sArrF[i]);
    }
    FalseCrossOverLoop3();
    for (int i = 0; i < sArrJ.length; i++) {
      expectEquals(-5, sArrJ[i]);
    }
    FalseCrossOverLoop4();
    for (int i = 0; i < sArrD.length; i++) {
      expectEquals(-6, sArrD[i]);
    }
  }

  private static void expectEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(Object expected, Object result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
