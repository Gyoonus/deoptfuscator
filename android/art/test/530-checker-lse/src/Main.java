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

class Circle {
  Circle(double radius) {
    this.radius = radius;
  }
  public double getRadius() {
    return radius;
  }
  public double getArea() {
    return radius * radius * Math.PI;
  }
  private double radius;
}

class TestClass {
  static {
    sTestClassObj = new TestClass(-1, -2);
  }
  TestClass() {
  }
  TestClass(int i, int j) {
    this.i = i;
    this.j = j;
  }
  int i;
  int j;
  volatile int k;
  TestClass next;
  String str;
  static int si;
  static TestClass sTestClassObj;
}

class SubTestClass extends TestClass {
  int k;
}

class TestClass2 {
  int i;
  int j;
}

class TestClass3 {
  float floatField = 8.0f;
  boolean test1 = true;
}

class Finalizable {
  static boolean sVisited = false;
  static final int VALUE1 = 0xbeef;
  static final int VALUE2 = 0xcafe;
  int i;

  protected void finalize() {
    if (i != VALUE1) {
      System.out.println("Where is the beef?");
    }
    sVisited = true;
  }
}

interface Filter {
  public boolean isValid(int i);
}

public class Main {

  /// CHECK-START: double Main.calcCircleArea(double) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: double Main.calcCircleArea(double) load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet

  static double calcCircleArea(double radius) {
    return new Circle(radius).getArea();
  }

  /// CHECK-START: int Main.test1(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test1(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Different fields shouldn't alias.
  static int test1(TestClass obj1, TestClass obj2) {
    obj1.i = 1;
    obj2.j = 2;
    return obj1.i + obj2.j;
  }

  /// CHECK-START: int Main.test2(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test2(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet

  // Redundant store of the same value.
  static int test2(TestClass obj) {
    obj.j = 1;
    obj.j = 1;
    return obj.j;
  }

  /// CHECK-START: int Main.test3(TestClass) load_store_elimination (before)
  /// CHECK: StaticFieldGet
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test3(TestClass) load_store_elimination (after)
  /// CHECK: StaticFieldGet
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK-NOT: StaticFieldGet

  // A new allocation (even non-singleton) shouldn't alias with pre-existing values.
  static int test3(TestClass obj) {
    TestClass obj1 = TestClass.sTestClassObj;
    TestClass obj2 = new TestClass();  // Cannot alias with obj or obj1 which pre-exist.
    obj.next = obj2;  // Make obj2 a non-singleton.
    // All stores below need to stay since obj/obj1/obj2 are not singletons.
    obj.i = 1;
    obj1.j = 2;
    // Following stores won't kill values of obj.i and obj1.j.
    obj2.i = 3;
    obj2.j = 4;
    return obj.i + obj1.j + obj2.i + obj2.j;
  }

  /// CHECK-START: int Main.test4(TestClass, boolean) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: Return
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.test4(TestClass, boolean) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK: Return
  /// CHECK: InstanceFieldSet

  // Set and merge the same value in two branches.
  static int test4(TestClass obj, boolean b) {
    if (b) {
      obj.i = 1;
    } else {
      obj.i = 1;
    }
    return obj.i;
  }

  /// CHECK-START: int Main.test5(TestClass, boolean) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: Return
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.test5(TestClass, boolean) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: Return
  /// CHECK: InstanceFieldSet

  // Set and merge different values in two branches.
  static int test5(TestClass obj, boolean b) {
    if (b) {
      obj.i = 1;
    } else {
      obj.i = 2;
    }
    return obj.i;
  }

  /// CHECK-START: int Main.test6(TestClass, TestClass, boolean) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test6(TestClass, TestClass, boolean) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Setting the same value doesn't clear the value for aliased locations.
  static int test6(TestClass obj1, TestClass obj2, boolean b) {
    obj1.i = 1;
    obj1.j = 2;
    if (b) {
      obj2.j = 2;
    }
    return obj1.j + obj2.j;
  }

  /// CHECK-START: int Main.test7(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test7(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  // Invocation should kill values in non-singleton heap locations.
  static int test7(TestClass obj) {
    obj.i = 1;
    System.out.print("");
    return obj.i;
  }

  /// CHECK-START: int Main.test8() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InvokeVirtual
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test8() load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK: InvokeVirtual
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Invocation should not kill values in singleton heap locations.
  static int test8() {
    TestClass obj = new TestClass();
    obj.i = 1;
    System.out.print("");
    return obj.i;
  }

