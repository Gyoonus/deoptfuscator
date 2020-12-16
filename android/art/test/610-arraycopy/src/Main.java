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
  public static void main(String[] args) {
    Object[] a = new Object[5];
    for (int i = 0; i < 5; i++) {
      a[i] = new Integer(i);
    }
    $noinline$callArrayCopy(a, a);

    expectEquals(0, ((Integer)a[0]).intValue());
    expectEquals(0, ((Integer)a[1]).intValue());
    expectEquals(1, ((Integer)a[2]).intValue());
    expectEquals(2, ((Integer)a[3]).intValue());
    expectEquals(4, ((Integer)a[4]).intValue());
  }

  public static void expectEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void $noinline$callArrayCopy(Object[] a, Object[] b) {
    System.arraycopy(a, 0, b, 1, 3);
    if (doThrow) { throw new Error(); }
  }

  static boolean doThrow = false;
}
