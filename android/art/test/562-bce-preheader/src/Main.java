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

  /**
   * Method with an outer countable loop and an inner do-while loop.
   * Since all work is done in the header of the inner loop, any invariant hoisting
   * and deopting should be done in its proper loop preheader, not the true-block
   * of the newly generated taken-test after dynamic BCE.
   */
  public static int doit(int[][] x, int j) {
    float f = 0;
    int acc = 0;
    for (int i = 0; i < 2; i++) {
      // The full body of a do-while loop is the loop header.
      do {
        // Some "noise" to avoid hoisting the array reference
        // before the dynamic BCE phase runs.
        f++;
        // The invariant array reference with corresponding bounds check
        // is a candidate for hoisting when dynamic BCE runs. If it is
        // not moved to the proper loop preheader, the wrong values
        // cause the test to fail.
        acc += x[i][i];
      } while (++j < i);
    }
    return acc;
  }

  /**
   * Single countable loop with a clear header and a loop body. In this case,
   * after dynamic bce, some invariant hoisting and deopting must go to the
   * proper loop preheader and some must go to the true-block.
   */
  public static int foo(int[] x, int[] y, int n) {
    float f = 0;
    int acc = 0;
    int i = 0;
    while (true) {
      // This part is the loop header.
      // Some "noise" to avoid hoisting the array reference
      // before the dynamic BCE phase runs.
      f++;
      // The invariant array reference with corresponding bounds check
      // is a candidate for hoisting when dynamic BCE runs. If it is
      // not moved to the proper loop preheader, the wrong values
      // cause the test to fail.
      acc += y[0];
      if (++i > n)
        break;
      // From here on, this part is the loop body.
      // The unit-stride array reference is a candidate for dynamic BCE.
      // The deopting appears in the true-block.
      acc += x[i];
    }
    return acc;
  }

  /**
   * An artificial example with an inconsistent phi structure during
   * dynamic bce that is corrected afterwards. Note that only the last
   * assignment is really live, but the other statements set up an
   * interesting phi structure.
   */
  private static int doit(int[] z) {
    int a = 0;
    for (int i = 0; i < 10; ++i) {
      for (int j = i; j < 10; ++j) {
        a = z[i];
        for (int k = 0; k < 10; ++k) {
          a += z[k];
          a = z[i];
        }
      }
    }
    return a;
  }

  /**
   * Example shows that we can hoist ArrayGet to pre-header only if
   * its execution is guaranteed.
   */
  public static int hoistcheck(int[] c) {
    int i = 0, i2 = 0, i3 = 0, k = 0;
    int n = c.length;
    for (i = -100000000; i < 20; i += 10000000) {
      i3 = i;
      i2 = 0;
      while (i2++ < 1) {
        if (i3 >= 0 && i3 < n) {
          k += c[i3];
        }
      }
    }
    return k;
  }

  public static void main(String args[]) {
    int[][] x = new int[2][2];
    int y;

    x[0][0] = 1;
    x[1][1] = 2;

    expectEquals(8, doit(x, -6));
    expectEquals(7, doit(x, -5));
    expectEquals(6, doit(x, -4));
    expectEquals(5, doit(x, -3));
    expectEquals(4, doit(x, -2));
    expectEquals(3, doit(x, -1));
    expectEquals(3, doit(x,  0));
    expectEquals(3, doit(x,  1));
    expectEquals(3, doit(x, 22));

    int a[] = { 1, 2, 3, 5 };
    int b[] = { 7 };

    expectEquals(7,  foo(a, b, -1));
    expectEquals(7,  foo(a, b,  0));
    expectEquals(16, foo(a, b,  1));
    expectEquals(26, foo(a, b,  2));
    expectEquals(38, foo(a, b,  3));

    int[] z = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    expectEquals(10, doit(z));

    int c[] = { 1, 2, 3, 5 };
    expectEquals(1, hoistcheck(c));

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