  /// CHECK-START: int Main.test9(TestClass) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test9(TestClass) load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  // Invocation should kill values in non-singleton heap locations.
  static int test9(TestClass obj) {
    TestClass obj2 = new TestClass();
    obj2.i = 1;
    obj.next = obj2;
    System.out.print("");
    return obj2.i;
  }

  /// CHECK-START: int Main.test10(TestClass) load_store_elimination (before)
  /// CHECK: StaticFieldGet
  /// CHECK: InstanceFieldGet
  /// CHECK: StaticFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test10(TestClass) load_store_elimination (after)
  /// CHECK: StaticFieldGet
  /// CHECK: InstanceFieldGet
  /// CHECK: StaticFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Static fields shouldn't alias with instance fields.
  static int test10(TestClass obj) {
    TestClass.si += obj.i;
    return obj.i;
  }

  /// CHECK-START: int Main.test11(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test11(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Loop without heap writes.
  // obj.i is actually hoisted to the loop pre-header by licm already.
  static int test11(TestClass obj) {
    obj.i = 1;
    int sum = 0;
    for (int i = 0; i < 10; i++) {
      sum += obj.i;
    }
    return sum;
  }

  /// CHECK-START: int Main.test12(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.test12(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  // Loop with heap writes.
  static int test12(TestClass obj1, TestClass obj2) {
    obj1.i = 1;
    int sum = 0;
    for (int i = 0; i < 10; i++) {
      sum += obj1.i;
      obj2.i = sum;
    }
    return sum;
  }

  /// CHECK-START: int Main.test13(TestClass, TestClass2) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test13(TestClass, TestClass2) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Different classes shouldn't alias.
  static int test13(TestClass obj1, TestClass2 obj2) {
    obj1.i = 1;
    obj2.i = 2;
    return obj1.i + obj2.i;
  }

  /// CHECK-START: int Main.test14(TestClass, SubTestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test14(TestClass, SubTestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  // Subclass may alias with super class.
  static int test14(TestClass obj1, SubTestClass obj2) {
    obj1.i = 1;
    obj2.i = 2;
    return obj1.i;
  }

  /// CHECK-START: int Main.test15() load_store_elimination (before)
  /// CHECK: StaticFieldSet
  /// CHECK: StaticFieldSet
  /// CHECK: StaticFieldGet

  /// CHECK-START: int Main.test15() load_store_elimination (after)
  /// CHECK: <<Const2:i\d+>> IntConstant 2
  /// CHECK: StaticFieldSet
  /// CHECK-NOT: StaticFieldGet
  /// CHECK: Return [<<Const2>>]

  // Static field access from subclass's name.
  static int test15() {
    TestClass.si = 1;
    SubTestClass.si = 2;
    return TestClass.si;
  }

  /// CHECK-START: int Main.test16() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test16() load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet

  // Test inlined constructor.
  static int test16() {
    TestClass obj = new TestClass(1, 2);
    return obj.i + obj.j;
  }

  /// CHECK-START: int Main.test17() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test17() load_store_elimination (after)
  /// CHECK: <<Const0:i\d+>> IntConstant 0
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK: Return [<<Const0>>]

  // Test getting default value.
  static int test17() {
    TestClass obj = new TestClass();
    obj.j = 1;
    return obj.i;
  }

  /// CHECK-START: int Main.test18(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test18(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  // Volatile field load/store shouldn't be eliminated.
  static int test18(TestClass obj) {
    obj.k = 1;
    return obj.k;
  }

  /// CHECK-START: float Main.test19(float[], float[]) load_store_elimination (before)
  /// CHECK:     {{f\d+}} ArrayGet
  /// CHECK:     {{f\d+}} ArrayGet

  /// CHECK-START: float Main.test19(float[], float[]) load_store_elimination (after)
  /// CHECK:     {{f\d+}} ArrayGet
  /// CHECK-NOT: {{f\d+}} ArrayGet

  // I/F, J/D aliasing should not happen any more and LSE should eliminate the load.
  static float test19(float[] fa1, float[] fa2) {
    fa1[0] = fa2[0];
    return fa1[0];
  }

  /// CHECK-START: TestClass Main.test20() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet

  /// CHECK-START: TestClass Main.test20() load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK-NOT: InstanceFieldSet

