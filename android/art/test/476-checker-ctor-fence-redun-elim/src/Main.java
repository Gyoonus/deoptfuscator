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

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

// Baseline class. This has no final fields, so there are no additional freezes
// in its constructor.
//
// The new-instance itself always has 1 freeze for the happens-before on the object header
// write (i.e. [obj.class = X] happens-before any access to obj).
//
// Total freezes for "new Base()": 1.
class Base {
  int w0;
  int w1;
  int w2;
  int w3;

  @Override
  public String toString() {
    return getClass().getName() + "(" + baseString() + ")";
  }

  protected String baseString() {
    return String.format("w0: %d, w1: %d, w2: %d, w3: %d", w0, w1, w2, w3);
  }
}

// This has a final field in its constructor, so there must be a field freeze
// at the end of <init>.
//
// Total freezes for "new OneFinal()": 2.
class OneFinal extends Base {
  final int x;
  OneFinal(int x) {
    this.x = x;
  }

  @Override
  protected String baseString() {
    return String.format("%s, x: %d", super.baseString(), x);
  }
}

class Assert {
  public static void stringEquals(String expected, Object actual) {
    stringEquals$noinline$(expected, actual);
  }

  // Forbid compiler from inlining this to avoid overly clever optimizations.
  private static void stringEquals$noinline$(String expected, Object actual) {
    String actualStr = Main.valueToString(actual);
    if (!expected.equals(actualStr)) {
      throw new AssertionError("Expected: " + expected + ", actual: " + actualStr);
    }
  }
}

interface Test {
  public void exercise();
  public void check();
}

class TestOneFinal implements Test {
  // Initialize at least once before actual test.
  public static Object external;

  /// CHECK-START: void TestOneFinal.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]

  /// CHECK-START: void TestOneFinal.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  @Override
  public void exercise() {
      Base b = new OneFinal(1);
      // 1 store, 2 freezes.

      // Stores to 'b' do not escape b.
      b.w0 = 1;
      b.w1 = 2;
      b.w2 = 3;

      // Publish the result to a global so that it is not LSE-eliminated.
      external = b;
  }

  @Override
  public void check() {
    Assert.stringEquals("OneFinal(w0: 1, w1: 2, w2: 3, w3: 0, x: 1)", external);
  }
}

// This has a final field in its constructor, so there must be a field freeze
// at the end of <init>. The previous base class's freezes accumulate on top
// of this one.
//
// Total freezes for "new TwoFinal()": 3.
class TwoFinal extends OneFinal {
  final int y;
  TwoFinal(int x, int y) {
    super(x);
    this.y = y;
  }

  @Override
  protected String baseString() {
    return String.format("%s, y: %d", super.baseString(), y);
  }
}

// This has a final field in its constructor, so there must be a field freeze
// at the end of <init>. The previous base class's freezes accumulate on top
// of this one.
//
// Total freezes for "new ThreeFinal()": 4.
class ThreeFinal extends TwoFinal {
  final int z;
  ThreeFinal(int x, int y, int z) {
    super(x, y);
    this.z = z;
  }

  @Override
  protected String baseString() {
    return String.format("%s, z: %d", super.baseString(), z);
  }
}

class TestThreeFinal implements Test {
  // Initialize at least once before actual test.
  public static Object external;

  /// CHECK-START: void TestThreeFinal.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]

  /// CHECK-START: void TestThreeFinal.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  @Override
  public void exercise() {
    Base b = new ThreeFinal(1, 1, 2);
    // 3 store, 4 freezes.

    // Stores to 'b' do not escape b.
    b.w0 = 3;

    // Publish the result to a global so that it is not LSE-eliminated.
    external = b;
  }

  @Override
  public void check() {
    Assert.stringEquals("ThreeFinal(w0: 3, w1: 0, w2: 0, w3: 0, x: 1, y: 1, z: 2)", external);
  }
}

// Ensure "freezes" between multiple new-instances are optimized out.
class TestMultiAlloc implements Test {
  public static Object external;
  public static Object external2;

  /// CHECK-START: void TestMultiAlloc.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]

  /// CHECK-START: void TestMultiAlloc.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]
  @Override
  public void exercise() {
    // 1 freeze
    Base b = new Base();
    // 1 freeze
    Base b2 = new Base();

    // Merge 2 freezes above into 1 constructor fence.
    external = b;
    external2 = b2;
  }

