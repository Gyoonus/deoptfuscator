/*
 * Copyright (C) 2014 The Android Open Source Project
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

public class Main extends TestCase {
  public static void main(String[] args) {
    $opt$TestThisClassStaticField();
    $opt$TestOtherClassStaticField();
    $opt$TestAddThisClassStaticField();
    $opt$TestAddOtherClassStaticField();
    $opt$TestOtherClassWithClinitStaticField();
    $opt$TestAccess();
  }

  static int staticField = 42;

  static int getInt() {
    return 33;
  }

  static void $opt$TestThisClassStaticField() {
    assertEquals(42, staticField);
  }

  static void $opt$TestOtherClassStaticField() {
    assertEquals(41, Other.staticField);
  }

  static void $opt$TestAddThisClassStaticField() {
    int a = getInt();
    assertEquals(a + 42, a + staticField);
  }

  static void $opt$TestAddOtherClassStaticField() {
    int a = getInt();
    assertEquals(a + 41, a + Other.staticField);
  }

  static void $opt$TestOtherClassWithClinitStaticField() {
    assertEquals(40, OtherWithClinit.staticField);
  }

  static void $opt$TestAccess() {
    assertEquals(false, sZ);
    assertEquals(0, sB);
    assertEquals(0, sC);
    assertEquals(0, sI);
    assertEquals(0, sJ);
    assertEquals(0, sS);
    assertEquals(0.0f, sF);
    assertEquals(0.0, sD);
    assertNull(sObject);

    long longValue = -1122198787987987987L;
    Object o = new Object();
    sZ = true;
    sB = -2;
    sC = 'c';
    sI = 42;
    sJ = longValue;
    sS = 68;
    sObject = o;
    sF = 2.3f;
    sD = 5.3;

    assertEquals(true, sZ);
    assertEquals(-2, sB);
    assertEquals('c', sC);
    assertEquals(42, sI);
    assertEquals(longValue, sJ);
    assertEquals(68, sS);
    assertEquals(o, sObject);
    assertEquals(2.3f, sF);
    assertEquals(5.3, sD);
  }

  static boolean sZ;
  static byte sB;
  static char sC;
  static double sD;
  static float sF;
  static int sI;
  static long sJ;
  static short sS;
  static Object sObject;
}