  // Storing default heap value is redundant if the heap location has the
  // default heap value.
  static TestClass test20() {
    TestClass obj = new TestClass();
    obj.i = 0;
    return obj;
  }

  /// CHECK-START: void Main.test21(TestClass) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: void Main.test21(TestClass) load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  // Loop side effects can kill heap values, stores need to be kept in that case.
  static void test21(TestClass obj0) {
    TestClass obj = new TestClass();
    obj0.str = "abc";
    obj.str = "abc";
    for (int i = 0; i < 2; i++) {
      // Generate some loop side effect that writes into obj.
      obj.str = "def";
    }
    System.out.print(obj0.str.substring(0, 0) + obj.str.substring(0, 0));
  }

  /// CHECK-START: int Main.test22() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.test22() load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK-NOT: InstanceFieldGet

  // For a singleton, loop side effects can kill its field values only if:
  // (1) it dominiates the loop header, and
  // (2) its fields are stored into inside a loop.
  static int test22() {
    int sum = 0;
    TestClass obj1 = new TestClass();
    obj1.i = 2;    // This store can be eliminated since obj1 is never stored into inside a loop.
    for (int i = 0; i < 2; i++) {
      TestClass obj2 = new TestClass();
      obj2.i = 3;  // This store can be eliminated since the singleton is inside the loop.
      sum += obj2.i;
    }
    TestClass obj3 = new TestClass();
    obj3.i = 5;    // This store can be eliminated since the singleton is created after the loop.
    sum += obj1.i + obj3.i;
    return sum;
  }

  /// CHECK-START: int Main.test23(boolean) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: Return
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.test23(boolean) load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: Return
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  // Test store elimination on merging.
  static int test23(boolean b) {
    TestClass obj = new TestClass();
    obj.i = 3;      // This store can be eliminated since the value flows into each branch.
    if (b) {
      obj.i += 1;   // This store cannot be eliminated due to the merge later.
    } else {
      obj.i += 2;   // This store cannot be eliminated due to the merge later.
    }
    return obj.i;
  }