  @Override
  public void check() {
    Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external);
    Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external2);
  }
}

// Ensure "freezes" between multiple new-instances are optimized out.
class TestThreeFinalTwice implements Test {
  // Initialize at least once before actual test.
  public static Object external;
  public static Object external2;

  /// CHECK-START: void TestThreeFinalTwice.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]

  /// CHECK-START: void TestThreeFinalTwice.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]
  @Override
  public void exercise() {
    Base b = new ThreeFinal(1, 1, 2);
    // 3 store, 4 freezes.

    // Stores to 'b' do not escape b.
    b.w0 = 3;

    Base b2 = new ThreeFinal(4, 5, 6);
    // 3 store, 4 freezes.

    // Stores to 'b2' do not escape b2.
    b2.w0 = 7;

    // Publish the result to a global so that it is not LSE-eliminated.
    // Publishing is done at the end to give freezes above a chance to merge.
    external = b;
    external2 = b2;
  }

  @Override
  public void check() {
    Assert.stringEquals("ThreeFinal(w0: 3, w1: 0, w2: 0, w3: 0, x: 1, y: 1, z: 2)", external);
    Assert.stringEquals("ThreeFinal(w0: 7, w1: 0, w2: 0, w3: 0, x: 4, y: 5, z: 6)", external2);
  }
}

class TestNonEscaping {
  // Prevent constant folding.
  static boolean test;

  static Object external;
  static Object external2;
  static Object external3;
  static Object external4;

  static class Invoke implements Test {
    /// CHECK-START: void TestNonEscaping$Invoke.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK:                          InvokeStaticOrDirect
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestNonEscaping$Invoke.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          InvokeStaticOrDirect
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
    /// CHECK-NOT:                      ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      // b cannot possibly escape into this invoke because it hasn't escaped onto the heap earlier,
      // and the invoke doesn't take it as a parameter.
      noEscape$noinline$();

      // Remove the Constructor Fence for b, merging into the fence for b2.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external = b;
      external2 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external2);
    }
  }

  public static int[] array = new int[1];
  static Base base = new Base();

  static class Store implements Test {
    /// CHECK-START: void TestNonEscaping$Store.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                      ArraySet
    /// CHECK-DAG:                      StaticFieldSet
    /// CHECK-DAG:                      InstanceFieldSet
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestNonEscaping$Store.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
    /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
    /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
    /// CHECK-NOT:                        ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      // Stores of inputs other than the fence target do not publish 'b'.
      array[0] = b.w0;  // aput
      external = array; // sput
      base.w0 = b.w0;   // iput

      // Remove the Constructor Fence for b, merging into the fence for b2.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external3 = b;
      external4 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals("[0]", array);
      Assert.stringEquals("[0]", external);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", base);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external4);
    }
  }

  private static void noEscape$noinline$() {
  }
}

class TestDontOptimizeAcrossBlocks implements Test {
  // Prevent constant folding.
  static boolean test;

  static Object external;
  static Object external3;

  /// CHECK-START: void TestDontOptimizeAcrossBlocks.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]

  /// CHECK-START: void TestDontOptimizeAcrossBlocks.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]
  @Override
  public void exercise() {
    Base b = new Base();

    // Do not move constructor fence across this block, even though 'b' is not published yet.
    if (test) {
      external = null;
    }

    Base b2 = new Base();
    external = b2;
    external3 = b;
  }

  @Override
  public void check() {
    Assert.stringEquals("false", test);
    Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external);
    Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
  }
}

class TestDontOptimizeAcrossEscape {
  // Prevent constant folding.
  static boolean test;

  static Object external;
  static Object external2;
  static Object external3;
  static Object external4;

  static class Invoke implements Test {
    /// CHECK-START: void TestDontOptimizeAcrossEscape$Invoke.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK:                          InvokeStaticOrDirect
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$Invoke.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK:                          InvokeStaticOrDirect
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();
      // Do not optimize across invokes into which the fence target escapes.
      invoke$noinline$(b);

      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external = b;
      external2 = b2;
    }

    private static void invoke$noinline$(Object b) {
      // Even though 'b' does not escape this method, we conservatively assume all parameters
      // of an invoke escape.
    }

