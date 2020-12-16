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

/**
 * Automatically generated fuzz test that exposed bug in the way bounds
 * check elimination visits basic blocks. If, after dynamic bce, the same
 * block would be visited again, then static length based bce would incorrectly
 * feed information back to itself and removed a necessary bounds check.
 */
public class Main {

  private static int[][][] mA = new int[10][10][10];

  private static int mX = 17;

  private static int doit() {
    int l0 = (((++mA[7][2][8]) <= mA[0][1][3]) ? (++mA[9][0][5]) : ((( -mA[0][7][0]) * ((mX == mX) ? 180 : mX)) + (mA[7][8][8]++)));
    mA[1][0][4] -= mX;
    int l1 = (((l0 >= ( ~mA[6][7][5])) && ((921 <= l0) && (mA[3][9][6] > l0))) ? mX : (l0--));
    int l2 = ( -384);
    for (int i0 = 7 - 1; i0 >= 1; i0--) {
      mA[6][0][0] -= ((((l0++) == ( -mX)) ? (((mA[3][i0][1] > 503) || (mX <= i0)) ? (--l0) : (l0--)) : mX) - ( ~(mX--)));
      int l3 = 24;
      int l4 = ((l2--) & mX);
      for (int i1 = i0-2 - 1; i1 >= 3; i1--) {
        for (int i2 = 2; i2 < i0; i2++) {
          mA[i0][4][l3] >>= 1;
        }
      }
    }
    return 1;
  }

  public static void main(String[] args) {
    int k = 1;
    for (int i0 = 0; i0 < 10; i0++)
    for (int i1 = 0; i1 < 10; i1++)
    for (int i2 = 0; i2 < 10; i2++)
      mA[i0][i1][i2] = k++;
    try {
      k = doit();
    } catch (Exception e) {
      System.out.println("exception caught");
    }
    System.out.println("FUZZ result = " + k + " " + mX);
  }
}
