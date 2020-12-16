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

// TODO: Add more tests after we can inline functions with calls.

class ClassWithoutFinals {
  /// CHECK-START: void ClassWithoutFinals.<init>() inliner (after)
  /// CHECK-NOT: ConstructorFence
  public ClassWithoutFinals() {}
}

class ClassWithFinals {
  public final int x;
  public ClassWithFinals obj;
  public static boolean doThrow = false;

  public ClassWithFinals(boolean cond) {
    x = 1;
    throw new RuntimeException();
    // should not inline this constructor
  }

  /// CHECK-START: void ClassWithFinals.<init>() inliner (after)
  /// CHECK:      ConstructorFence
  /// CHECK-NOT:  ConstructorFence

  /*
   * Check that the correct assembly instructions are selected for a Store/Store fence.
   *
   * - ARM variants:   DMB ISHST (store-store fence for inner shareable domain)
   * - Intel variants: no-op (store-store does not need a fence).
   */

  /// CHECK-START-ARM64: void ClassWithFinals.<init>() disassembly (after)
  /// CHECK:      ConstructorFence
  /// CHECK-NEXT: dmb ishst

  /// CHECK-START-ARM: void ClassWithFinals.<init>() disassembly (after)
  /// CHECK:      ConstructorFence
  /// CHECK-NEXT: dmb ishst

  /// CHECK-START-X86_64: void ClassWithFinals.<init>() disassembly (after)
  /// CHECK:      ConstructorFence
  /// CHECK-NOT:  {{[slm]}}fence

  /// CHECK-START-X86: void ClassWithFinals.<init>() disassembly (after)
  /// CHECK:      ConstructorFence
  /// CHECK-NOT:  {{[slm]}}fence
  public ClassWithFinals() {
    // Exactly one constructor barrier.
    x = 0;
  }

  /// CHECK-START: void ClassWithFinals.<init>(int) inliner (after)
  /// CHECK: <<This:l\d+>>            ParameterValue
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<This>>]
  /// CHECK-NOT:                      ConstructorFence
  public ClassWithFinals(int x) {
    // This should have exactly three barriers:
    //   - one for the new-instance
    //   - one for the constructor
    //   - one for the `new` which should be inlined.
    obj = new ClassWithFinals();
    this.x = x;
  }
}

class InheritFromClassWithFinals extends ClassWithFinals {
  /// CHECK-START: void InheritFromClassWithFinals.<init>() inliner (after)
  /// CHECK: <<This:l\d+>>            ParameterValue
  /// CHECK:                          ConstructorFence [<<This>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void InheritFromClassWithFinals.<init>() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public InheritFromClassWithFinals() {
    // Should inline the super constructor.
    //
    // Exactly one constructor barrier here.
  }

  /// CHECK-START: void InheritFromClassWithFinals.<init>(boolean) inliner (after)
  /// CHECK:      InvokeStaticOrDirect

  /// CHECK-START: void InheritFromClassWithFinals.<init>(boolean) inliner (after)
  /// CHECK-NOT:  ConstructorFence
  public InheritFromClassWithFinals(boolean cond) {
    super(cond);
    // should not inline the super constructor
  }

  /// CHECK-START: void InheritFromClassWithFinals.<init>(int) inliner (after)
  /// CHECK: <<This:l\d+>>            ParameterValue
  /// CHECK-DAG: <<NewHere:l\d+>>     NewInstance klass:InheritFromClassWithFinals
  /// CHECK-DAG:                      ConstructorFence [<<This>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewHere>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewHere>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void InheritFromClassWithFinals.<init>(int) inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public InheritFromClassWithFinals(int unused) {
    // super();  // implicitly the first invoke in this constructor.
    //   Should inline the super constructor and insert a constructor fence there.

    // Should inline the new instance call (barrier); and add another one
    // because the superclass has finals.
    new InheritFromClassWithFinals();
  }
}

class HaveFinalsAndInheritFromClassWithFinals extends ClassWithFinals {
  final int y;