    @Override
    public void check() {
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external2);
    }
  }

  public static Object[] array = new Object[3];
  static Base base = new Base();

  static class InstanceEscaper {
    public Object holder;

    @Override
    public String toString() {
      return getClass().getName() + "(" + baseString() + ")";
    }

    protected String baseString() {
      return String.format("holder: %s", Main.valueToString(holder));
    }
  }

  static InstanceEscaper instanceEscaper = new InstanceEscaper();

  static class StoreIput implements Test {
    /// CHECK-START: void TestDontOptimizeAcrossEscape$StoreIput.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                      InstanceFieldSet
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$StoreIput.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
    /// CHECK:                            ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
    /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                        ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      // A store of 'b' into another instance will publish 'b'.
      instanceEscaper.holder = b;

      // Do not remove any constructor fences above.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external3 = b;
      external4 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals(
          "TestDontOptimizeAcrossEscape$InstanceEscaper(holder: Base(w0: 0, w1: 0, w2: 0, w3: 0))",
          instanceEscaper);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external4);
    }
  }

  static class StoreAput implements Test {
    /// CHECK-START: void TestDontOptimizeAcrossEscape$StoreAput.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                      ArraySet
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$StoreAput.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
    /// CHECK:                            ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
    /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                        ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      // A store of 'b' into another array will publish 'b'.
      array[0] = b;  // aput

      // Do not remove any constructor fences above.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external3 = b;
      external4 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals("[Base(w0: 0, w1: 0, w2: 0, w3: 0),<null>,<null>]", array);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external4);
    }
  }

  static class StoreSput implements Test {
    /// CHECK-START: void TestDontOptimizeAcrossEscape$StoreSput.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                      StaticFieldSet
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$StoreSput.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
    /// CHECK:                            ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
    /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                        ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      // A store of 'b' into a static will publish 'b'.
      external = b;

      // Do not remove any constructor fences above.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external3 = b;
      external4 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external4);
    }
  }

  static class Deopt implements Test {
    /// CHECK-START: void TestDontOptimizeAcrossEscape$Deopt.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                      Deoptimize
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$Deopt.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
    /// CHECK:                            ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
    /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                        ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      // An array access generates a Deopt to avoid doing bounds check.
      array[0] = external;  // aput
      array[1] = external;  // aput
      array[2] = external;  // aput

      // Do not remove any constructor fences above.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external3 = b;
      external4 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals("[Base(w0: 0, w1: 0, w2: 0, w3: 0),"
              + "Base(w0: 0, w1: 0, w2: 0, w3: 0),"
              + "Base(w0: 0, w1: 0, w2: 0, w3: 0)]",
          array);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external4);
    }
  }

  static class Select implements Test {
    /// CHECK-START: void TestDontOptimizeAcrossEscape$Select.exercise() constructor_fence_redundancy_elimination (before)
    /// CHECK: <<NewInstance:l\d+>>     NewInstance
    /// CHECK:                          ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                      Select
    /// CHECK: <<NewInstance2:l\d+>>    NewInstance
    /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                      ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$Select.exercise() constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
    /// CHECK:                            ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
    /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
    /// CHECK-NOT:                        ConstructorFence
    @Override
    public void exercise() {
      Base b = new Base();

      boolean localTest = test;
      Object localExternal = external3;

      // Selecting 'b' creates an alias, which we conservatively assume escapes immediately.
      external = localTest ? b : localExternal;

      // Do not remove any constructor fences above.
      Base b2 = new Base();

      // Do not LSE-eliminate b,b2
      external3 = b;
      external4 = b2;
    }

    @Override
    public void check() {
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external3);
      Assert.stringEquals("Base(w0: 0, w1: 0, w2: 0, w3: 0)", external4);
    }
  }

  static class MakeBoundTypeTest implements Test {
    public static Object makeBoundType;
    public static Object makeBoundTypeSub;

    @Override
    public void exercise() {
      // Note: MakeBoundType is special and we have to call the constructor directly
      // to prevent inlining it.
      try {
        makeBoundType = exerciseNewInstance(MakeBoundType.class, 123);
        makeBoundTypeSub = exerciseNewInstance(MakeBoundTypeSub.class, 123);
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    }

    @Override
    public void check() {
      Assert.stringEquals(
          "TestDontOptimizeAcrossEscape$MakeBoundTypeTest$MakeBoundType(abcdefgh: 123, x: 2)",
          makeBoundType);
      Assert.stringEquals(
          "TestDontOptimizeAcrossEscape$MakeBoundTypeTest$MakeBoundTypeSub(abcdefgh: 123, x: 1)",
          makeBoundTypeSub);
    }

    // Make a new instance of 'klass'.
    private static <T> T exerciseNewInstance(Class<T> klass, int params) throws Exception {
      return klass.cast(klass.getDeclaredConstructor(int.class).newInstance(params));
    }

    /// CHECK-START: void TestDontOptimizeAcrossEscape$MakeBoundTypeTest$MakeBoundType.<init>(int) constructor_fence_redundancy_elimination (before)
    /// CHECK-DAG: <<This:l\d+>>         ParameterValue
    /// CHECK-DAG: <<NewInstance:l\d+>>  NewInstance
    /// CHECK:                           ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                       BoundType
    /// CHECK-DAG:                       ConstructorFence [<<This>>]
    /// CHECK-NOT:                       ConstructorFence

    /// CHECK-START: void TestDontOptimizeAcrossEscape$MakeBoundTypeTest$MakeBoundType.<init>(int) constructor_fence_redundancy_elimination (after)
    /// CHECK-DAG: <<This:l\d+>>         ParameterValue
    /// CHECK-DAG: <<NewInstance:l\d+>>  NewInstance
    /// CHECK:                           ConstructorFence [<<NewInstance>>]
    /// CHECK-DAG:                       BoundType
    /// CHECK-DAG:                       ConstructorFence [<<This>>]
    /// CHECK-NOT:                       ConstructorFence
    static class MakeBoundType {
      final int abcdefgh;
      int x;

      MakeBoundType(int param) {
        abcdefgh = param;

        Base b = new Base();
        // constructor-fence(b)

        if (this instanceof MakeBoundTypeSub) {
          // Create a "BoundType(this)" which prevents
          // a merged constructor-fence(this, b)
          x = 1;
        } else {
          x = 2;
        }

        // publish(b).
        external = b;

        // constructor-fence(this)
      }

      @Override
      public String toString() {
        return getClass().getName() + "(" + baseString() + ")";
      }

      protected String baseString() {
        return String.format("abcdefgh: %d, x: %d", abcdefgh, x);
      }
    }

    static class MakeBoundTypeSub extends MakeBoundType {
      MakeBoundTypeSub(int xyz) {
        super(xyz);
      }
    }
  }
}

