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
// Test on (in)variant instance field and array references in loops.
//
public class Main {

  private static Object anObject = new Object();
  private static Object anotherObject = new Object();

  //
  // Instance fields.
  //

  private boolean mZ;
  private byte mB;
  private char mC;
  private short mS;
  private int mI;
  private long mJ;
  private float mF;
  private double mD;
  private Object mL;

  //
  // Instance arrays.
  //

  private boolean[] mArrZ;
  private byte[] mArrB;
  private char[] mArrC;
  private short[] mArrS;
  private int[] mArrI;
  private long[] mArrJ;
  private float[] mArrF;
  private double[] mArrD;
  private Object[] mArrL;

  //
  // Loops on instance arrays with invariant instance field references.
  // The checker is used to ensure hoisting occurred.
  //

  /// CHECK-START: void Main.InvLoopZ() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopZ() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopZ() {
    for (int i = 0; i < mArrZ.length; i++) {
      mArrZ[i] = mZ;
    }
  }

  /// CHECK-START: void Main.InvLoopB() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopB() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopB() {
    for (int i = 0; i < mArrB.length; i++) {
      mArrB[i] = mB;
    }
  }

  /// CHECK-START: void Main.InvLoopC() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopC() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopC() {
    for (int i = 0; i < mArrC.length; i++) {
      mArrC[i] = mC;
    }
  }

  /// CHECK-START: void Main.InvLoopS() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopS() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopS() {
    for (int i = 0; i < mArrS.length; i++) {
      mArrS[i] = mS;
    }
  }

  /// CHECK-START: void Main.InvLoopI() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopI() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopI() {
    for (int i = 0; i < mArrI.length; i++) {
      mArrI[i] = mI;
    }
  }

  /// CHECK-START: void Main.InvLoopJ() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopJ() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopJ() {
    for (int i = 0; i < mArrJ.length; i++) {
      mArrJ[i] = mJ;
    }
  }

  /// CHECK-START: void Main.InvLoopF() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopF() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopF() {
    for (int i = 0; i < mArrF.length; i++) {
      mArrF[i] = mF;
    }
  }

  /// CHECK-START: void Main.InvLoopD() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopD() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopD() {
    for (int i = 0; i < mArrD.length; i++) {
      mArrD[i] = mD;
    }
  }

  /// CHECK-START: void Main.InvLoopL() licm (before)
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}
  /// CHECK-DAG: InstanceFieldGet loop:{{B\d+}}

  /// CHECK-START: void Main.InvLoopL() licm (after)
  /// CHECK-DAG: InstanceFieldGet loop:none
  /// CHECK-DAG: InstanceFieldGet loop:none

  private void InvLoopL() {
    for (int i = 0; i < mArrL.length; i++) {
      mArrL[i] = mL;
    }
  }

  //
  // Loops on instance arrays with variant instance field references.
  // Incorrect hoisting is detected by incorrect outcome.
  //

  private void VarLoopZ() {
    for (int i = 0; i < mArrZ.length; i++) {
      mArrZ[i] = mZ;
      if (i == 10)
        mZ = !mZ;
    }
  }

  private void VarLoopB() {
    for (int i = 0; i < mArrB.length; i++) {
      mArrB[i] = mB;
      if (i == 10)
        mB++;
    }
  }

  private void VarLoopC() {
    for (int i = 0; i < mArrC.length; i++) {
      mArrC[i] = mC;
      if (i == 10)
        mC++;
    }
  }

  private void VarLoopS() {
    for (int i = 0; i < mArrS.length; i++) {
      mArrS[i] = mS;
      if (i == 10)
        mS++;
    }
  }

  private void VarLoopI() {
    for (int i = 0; i < mArrI.length; i++) {
      mArrI[i] = mI;
      if (i == 10)
        mI++;
    }
  }

  private void VarLoopJ() {
    for (int i = 0; i < mArrJ.length; i++) {
      mArrJ[i] = mJ;
      if (i == 10)
        mJ++;
    }
  }

  private void VarLoopF() {
    for (int i = 0; i < mArrF.length; i++) {
      mArrF[i] = mF;
      if (i == 10)
        mF++;
    }
  }

  private void VarLoopD() {
    for (int i = 0; i < mArrD.length; i++) {
      mArrD[i] = mD;
      if (i == 10)
        mD++;
    }
  }

  private void VarLoopL() {
    for (int i = 0; i < mArrL.length; i++) {
      mArrL[i] = mL;
      if (i == 10)
        mL = anotherObject;
    }
  }

  //
  // Loops on instance arrays with a cross-over reference.
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

  private void CrossOverLoopZ() {
    mArrZ[20] = false;
    for (int i = 0; i < mArrZ.length; i++) {
      mArrZ[i] = !mArrZ[20];
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

  private void CrossOverLoopB() {
    mArrB[20] = 111;
    for (int i = 0; i < mArrB.length; i++) {
      mArrB[i] = (byte)(mArrB[20] + 2);
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

  private void CrossOverLoopC() {
    mArrC[20] = 111;
    for (int i = 0; i < mArrC.length; i++) {
      mArrC[i] = (char)(mArrC[20] + 2);
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

  private void CrossOverLoopS() {
    mArrS[20] = 111;
    for (int i = 0; i < mArrS.length; i++) {
      mArrS[i] = (short)(mArrS[20] + 2);
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

  private void CrossOverLoopI() {
    mArrI[20] = 111;
    for (int i = 0; i < mArrI.length; i++) {
      mArrI[i] = mArrI[20] + 2;
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

  private void CrossOverLoopJ() {
    mArrJ[20] = 111;
    for (int i = 0; i < mArrJ.length; i++) {
      mArrJ[i] = mArrJ[20] + 2;
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

  private void CrossOverLoopF() {
    mArrF[20] = 111;
    for (int i = 0; i < mArrF.length; i++) {
      mArrF[i] = mArrF[20] + 2;
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

  private void CrossOverLoopD() {
    mArrD[20] = 111;
    for (int i = 0; i < mArrD.length; i++) {
      mArrD[i] = mArrD[20] + 2;
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

  private void CrossOverLoopL() {
    mArrL[20] = anotherObject;
    for (int i = 0; i < mArrL.length; i++) {
      mArrL[i] = (mArrL[20] == anObject) ? anotherObject : anObject;
    }
  }

  //
  // False cross-over loops on instance arrays with data types (I/F and J/D) that used
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

  private void FalseCrossOverLoop1() {
    mArrF[20] = -1;
    for (int i = 0; i < mArrI.length; i++) {
      mArrI[i] = (int) mArrF[20] - 2;
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

  private void FalseCrossOverLoop2() {
    mArrI[20] = -2;
    for (int i = 0; i < mArrF.length; i++) {
      mArrF[i] = mArrI[20] - 2;
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

  private void FalseCrossOverLoop3() {
    mArrD[20] = -3;
    for (int i = 0; i < mArrJ.length; i++) {
      mArrJ[i] = (long) mArrD[20] - 2;
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

  private void FalseCrossOverLoop4() {
    mArrJ[20] = -4;
    for (int i = 0; i < mArrD.length; i++) {
      mArrD[i] = mArrJ[20] - 2;
    }
  }

  //
  // Main driver and testers.
  //

  public static void main(String[] args) {
    new Main().DoInstanceTests();
    System.out.println("passed");
  }

  private void DoInstanceTests() {
    // Type Z.
    mZ = true;
    mArrZ = new boolean[100];
    InvLoopZ();
    for (int i = 0; i < mArrZ.length; i++) {
      expectEquals(true, mArrZ[i]);
    }
    VarLoopZ();
    for (int i = 0; i < mArrZ.length; i++) {
      expectEquals(i <= 10, mArrZ[i]);
    }
    CrossOverLoopZ();
    for (int i = 0; i < mArrZ.length; i++) {
      expectEquals(i <= 20, mArrZ[i]);
    }
    // Type B.
    mB = 1;
    mArrB = new byte[100];
    InvLoopB();
    for (int i = 0; i < mArrB.length; i++) {
      expectEquals(1, mArrB[i]);
    }
    VarLoopB();
    for (int i = 0; i < mArrB.length; i++) {
      expectEquals(i <= 10 ? 1 : 2, mArrB[i]);
    }
    CrossOverLoopB();
    for (int i = 0; i < mArrB.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrB[i]);
    }
    // Type C.
    mC = 2;
    mArrC = new char[100];
    InvLoopC();
    for (int i = 0; i < mArrC.length; i++) {
      expectEquals(2, mArrC[i]);
    }
    VarLoopC();
    for (int i = 0; i < mArrC.length; i++) {
      expectEquals(i <= 10 ? 2 : 3, mArrC[i]);
    }
    CrossOverLoopC();
    for (int i = 0; i < mArrC.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrC[i]);
    }
    // Type S.
    mS = 3;
    mArrS = new short[100];
    InvLoopS();
    for (int i = 0; i < mArrS.length; i++) {
      expectEquals(3, mArrS[i]);
    }
    VarLoopS();
    for (int i = 0; i < mArrS.length; i++) {
      expectEquals(i <= 10 ? 3 : 4, mArrS[i]);
    }
    CrossOverLoopS();
    for (int i = 0; i < mArrS.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrS[i]);
    }
    // Type I.
    mI = 4;
    mArrI = new int[100];
    InvLoopI();
    for (int i = 0; i < mArrI.length; i++) {
      expectEquals(4, mArrI[i]);
    }
    VarLoopI();
    for (int i = 0; i < mArrI.length; i++) {
      expectEquals(i <= 10 ? 4 : 5, mArrI[i]);
    }
    CrossOverLoopI();
    for (int i = 0; i < mArrI.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrI[i]);
    }
    // Type J.
    mJ = 5;
    mArrJ = new long[100];
    InvLoopJ();
    for (int i = 0; i < mArrJ.length; i++) {
      expectEquals(5, mArrJ[i]);
    }
    VarLoopJ();
    for (int i = 0; i < mArrJ.length; i++) {
      expectEquals(i <= 10 ? 5 : 6, mArrJ[i]);
    }
    CrossOverLoopJ();
    for (int i = 0; i < mArrJ.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrJ[i]);
    }
    // Type F.
    mF = 6.0f;
    mArrF = new float[100];
    InvLoopF();
    for (int i = 0; i < mArrF.length; i++) {
      expectEquals(6, mArrF[i]);
    }
    VarLoopF();
    for (int i = 0; i < mArrF.length; i++) {
      expectEquals(i <= 10 ? 6 : 7, mArrF[i]);
    }
    CrossOverLoopF();
    for (int i = 0; i < mArrF.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrF[i]);
    }
    // Type D.
    mD = 7.0;
    mArrD = new double[100];
    InvLoopD();
    for (int i = 0; i < mArrD.length; i++) {
      expectEquals(7.0, mArrD[i]);
    }
    VarLoopD();
    for (int i = 0; i < mArrD.length; i++) {
      expectEquals(i <= 10 ? 7 : 8, mArrD[i]);
    }
    CrossOverLoopD();
    for (int i = 0; i < mArrD.length; i++) {
      expectEquals(i <= 20 ? 113 : 115, mArrD[i]);
    }
    // Type L.
    mL = anObject;
    mArrL = new Object[100];
    InvLoopL();
    for (int i = 0; i < mArrL.length; i++) {
      expectEquals(anObject, mArrL[i]);
    }
    VarLoopL();
    for (int i = 0; i < mArrL.length; i++) {
      expectEquals(i <= 10 ? anObject : anotherObject, mArrL[i]);
    }
    CrossOverLoopL();
    for (int i = 0; i < mArrL.length; i++) {
      expectEquals(i <= 20 ? anObject : anotherObject, mArrL[i]);
    }
    // False cross-over.
    FalseCrossOverLoop1();
    for (int i = 0; i < mArrI.length; i++) {
      expectEquals(-3, mArrI[i]);
    }
    FalseCrossOverLoop2();
    for (int i = 0; i < mArrF.length; i++) {
      expectEquals(-4, mArrF[i]);
    }
    FalseCrossOverLoop3();
    for (int i = 0; i < mArrJ.length; i++) {
      expectEquals(-5, mArrJ[i]);
    }
    FalseCrossOverLoop4();
    for (int i = 0; i < mArrD.length; i++) {
      expectEquals(-6, mArrD[i]);
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
