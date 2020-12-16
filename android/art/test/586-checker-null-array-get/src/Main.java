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

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

class Test1 {
  int[] iarr;
}

class Test2 {
  float[] farr;
}

public class Main {
  public static Object[] getObjectArray() { return null; }
  public static long[] getLongArray() { return null; }
  public static Object getNull() { return null; }
  public static Test1 getNullTest1() { return null; }
  public static Test2 getNullTest2() { return null; }

  public static void $noinline$runSmaliTest(String name) throws Throwable {
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name);
      m.invoke(null);
    } catch (InvocationTargetException ex) {
      throw ex.getCause(); // re-raise expected exception.
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  public static void main(String[] args) {
    try {
      foo();
      throw new Error("Expected NullPointerException");
    } catch (NullPointerException e) {
      // Expected.
    }
    try {
      bar();
      throw new Error("Expected NullPointerException");
    } catch (NullPointerException e) {
      // Expected.
    }
    try {
      $noinline$runSmaliTest("bar");
      throw new Error("Expected NullPointerException");
    } catch (NullPointerException e) {
      // Expected.
    } catch (Throwable t) {
      throw new Error("Unexpected Throwable", t);
    }
    try {
      $noinline$runSmaliTest("bar2");
      throw new Error("Expected NullPointerException");
    } catch (NullPointerException e) {
      // Expected.
    } catch (Throwable t) {
      throw new Error("Unexpected Throwable", t);
    }

    try {
      test1();
      throw new Error("Expected NullPointerException");
    } catch (NullPointerException e) {
      // Expected.
    }
  }

  /// CHECK-START: void Main.foo() load_store_elimination (after)
  /// CHECK-DAG: <<Null:l\d+>>   NullConstant
  /// CHECK-DAG: <<Check:l\d+>>  NullCheck [<<Null>>]
  /// CHECK-DAG: <<Get1:j\d+>>   ArrayGet [<<Check>>,{{i\d+}}]
  /// CHECK-DAG: <<Get2:l\d+>>   ArrayGet [<<Check>>,{{i\d+}}]
  public static void foo() {
    longField = getLongArray()[0];
    objectField = getObjectArray()[0];
  }

  /// CHECK-START: void Main.bar() load_store_elimination (after)
  /// CHECK-DAG: <<Null:l\d+>>       NullConstant
  /// CHECK-DAG:                     BoundType [<<Null>>]
  /// CHECK-DAG: <<CheckL:l\d+>>     NullCheck
  /// CHECK-DAG: <<GetL0:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
  /// CHECK-DAG: <<GetL1:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
  /// CHECK-DAG: <<GetL2:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
  /// CHECK-DAG: <<GetL3:l\d+>>      ArrayGet [<<CheckL>>,{{i\d+}}]
  /// CHECK-DAG: <<CheckJ:l\d+>>     NullCheck [<<Null>>]
  /// CHECK-DAG: <<GetJ0:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
  /// CHECK-DAG: <<GetJ1:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
  /// CHECK-DAG: <<GetJ2:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
  /// CHECK-DAG: <<GetJ3:j\d+>>      ArrayGet [<<CheckJ>>,{{i\d+}}]
  public static void bar() {
    // We create multiple accesses that will lead the bounds check
    // elimination pass to add a HDeoptimize. Not having the bounds check helped
    // the load store elimination think it could merge two ArrayGet with different
    // types.
    String[] array = (String[])getNull();
    objectField = array[0];
    objectField = array[1];
    objectField = array[2];
    objectField = array[3];
    long[] longArray = getLongArray();
    longField = longArray[0];
    longField = longArray[1];
    longField = longArray[2];
    longField = longArray[3];
  }

  /// CHECK-START: float Main.test1() load_store_elimination (after)
  /// CHECK-DAG: <<Null:l\d+>>       NullConstant
  /// CHECK-DAG: <<Check1:l\d+>>     NullCheck [<<Null>>]
  /// CHECK-DAG: <<FieldGet1:l\d+>>  InstanceFieldGet [<<Check1>>] field_name:Test1.iarr
  /// CHECK-DAG: <<Check2:l\d+>>     NullCheck [<<FieldGet1>>]
  /// CHECK-DAG: <<ArrayGet1:i\d+>>  ArrayGet [<<Check2>>,{{i\d+}}]
  /// CHECK-DAG: <<ArrayGet2:f\d+>>  ArrayGet [<<Check2>>,{{i\d+}}]
  /// CHECK-DAG:                     Return [<<ArrayGet2>>]
  public static float test1() {
    Test1 test1 = getNullTest1();
    Test2 test2 = getNullTest2();
    int[] iarr = test1.iarr;
    float[] farr = test2.farr;
    iarr[0] = iarr[1];
    return farr[0];
  }

  public static long longField;
  public static Object objectField;
}
