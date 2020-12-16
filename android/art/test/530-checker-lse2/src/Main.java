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

import java.util.Arrays;

// Modified from a fuzz test.
public class Main {

  private interface X {
    int x();
  }

  private class A {
    public int a() {
      return (+ (Math.multiplyExact(mI, mI)));
    }
  }

  private class B extends A implements X {
    public int a() {
      return super.a() + ((int) (Math.max(364746077.0f, ((float) mD))));
    }
    public int x() {
      return (mI >> (mI++));
    }
  }

  private static class C implements X {
    public static int s() {
      return 671468641;
    }
    public int c() {
      return -383762838;
    }
    public int x() {
      return -138813312;
    }
  }

  private A mA  = new B();
  private B mB  = new B();
  private X mBX = new B();
  private C mC  = new C();
  private X mCX = new C();

  private boolean mZ = false;
  private int     mI = 0;
  private long    mJ = 0;
  private float   mF = 0;
  private double  mD = 0;

  private boolean[] mArray = new boolean[576];

  private Main() {
    boolean a = false;
    for (int i0 = 0; i0 < 576; i0++) {
      mArray[i0] = a;
      a = !a;
    }
  }

  /// CHECK-START: float Main.testMethod() load_store_elimination (before)
  /// CHECK-DAG: Deoptimize
  /// CHECK-DAG: Deoptimize
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: ConstructorFence
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: NewInstance
  /// CHECK-DAG: NewInstance
  /// CHECK-NOT: NewInstance

  /// CHECK-START: float Main.testMethod() load_store_elimination (after)
  /// CHECK-DAG: Deoptimize
  /// CHECK-DAG: Deoptimize
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: ConstructorFence

  private float testMethod() {
    {
      // Each of the "new" statements here will initialize an object with final fields,
      // which after inlining will also retain a constructor fence.
      //
      // After LSE we remove the 'new-instance' and the associated constructor fence.
      int lI0 = (-1456058746 << mI);
      mD = ((double)(int)(double) mD);
      for (int i0 = 56 - 1; i0 >= 0; i0--) {
        mArray[i0] &= (Boolean.logicalOr(((true ? ((boolean) new Boolean((mZ))) : mZ) || mArray[i0]), (mZ)));
        mF *= (mF * mF);
        if ((mZ ^ true)) {
          mF *= ((float)(int)(float) 267827331.0f);
          mZ ^= ((false & ((boolean) new Boolean(false))) | mZ);
          for (int i1 = 576 - 1; i1 >= 0; i1--) {
            mZ &= ((mArray[279]) | ((boolean) new Boolean(true)));
            mD -= (--mD);
            for (int i2 = 56 - 1; i2 >= 0; i2--) {
              mF /= (mF - mF);
              mI = (Math.min(((int) new Integer(mI)), (766538816 * (++mI))));
              mF += (mZ ? (mB.a()) : ((! mZ) ? -752042357.0f : (++mF)));
              mJ |= ((long) new Long((-2084191070L + (mJ | mJ))));
              lI0 |= ((int) new Integer(((int) new Integer(mI))));
              if (((boolean) new Boolean(false))) {
                mZ &= (mZ);
                mF *= (mF--);
                mD = (Double.POSITIVE_INFINITY);
                mF += ((float)(int)(float) (-2026938813.0f * 638401585.0f));
                mJ = (--mJ);
                for (int i3 = 56 - 1; i3 >= 0; i3--) {
                  mI &= (- mI);
                  mD = (--mD);
                  mArray[426] = (mZ || false);
                  mF -= (((this instanceof Main) ? mF : mF) + 976981405.0f);
                  mZ &= ((mZ) & (this instanceof Main));
                }
                mZ ^= (Float.isFinite(-1975953895.0f));
              } else {
                mJ /= ((long) (Math.nextDown(-1519600008.0f)));
                mJ <<= (Math.round(1237681786.0));
              }
            }
            mArray[i0] &= (false || ((1256071300.0f != -353296391.0f) ? false : (mZ ^ mArray[i0])));
            mF *= (+ ((float) mD));
            for (int i2 = 0; i2 < 576; i2++) {
              mD *= ((double) lI0);
              lI0 = (lI0 & (Integer.MIN_VALUE));
              mF -= (--mF);
            }
            if ((this instanceof Main)) {
              mZ ^= ((boolean) new Boolean(true));
            } else {
              {
                int lI1 = (mZ ? (--lI0) : 1099574344);
                mJ >>= (Math.incrementExact(mJ));
                mJ = (~ -2103354070L);
              }
            }
          }
        } else {
          mJ *= (- ((long) new Long(479832084L)));
          mJ %= (Long.MAX_VALUE);
          mD /= (--mD);
          if ((mI > ((mBX.x()) << mI))) {
            {
              long lJ0 = (mJ--);
              mI >>>= (mBX.x());
            }
            mF = (+ 505094603.0f);
            mD *= (((boolean) new Boolean((! false))) ? mD : 1808773781.0);
            mI *= (Integer.MIN_VALUE);
            for (int i1 = 576 - 1; i1 >= 0; i1--) {
              if (((boolean) new Boolean(false))) {
                mD += ((double)(float)(double) -1051436901.0);
              } else {
                mF -= ((float)(int)(float) (Float.min(mF, (mF--))));
              }
              for (int i2 = 0; i2 < 576; i2++) {
                mJ -= ((long) new Long(-1968644857L));
                mJ ^= (+ (mC.s()));
              }
            }
          } else {
            mF -= ((- mF) + -2145489966.0f);
          }
          mD -= (mD++);
          mD = (949112777.0 * 1209996119.0);
        }
        mZ &= (Boolean.logicalAnd(true, ((mZ) & (((boolean) new Boolean(true)) && true))));
      }
    }
    return ((float) 964977619L);
  }

  public static void main(String[] args) {
    System.out.println("Start....");
    Main t = new Main();
    float r = 1883600237.0f;
    try {
      r = t.testMethod();
    } catch (Exception e) {
      // Arithmetic, null pointer, index out of bounds, etc.
      System.out.println("An exception was caught.");
    }
    System.out.println("r  = " + r);
    System.out.println("mZ = " + t.mZ);
    System.out.println("mI = " + t.mI);
    System.out.println("mJ = " + t.mJ);
    System.out.println("mF = " + t.mF);
    System.out.println("mD = " + t.mD);
    System.out.println("Done....");
  }
}