  /// CHECK-START: void HaveFinalsAndInheritFromClassWithFinals.<init>() inliner (after)
  /// CHECK: <<This:l\d+>>            ParameterValue
  /// CHECK:                          ConstructorFence [<<This>>]
  /// CHECK:                          ConstructorFence [<<This>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void HaveFinalsAndInheritFromClassWithFinals.<init>() inliner (after)
  /// CHECK-NOT: InvokeStaticOrDirect
  public HaveFinalsAndInheritFromClassWithFinals() {
    // Should inline the super constructor and keep the memory barrier.
    y = 0;
  }

  /// CHECK-START: void HaveFinalsAndInheritFromClassWithFinals.<init>(boolean) inliner (after)
  /// CHECK: <<This:l\d+>>            ParameterValue
  /// CHECK:                          InvokeStaticOrDirect
  /// CHECK:                          ConstructorFence [<<This>>]
  /// CHECK-NOT:                      ConstructorFence
  public HaveFinalsAndInheritFromClassWithFinals(boolean cond) {
    super(cond);
    // should not inline the super constructor
    y = 0;
  }

  /// CHECK-START: void HaveFinalsAndInheritFromClassWithFinals.<init>(int) inliner (after)
  /// CHECK: <<This:l\d+>>            ParameterValue
  /// CHECK-DAG: <<NewHF:l\d+>>       NewInstance klass:HaveFinalsAndInheritFromClassWithFinals
  /// CHECK-DAG: <<NewIF:l\d+>>       NewInstance klass:InheritFromClassWithFinals
  /// CHECK-DAG:                      ConstructorFence [<<This>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewHF>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewHF>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewHF>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewIF>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewIF>>]
  /// CHECK-DAG:                      ConstructorFence [<<This>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void HaveFinalsAndInheritFromClassWithFinals.<init>(int) inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public HaveFinalsAndInheritFromClassWithFinals(int unused) {
    // super()
    // -- Inlined super constructor, insert memory barrier here.
    y = 0;

    // Should inline new instance and keep both memory barriers.
    // One more memory barrier for new-instance.
    // (3 total for this new-instance #1)
    new HaveFinalsAndInheritFromClassWithFinals();
    // Should inline new instance and have exactly one barrier.
    // One more barrier for new-instance.
    // (2 total for this new-instance #2)
    new InheritFromClassWithFinals();

    // -- End of constructor, insert memory barrier here to freeze 'y'.
  }
}

public class Main {

  /// CHECK-START: ClassWithFinals Main.noInlineNoConstructorBarrier() inliner (after)
  /// CHECK:      InvokeStaticOrDirect

  /// CHECK-START: ClassWithFinals Main.noInlineNoConstructorBarrier() inliner (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  public static ClassWithFinals noInlineNoConstructorBarrier() {
    // Exactly one barrier for the new-instance.
    return new ClassWithFinals(false);
    // should not inline the constructor
  }

  /// CHECK-START: void Main.inlineNew() inliner (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void Main.inlineNew() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public static void inlineNew() {
    // Exactly 2 barriers. One for new-instance, one for constructor with finals.
    new ClassWithFinals();
  }

  /// CHECK-START: void Main.inlineNew1() inliner (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void Main.inlineNew1() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public static void inlineNew1() {
    new InheritFromClassWithFinals();
  }

  /// CHECK-START: void Main.inlineNew2() inliner (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void Main.inlineNew2() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public static void inlineNew2() {
    new HaveFinalsAndInheritFromClassWithFinals();
  }

  /// CHECK-START: void Main.inlineNew3() inliner (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void Main.inlineNew3() inliner (after)
  /// CHECK-NOT:  InvokeStaticOrDirect
  public static void inlineNew3() {
    new HaveFinalsAndInheritFromClassWithFinals();
    new HaveFinalsAndInheritFromClassWithFinals();
  }

  static int[] mCodePointsEmpty = new int[0];

  /// CHECK-START: void Main.testNewString() inliner (after)
  /// CHECK-NOT:  ConstructorFence
  /// CHECK:      InvokeStaticOrDirect method_load_kind:StringInit
  /// CHECK-NOT:  ConstructorFence
  /// CHECK-NOT:  InvokeStaticOrDirect
  public static void testNewString() {
    // Strings are special because of StringFactory hackeries.
    //
    // Assume they handle their own fencing internally in the StringFactory.
    int[] codePoints = null;
    String some_new_string = new String(mCodePointsEmpty, 0, 0);
  }

  public static void main(String[] args) {}
}