  /// CHECK-START: float Main.test24() load_store_elimination (before)
  /// CHECK-DAG:     <<True:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<Float8:f\d+>>   FloatConstant 8
  /// CHECK-DAG:     <<Float42:f\d+>>  FloatConstant 42
  /// CHECK-DAG:     <<Obj:l\d+>>      NewInstance
  /// CHECK-DAG:                       InstanceFieldSet [<<Obj>>,<<True>>]
  /// CHECK-DAG:                       InstanceFieldSet [<<Obj>>,<<Float8>>]
  /// CHECK-DAG:     <<GetTest:z\d+>>  InstanceFieldGet [<<Obj>>]
  /// CHECK-DAG:     <<GetField:f\d+>> InstanceFieldGet [<<Obj>>]
  /// CHECK-DAG:     <<Select:f\d+>>   Select [<<Float42>>,<<GetField>>,<<GetTest>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: float Main.test24() load_store_elimination (after)
  /// CHECK-DAG:     <<True:i\d+>>     IntConstant 1
  /// CHECK-DAG:     <<Float8:f\d+>>   FloatConstant 8
  /// CHECK-DAG:     <<Float42:f\d+>>  FloatConstant 42
  /// CHECK-DAG:     <<Select:f\d+>>   Select [<<Float42>>,<<Float8>>,<<True>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  static float test24() {
    float a = 42.0f;
    TestClass3 obj = new TestClass3();
    if (obj.test1) {
      a = obj.floatField;
    }
    return a;
  }

  /// CHECK-START: void Main.testFinalizable() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: void Main.testFinalizable() load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  // Allocations of finalizable objects cannot be eliminated.
  static void testFinalizable() {
    Finalizable finalizable = new Finalizable();
    finalizable.i = Finalizable.VALUE2;
    finalizable.i = Finalizable.VALUE1;
  }

  static java.lang.ref.WeakReference<Object> getWeakReference() {
    return new java.lang.ref.WeakReference<>(new Object());
  }

  static void testFinalizableByForcingGc() {
    testFinalizable();
    java.lang.ref.WeakReference<Object> reference = getWeakReference();

    Runtime runtime = Runtime.getRuntime();
    for (int i = 0; i < 20; ++i) {
      runtime.gc();
      System.runFinalization();
      try {
        Thread.sleep(1);
      } catch (InterruptedException e) {
        throw new AssertionError(e);
      }

      // Check to see if the weak reference has been garbage collected.
      if (reference.get() == null) {
        // A little bit more sleep time to make sure.
        try {
          Thread.sleep(100);
        } catch (InterruptedException e) {
          throw new AssertionError(e);
        }
        if (!Finalizable.sVisited) {
          System.out.println("finalize() not called.");
        }
        return;
      }
    }
    System.out.println("testFinalizableByForcingGc() failed to force gc.");
  }

  /// CHECK-START: int Main.$noinline$testHSelect(boolean) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: Select

  /// CHECK-START: int Main.$noinline$testHSelect(boolean) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: Select

  // Test that HSelect creates alias.
  static int $noinline$testHSelect(boolean b) {
    if (sFlag) {
      throw new Error();
    }
    TestClass obj = new TestClass();
    TestClass obj2 = null;
    obj.i = 0xdead;
    if (b) {
      obj2 = obj;
    }
    return obj2.i;
  }

  static int sumWithFilter(int[] array, Filter f) {
    int sum = 0;
    for (int i = 0; i < array.length; i++) {
      if (f.isValid(array[i])) {
        sum += array[i];
      }
    }
    return sum;
  }

  /// CHECK-START: int Main.sumWithinRange(int[], int, int) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.sumWithinRange(int[], int, int) load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet

  // A lambda-style allocation can be eliminated after inlining.
  static int sumWithinRange(int[] array, final int low, final int high) {
    Filter filter = new Filter() {
      public boolean isValid(int i) {
        return (i >= low) && (i <= high);
      }
    };
    return sumWithFilter(array, filter);
  }

  private static int mI = 0;
  private static float mF = 0f;

  /// CHECK-START: float Main.testAllocationEliminationWithLoops() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: NewInstance
  /// CHECK: NewInstance

  /// CHECK-START: float Main.testAllocationEliminationWithLoops() load_store_elimination (after)
  /// CHECK-NOT: NewInstance

  private static float testAllocationEliminationWithLoops() {
    for (int i0 = 0; i0 < 5; i0++) {
      for (int i1 = 0; i1 < 5; i1++) {
        for (int i2 = 0; i2 < 5; i2++) {
          int lI0 = ((int) new Integer(((int) new Integer(mI))));
          if (((boolean) new Boolean(false))) {
            for (int i3 = 576 - 1; i3 >= 0; i3--) {
              mF -= 976981405.0f;
            }
          }
        }
      }
    }
    return 1.0f;
  }

  /// CHECK-START: TestClass2 Main.testStoreStore() load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: TestClass2 Main.testStoreStore() load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  private static TestClass2 testStoreStore() {
    TestClass2 obj = new TestClass2();
    obj.i = 41;
    obj.j = 42;
    obj.i = 41;
    obj.j = 43;
    return obj;
  }

  /// CHECK-START: void Main.testStoreStore2(TestClass2) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: void Main.testStoreStore2(TestClass2) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  private static void testStoreStore2(TestClass2 obj) {
    obj.i = 41;
    obj.j = 42;
    obj.i = 43;
    obj.j = 44;
  }

  /// CHECK-START: void Main.testStoreStore3(TestClass2, boolean) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: void Main.testStoreStore3(TestClass2, boolean) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  private static void testStoreStore3(TestClass2 obj, boolean flag) {
    obj.i = 41;
    obj.j = 42;    // redundant since it's overwritten in both branches below.
    if (flag) {
      obj.j = 43;
    } else {
      obj.j = 44;
    }
  }

  /// CHECK-START: void Main.testStoreStore4() load_store_elimination (before)
  /// CHECK: StaticFieldSet
  /// CHECK: StaticFieldSet

  /// CHECK-START: void Main.testStoreStore4() load_store_elimination (after)
  /// CHECK: StaticFieldSet
  /// CHECK-NOT: StaticFieldSet

  private static void testStoreStore4() {
    TestClass.si = 61;
    TestClass.si = 62;
  }

  /// CHECK-START: int Main.testStoreStore5(TestClass2, TestClass2) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.testStoreStore5(TestClass2, TestClass2) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  private static int testStoreStore5(TestClass2 obj1, TestClass2 obj2) {
    obj1.i = 71;      // This store is needed since obj2.i may load from it.
    int i = obj2.i;
    obj1.i = 72;
    return i;
  }

  /// CHECK-START: int Main.testStoreStore6(TestClass2, TestClass2) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.testStoreStore6(TestClass2, TestClass2) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet

  private static int testStoreStore6(TestClass2 obj1, TestClass2 obj2) {
    obj1.i = 81;      // This store is not needed since obj2.j cannot load from it.
    int j = obj2.j;
    obj1.i = 82;
    return j;
  }

  /// CHECK-START: int Main.testNoSideEffects(int[]) load_store_elimination (before)
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testNoSideEffects(int[]) load_store_elimination (after)
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK-NOT: ArraySet
  /// CHECK-NOT: ArrayGet

  private static int testNoSideEffects(int[] array) {
    array[0] = 101;
    array[1] = 102;
    int bitCount = Integer.bitCount(0x3456);
    array[1] = 103;
    return array[0] + bitCount;
  }

  /// CHECK-START: void Main.testThrow(TestClass2, java.lang.Exception) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: Throw

  /// CHECK-START: void Main.testThrow(TestClass2, java.lang.Exception) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: Throw

  // Make sure throw keeps the store.
  private static void testThrow(TestClass2 obj, Exception e) throws Exception {
    obj.i = 55;
    throw e;
  }

  /// CHECK-START: int Main.testStoreStoreWithDeoptimize(int[]) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: Deoptimize
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testStoreStoreWithDeoptimize(int[]) load_store_elimination (after)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK: Deoptimize
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK-NOT: ArrayGet

  private static int testStoreStoreWithDeoptimize(int[] arr) {
    TestClass2 obj = new TestClass2();
    obj.i = 41;
    obj.j = 42;
    obj.i = 41;
    obj.j = 43;
    arr[0] = 1;  // One HDeoptimize here.
    arr[1] = 1;
    arr[2] = 1;
    arr[3] = 1;
    return arr[0] + arr[1] + arr[2] + arr[3];
  }

  /// CHECK-START: double Main.getCircleArea(double, boolean) load_store_elimination (before)
  /// CHECK: NewInstance

  /// CHECK-START: double Main.getCircleArea(double, boolean) load_store_elimination (after)
  /// CHECK-NOT: NewInstance

  private static double getCircleArea(double radius, boolean b) {
    double area = 0d;
    if (b) {
      area = new Circle(radius).getArea();
    }
    return area;
  }

  /// CHECK-START: double Main.testDeoptimize(int[], double[], double) load_store_elimination (before)
  /// CHECK: Deoptimize
  /// CHECK: NewInstance
  /// CHECK: Deoptimize
  /// CHECK: NewInstance

  /// CHECK-START: double Main.testDeoptimize(int[], double[], double) load_store_elimination (after)
  /// CHECK: Deoptimize
  /// CHECK: NewInstance
  /// CHECK: Deoptimize
  /// CHECK-NOT: NewInstance

  private static double testDeoptimize(int[] iarr, double[] darr, double radius) {
    iarr[0] = 1;  // One HDeoptimize here. Not triggered.
    iarr[1] = 1;
    Circle circle1 = new Circle(radius);
    iarr[2] = 1;
    darr[0] = circle1.getRadius();  // One HDeoptimize here, which holds circle1 live. Triggered.
    darr[1] = circle1.getRadius();
    darr[2] = circle1.getRadius();
    darr[3] = circle1.getRadius();
    return new Circle(Math.PI).getArea();
  }

  /// CHECK-START: int Main.testAllocationEliminationOfArray1() load_store_elimination (before)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testAllocationEliminationOfArray1() load_store_elimination (after)
  /// CHECK-NOT: NewArray
  /// CHECK-NOT: ArraySet
  /// CHECK-NOT: ArrayGet
  private static int testAllocationEliminationOfArray1() {
    int[] array = new int[4];
    array[2] = 4;
    array[3] = 7;
    return array[0] + array[1] + array[2] + array[3];
  }

  /// CHECK-START: int Main.testAllocationEliminationOfArray2() load_store_elimination (before)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testAllocationEliminationOfArray2() load_store_elimination (after)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet
  private static int testAllocationEliminationOfArray2() {
    // Cannot eliminate array allocation since array is accessed with non-constant
    // index (only 3 elements to prevent vectorization of the reduction).
    int[] array = new int[3];
    array[1] = 4;
    array[2] = 7;
    int sum = 0;
    for (int e : array) {
      sum += e;
    }
    return sum;
  }

  /// CHECK-START: int Main.testAllocationEliminationOfArray3(int) load_store_elimination (before)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testAllocationEliminationOfArray3(int) load_store_elimination (after)
  /// CHECK-NOT: NewArray
  /// CHECK-NOT: ArraySet
  /// CHECK-NOT: ArrayGet
  private static int testAllocationEliminationOfArray3(int i) {
    int[] array = new int[4];
    array[i] = 4;
    return array[i];
  }

  /// CHECK-START: int Main.testAllocationEliminationOfArray4(int) load_store_elimination (before)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testAllocationEliminationOfArray4(int) load_store_elimination (after)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArraySet
  /// CHECK: ArrayGet
  /// CHECK-NOT: ArrayGet
  private static int testAllocationEliminationOfArray4(int i) {
    // Cannot eliminate array allocation due to index aliasing between 1 and i.
    int[] array = new int[4];
    array[1] = 2;
    array[i] = 4;
    return array[1] + array[i];
  }

  /// CHECK-START: int Main.testAllocationEliminationOfArray5(int) load_store_elimination (before)
  /// CHECK: NewArray
  /// CHECK: ArraySet
  /// CHECK: ArrayGet

  /// CHECK-START: int Main.testAllocationEliminationOfArray5(int) load_store_elimination (after)
  /// CHECK: NewArray
  /// CHECK-NOT: ArraySet
  /// CHECK-NOT: ArrayGet
  private static int testAllocationEliminationOfArray5(int i) {
    // Cannot eliminate array allocation due to unknown i that may
    // cause NegativeArraySizeException.
    int[] array = new int[i];
    array[1] = 12;
    return array[1];
  }

  /// CHECK-START: int Main.testExitMerge(boolean) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: Return
  /// CHECK: InstanceFieldSet
  /// CHECK: Throw

  /// CHECK-START: int Main.testExitMerge(boolean) load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  /// CHECK: Return
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK: Throw
  private static int testExitMerge(boolean cond) {
    TestClass obj = new TestClass();
    if (cond) {
      obj.i = 1;
      return obj.i + 1;
    } else {
      obj.i = 2;
      throw new Error();
    }
  }

  /// CHECK-START: int Main.testExitMerge2(boolean) load_store_elimination (before)
  /// CHECK: NewInstance
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.testExitMerge2(boolean) load_store_elimination (after)
  /// CHECK-NOT: NewInstance
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet
  private static int testExitMerge2(boolean cond) {
    TestClass obj = new TestClass();
    int res;
    if (cond) {
      obj.i = 1;
      res = obj.i + 1;
    } else {
      obj.i = 2;
      res = obj.j + 2;
    }
    return res;
  }

  /// CHECK-START: void Main.testStoreSameValue() load_store_elimination (before)
  /// CHECK: NewArray
  /// CHECK: ArrayGet
  /// CHECK: ArraySet

  /// CHECK-START: void Main.testStoreSameValue() load_store_elimination (after)
  /// CHECK: NewArray
  /// CHECK-NOT: ArrayGet
  /// CHECK-NOT: ArraySet
  private static void testStoreSameValue() {
    Object[] array = new Object[2];
    sArray = array;
    Object obj = array[0];
    array[1] = obj;    // store the same value as the defaut value.
  }

  static Object[] sArray;

  static void assertIntEquals(int result, int expected) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  static void assertFloatEquals(float result, float expected) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  static void assertDoubleEquals(double result, double expected) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main(String[] args) {
    assertDoubleEquals(Math.PI * Math.PI * Math.PI, calcCircleArea(Math.PI));
    assertIntEquals(test1(new TestClass(), new TestClass()), 3);
    assertIntEquals(test2(new TestClass()), 1);
    TestClass obj1 = new TestClass();
    TestClass obj2 = new TestClass();
    obj1.next = obj2;
    assertIntEquals(test3(obj1), 10);
    assertIntEquals(test4(new TestClass(), true), 1);
    assertIntEquals(test4(new TestClass(), false), 1);
    assertIntEquals(test5(new TestClass(), true), 1);
    assertIntEquals(test5(new TestClass(), false), 2);
    assertIntEquals(test6(new TestClass(), new TestClass(), true), 4);
    assertIntEquals(test6(new TestClass(), new TestClass(), false), 2);
    assertIntEquals(test7(new TestClass()), 1);
    assertIntEquals(test8(), 1);
    obj1 = new TestClass();
    obj2 = new TestClass();
    obj1.next = obj2;
    assertIntEquals(test9(new TestClass()), 1);
    assertIntEquals(test10(new TestClass(3, 4)), 3);
    assertIntEquals(TestClass.si, 3);
    assertIntEquals(test11(new TestClass()), 10);
    assertIntEquals(test12(new TestClass(), new TestClass()), 10);
    assertIntEquals(test13(new TestClass(), new TestClass2()), 3);
    SubTestClass obj3 = new SubTestClass();
    assertIntEquals(test14(obj3, obj3), 2);
    assertIntEquals(test15(), 2);
    assertIntEquals(test16(), 3);
    assertIntEquals(test17(), 0);
    assertIntEquals(test18(new TestClass()), 1);
    float[] fa1 = { 0.8f };
    float[] fa2 = { 1.8f };
    assertFloatEquals(test19(fa1, fa2), 1.8f);
    assertFloatEquals(test20().i, 0);
    test21(new TestClass());
    assertIntEquals(test22(), 13);
    assertIntEquals(test23(true), 4);
    assertIntEquals(test23(false), 5);
    assertFloatEquals(test24(), 8.0f);
    testFinalizableByForcingGc();
    assertIntEquals($noinline$testHSelect(true), 0xdead);
    int[] array = {2, 5, 9, -1, -3, 10, 8, 4};
    assertIntEquals(sumWithinRange(array, 1, 5), 11);
    assertFloatEquals(testAllocationEliminationWithLoops(), 1.0f);
    assertFloatEquals(mF, 0f);
    assertDoubleEquals(Math.PI * Math.PI * Math.PI, getCircleArea(Math.PI, true));
    assertDoubleEquals(0d, getCircleArea(Math.PI, false));

    int[] iarray = {0, 0, 0};
    double[] darray = {0d, 0d, 0d};
    try {
      assertDoubleEquals(Math.PI * Math.PI * Math.PI, testDeoptimize(iarray, darray, Math.PI));
    } catch (Exception e) {
      System.out.println(e);
    }
    assertIntEquals(iarray[0], 1);
    assertIntEquals(iarray[1], 1);
    assertIntEquals(iarray[2], 1);
    assertDoubleEquals(darray[0], Math.PI);
    assertDoubleEquals(darray[1], Math.PI);
    assertDoubleEquals(darray[2], Math.PI);

    assertIntEquals(testAllocationEliminationOfArray1(), 11);
    assertIntEquals(testAllocationEliminationOfArray2(), 11);
    assertIntEquals(testAllocationEliminationOfArray3(2), 4);
    assertIntEquals(testAllocationEliminationOfArray4(2), 6);
    assertIntEquals(testAllocationEliminationOfArray5(2), 12);
    try {
      testAllocationEliminationOfArray5(-2);
    } catch (NegativeArraySizeException e) {
      System.out.println("Got NegativeArraySizeException.");
    }

    assertIntEquals(testStoreStore().i, 41);
    assertIntEquals(testStoreStore().j, 43);

    assertIntEquals(testExitMerge(true), 2);
    assertIntEquals(testExitMerge2(true), 2);
    assertIntEquals(testExitMerge2(false), 2);

    TestClass2 testclass2 = new TestClass2();
    testStoreStore2(testclass2);
    assertIntEquals(testclass2.i, 43);
    assertIntEquals(testclass2.j, 44);

    testStoreStore3(testclass2, true);
    assertIntEquals(testclass2.i, 41);
    assertIntEquals(testclass2.j, 43);
    testStoreStore3(testclass2, false);
    assertIntEquals(testclass2.i, 41);
    assertIntEquals(testclass2.j, 44);

    testStoreStore4();
    assertIntEquals(TestClass.si, 62);

    int ret = testStoreStore5(testclass2, testclass2);
    assertIntEquals(testclass2.i, 72);
    assertIntEquals(ret, 71);

    testclass2.j = 88;
    ret = testStoreStore6(testclass2, testclass2);
    assertIntEquals(testclass2.i, 82);
    assertIntEquals(ret, 88);

    ret = testNoSideEffects(iarray);
    assertIntEquals(iarray[0], 101);
    assertIntEquals(iarray[1], 103);
    assertIntEquals(ret, 108);

    try {
      testThrow(testclass2, new Exception());
    } catch (Exception e) {}
    assertIntEquals(testclass2.i, 55);

    assertIntEquals(testStoreStoreWithDeoptimize(new int[4]), 4);
  }

  static boolean sFlag;
}