public class Main {
  public static void main(String[] args) throws Exception {
    // Ensure that all of this code does not get optimized out into a no-op
    // by actually running the code with reflection, then validating
    // the result by asserting it against a string.
    Class<? extends Test>[] testClasses = new Class[] {
      TestOneFinal.class,
      TestThreeFinal.class,
      TestMultiAlloc.class,
      TestThreeFinalTwice.class,
      TestNonEscaping.Invoke.class,
      TestNonEscaping.Store.class,
      TestDontOptimizeAcrossBlocks.class,
      TestDontOptimizeAcrossEscape.Invoke.class,
      TestDontOptimizeAcrossEscape.StoreIput.class,
      TestDontOptimizeAcrossEscape.StoreAput.class,
      TestDontOptimizeAcrossEscape.StoreSput.class,
      TestDontOptimizeAcrossEscape.Deopt.class,
      TestDontOptimizeAcrossEscape.Select.class,
      TestDontOptimizeAcrossEscape.MakeBoundTypeTest.class,
    };

    for (Class<? extends Test> klass : testClasses) {
      exerciseTestClass(klass);
    }
  }

  /**
   * Invoke Test#exercise(), then Test#check().
   * @throws AssertionError if test fails.
   */
  private static void exerciseTestClass(Class<? extends Test> klass) throws Exception {
    Test instance = klass.cast(klass.getDeclaredConstructor().newInstance());

    // Use reflection as a best-effort to avoid compiler optimizations (e.g. inlining).
    instance.getClass().getDeclaredMethod("exercise").invoke(instance);
    instance.getClass().getDeclaredMethod("check").invoke(instance);
  }

  // Print an object, with special handling for array and null.
  public static String valueToString(Object val) {
    if (val == null) {
      return "<null>";
    }
    if (val.getClass().isArray()) {
      String fmt = "[";
      int length = Array.getLength(val);
      for (int i = 0; i < length; ++i) {
        Object arrayElement = Array.get(val, i);
        fmt += valueToString(arrayElement);

        if (i != length - 1) {
          fmt += ",";
        }
      }
      fmt += "]";

      return fmt;
    }

    return val.toString();
  }
}
