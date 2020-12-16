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

interface SuperInterface {
  void superInterfaceMethod();
}

interface OtherInterface extends SuperInterface {
}

interface Interface extends SuperInterface {
  void $noinline$f();
}

class Super implements Interface {
  public void superInterfaceMethod() {}
  public void $noinline$f() {
    throw new RuntimeException();
  }

  public int $inline$h(boolean cond) {
    Super obj = (cond ? this : null);
    return obj.hashCode();
  }
}

class SubclassA extends Super {
  public void $noinline$f() {
    throw new RuntimeException();
  }

  public String $noinline$h() {
    throw new RuntimeException();
  }

  void $noinline$g() {
    throw new RuntimeException();
  }
}

class SubclassC extends SubclassA {
}

class SubclassB extends Super {
  public void $noinline$f() {
    throw new RuntimeException();
  }

  void $noinline$g() {
    throw new RuntimeException();
  }
}

class Generic<A> {
  private A a = null;
  public A get() {
    return a;
  }
}

final class Final {}

final class FinalException extends Exception {}

public class Main {

  /// CHECK-START: void Main.testSimpleRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testSimpleRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testSimpleRemove() {
    Super s = new SubclassA();
    ((SubclassA)s).$noinline$g();
  }

  /// CHECK-START: void Main.testSimpleKeep(Super) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testSimpleKeep(Super) instruction_simplifier (after)
  /// CHECK:         CheckCast
  public void testSimpleKeep(Super s) {
    ((SubclassA)s).$noinline$f();
  }

  /// CHECK-START: java.lang.String Main.testClassRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: java.lang.String Main.testClassRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public String testClassRemove() {
    Object s = SubclassA.class;
    return ((Class<?>)s).getName();
  }

  /// CHECK-START: java.lang.String Main.testClassKeep() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: java.lang.String Main.testClassKeep() instruction_simplifier (after)
  /// CHECK:         CheckCast
  public String testClassKeep() {
    Object s = SubclassA.class;
    return ((SubclassA)s).$noinline$h();
  }

  /// CHECK-START: void Main.testIfRemove(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testIfRemove(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testIfRemove(int x) {
    Super s;
    if (x % 2 == 0) {
      s = new SubclassA();
    } else {
      s = new SubclassC();
    }
    ((SubclassA)s).$noinline$g();
  }

  /// CHECK-START: void Main.testIfKeep(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testIfKeep(int) instruction_simplifier (after)
  /// CHECK:         CheckCast
  public void testIfKeep(int x) {
    Super s;
    if (x % 2 == 0) {
      s = new SubclassA();
    } else {
      s = new SubclassB();
    }
    ((SubclassA)s).$noinline$g();
  }

  /// CHECK-START: void Main.testForRemove(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testForRemove(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testForRemove(int x) {
    Super s = new SubclassA();
    for (int i = 0 ; i < x; i++) {
      if (x % 2 == 0) {
        s = new SubclassC();
      }
    }
    ((SubclassA)s).$noinline$g();
  }

  /// CHECK-START: void Main.testForKeep(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testForKeep(int) instruction_simplifier (after)
  /// CHECK:         CheckCast
  public void testForKeep(int x) {
    Super s = new SubclassA();
    for (int i = 0 ; i < x; i++) {
      if (x % 2 == 0) {
        s = new SubclassC();
      }
    }
    ((SubclassC)s).$noinline$g();
  }

  /// CHECK-START: void Main.testPhiFromCall(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testPhiFromCall(int) instruction_simplifier (after)
  /// CHECK:         CheckCast
  public void testPhiFromCall(int i) {
    Object x;
    if (i % 2 == 0) {
      x = new SubclassC();
    } else {
      x = newObject();  // this one will have an unknown type.
    }
    ((SubclassC)x).$noinline$g();
  }

  /// CHECK-START: void Main.testInstanceOf(java.lang.Object) instruction_simplifier (before)
  /// CHECK:         CheckCast
  /// CHECK:         CheckCast
  /// CHECK-NOT:     CheckCast

  /// CHECK-START: void Main.testInstanceOf(java.lang.Object) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOf(Object o) {
    if (o instanceof SubclassC) {
      ((SubclassC)o).$noinline$g();
    }
    if (o instanceof SubclassB) {
      ((SubclassB)o).$noinline$g();
    }
  }

  public static boolean $inline$InstanceofSubclassB(Object o) { return o instanceof SubclassB; }
  public static boolean $inline$InstanceofSubclassC(Object o) { return o instanceof SubclassC; }

  /// CHECK-START: void Main.testInstanceOf_NotInlined(java.lang.Object) builder (after)
  /// CHECK-DAG:     <<Cst0:i\d+>> IntConstant 0
  /// CHECK-DAG:     <<Cst1:i\d+>> IntConstant 1
  /// CHECK-DAG:     <<IOf1:z\d+>> InstanceOf
  /// CHECK-DAG:                   NotEqual [<<IOf1>>,<<Cst1>>]
  /// CHECK-DAG:     <<IOf2:z\d+>> InstanceOf
  /// CHECK-DAG:                   Equal [<<IOf2>>,<<Cst0>>]

  /// CHECK-START: void Main.testInstanceOf_NotInlined(java.lang.Object) instruction_simplifier (before)
  /// CHECK:         CheckCast
  /// CHECK:         CheckCast
  /// CHECK-NOT:     CheckCast

  /// CHECK-START: void Main.testInstanceOf_NotInlined(java.lang.Object) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOf_NotInlined(Object o) {
    if ((o instanceof SubclassC) == true) {
      ((SubclassC)o).$noinline$g();
    }
    if ((o instanceof SubclassB) != false) {
      ((SubclassB)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testNotInstanceOf_NotInlined(java.lang.Object) builder (after)
  /// CHECK-DAG:     <<Cst0:i\d+>> IntConstant 0
  /// CHECK-DAG:     <<Cst1:i\d+>> IntConstant 1
  /// CHECK-DAG:     <<IOf1:z\d+>> InstanceOf
  /// CHECK-DAG:                   Equal [<<IOf1>>,<<Cst1>>]
  /// CHECK-DAG:     <<IOf2:z\d+>> InstanceOf
  /// CHECK-DAG:                   NotEqual [<<IOf2>>,<<Cst0>>]

  /// CHECK-START: void Main.testNotInstanceOf_NotInlined(java.lang.Object) instruction_simplifier (before)
  /// CHECK:         CheckCast
  /// CHECK:         CheckCast
  /// CHECK-NOT:     CheckCast

  /// CHECK-START: void Main.testNotInstanceOf_NotInlined(java.lang.Object) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testNotInstanceOf_NotInlined(Object o) {
    if ((o instanceof SubclassC) != true) {
      // Empty branch to flip the condition.
    } else {
      ((SubclassC)o).$noinline$g();
    }
    if ((o instanceof SubclassB) == false) {
      // Empty branch to flip the condition.
    } else {
      ((SubclassB)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testInstanceOf_Inlined(java.lang.Object) inliner (after)
  /// CHECK-DAG:     <<IOf:z\d+>>  InstanceOf
  /// CHECK-DAG:                   If [<<IOf>>]

  /// CHECK-START: void Main.testInstanceOf_Inlined(java.lang.Object) instruction_simplifier$after_inlining (before)
  /// CHECK:         CheckCast
  /// CHECK-NOT:     CheckCast

  /// CHECK-START: void Main.testInstanceOf_Inlined(java.lang.Object) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOf_Inlined(Object o) {
    if (!$inline$InstanceofSubclassC(o)) {
      // Empty branch to flip the condition.
    } else {
      ((SubclassC)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testInstanceOfKeep(java.lang.Object) instruction_simplifier (before)
  /// CHECK:         CheckCast
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfKeep(java.lang.Object) instruction_simplifier (after)
  /// CHECK:         CheckCast
  /// CHECK:         CheckCast
  public void testInstanceOfKeep(Object o) {
    if (o instanceof SubclassC) {
      ((SubclassB)o).$noinline$g();
    }
    if (o instanceof SubclassB) {
      ((SubclassA)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testInstanceOfNested(java.lang.Object) instruction_simplifier (before)
  /// CHECK:         CheckCast
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfNested(java.lang.Object) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfNested(Object o) {
    if (o instanceof SubclassC) {
      if (o instanceof SubclassB) {
        ((SubclassB)o).$noinline$g();
      } else {
        ((SubclassC)o).$noinline$g();
      }
    }
  }

  /// CHECK-START: void Main.testInstanceOfWithPhi(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfWithPhi(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfWithPhi(int i) {
    Object o;
    if (i == 0) {
      o = new SubclassA();
    } else {
      o = new SubclassB();
    }

    if (o instanceof SubclassB) {
      ((SubclassB)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testInstanceOfInFor(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfInFor(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfInFor(int n) {
    Object o = new SubclassA();
    for (int i = 0; i < n; i++) {
      if (i / 2 == 0) {
        o = new SubclassB();
      }
      if (o instanceof SubclassB) {
        ((SubclassB)o).$noinline$g();
      }
    }
  }

  /// CHECK-START: void Main.testInstanceOfSubclass() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfSubclass() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfSubclass() {
    Object o = new SubclassA();
    if (o instanceof Super) {
      ((SubclassA)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testInstanceOfWithPhiSubclass(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfWithPhiSubclass(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfWithPhiSubclass(int i) {
    Object o;
    if (i == 0) {
      o = new SubclassA();
    } else {
      o = new SubclassC();
    }

    if (o instanceof Super) {
      ((SubclassA)o).$noinline$g();
    }
  }

  /// CHECK-START: void Main.testInstanceOfWithPhiTop(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfWithPhiTop(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfWithPhiTop(int i) {
    Object o;
    if (i == 0) {
      o = new Object();
    } else {
      o = new SubclassC();
    }

    if (o instanceof Super) {
      ((Super)o).$noinline$f();
    }
  }

  /// CHECK-START: void Main.testInstanceOfSubclassInFor(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfSubclassInFor(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfSubclassInFor(int n) {
    Object o = new SubclassA();
    for (int i = 0; i < n; i++) {
      if (o instanceof Super) {
        ((SubclassA)o).$noinline$g();
      }
      if (i / 2 == 0) {
        o = new SubclassC();
      }
    }
  }

  /// CHECK-START: void Main.testInstanceOfTopInFor(int) instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceOfTopInFor(int) instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceOfTopInFor(int n) {
    Object o = new SubclassA();
    for (int i = 0; i < n; i++) {
      if (o instanceof Super) {
        ((Super)o).$noinline$f();
      }
      if (i / 2 == 0) {
        o = new Object();
      }
    }
  }

  public Object newObject() {
    try {
      return Object.class.newInstance();
    } catch (Exception e) {
      return null;
    }
  }

  public SubclassA a = new SubclassA();
  public static SubclassA b = new SubclassA();

  /// CHECK-START: void Main.testInstanceFieldGetSimpleRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInstanceFieldGetSimpleRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInstanceFieldGetSimpleRemove() {
    Main m = new Main();
    Super a = m.a;
    ((SubclassA)a).$noinline$g();
  }

  /// CHECK-START: void Main.testStaticFieldGetSimpleRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testStaticFieldGetSimpleRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testStaticFieldGetSimpleRemove() {
    Super b = Main.b;
    ((SubclassA)b).$noinline$g();
  }

  public SubclassA $noinline$getSubclass() { throw new RuntimeException(); }

  /// CHECK-START: void Main.testArraySimpleRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testArraySimpleRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testArraySimpleRemove() {
    Super[] b = new SubclassA[10];
    SubclassA[] c = (SubclassA[])b;
  }

  /// CHECK-START: void Main.testInvokeSimpleRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testInvokeSimpleRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testInvokeSimpleRemove() {
    Super b = $noinline$getSubclass();
    ((SubclassA)b).$noinline$g();
  }
  /// CHECK-START: void Main.testArrayGetSimpleRemove() instruction_simplifier (before)
  /// CHECK:         CheckCast

  /// CHECK-START: void Main.testArrayGetSimpleRemove() instruction_simplifier (after)
  /// CHECK-NOT:     CheckCast
  public void testArrayGetSimpleRemove() {
    Super[] a = new SubclassA[10];
    ((SubclassA)a[0]).$noinline$g();
  }

  /// CHECK-START: int Main.testLoadExceptionInCatchNonExact(int, int) builder (after)
  /// CHECK:         LoadException klass:java.lang.ArithmeticException can_be_null:false exact:false
  public int testLoadExceptionInCatchNonExact(int x, int y) {
    try {
      return x / y;
    } catch (ArithmeticException ex) {
      return ex.hashCode();
    }
  }

  /// CHECK-START: int Main.testLoadExceptionInCatchExact(int) builder (after)
  /// CHECK:         LoadException klass:FinalException can_be_null:false exact:true
  public int testLoadExceptionInCatchExact(int x) {
    try {
      if (x == 42) {
        throw new FinalException();
      } else {
        return x;
      }
    } catch (FinalException ex) {
      return ex.hashCode();
    }
  }

  /// CHECK-START: int Main.testLoadExceptionInCatchAll(int, int) builder (after)
  /// CHECK:         LoadException klass:java.lang.Throwable can_be_null:false exact:false
  public int testLoadExceptionInCatchAll(int x, int y) {
    try {
      x = x / y;
    } finally {
      return x;
    }
  }

  private Generic<SubclassC> genericC = new Generic<SubclassC>();
  private Generic<Final> genericFinal = new Generic<Final>();

  private SubclassC get() {
    return genericC.get();
  }

  private Final getFinal() {
    return genericFinal.get();
  }

  /// CHECK-START: SubclassC Main.inlineGenerics() builder (after)
  /// CHECK:      <<Invoke:l\d+>>    InvokeStaticOrDirect klass:SubclassC exact:false
  /// CHECK-NEXT:                    Return [<<Invoke>>]

  /// CHECK-START: SubclassC Main.inlineGenerics() inliner (after)
  /// CHECK:      <<BoundType:l\d+>> BoundType klass:SubclassC exact:false
  /// CHECK:                         Return [<<BoundType>>]
  private SubclassC inlineGenerics() {
    SubclassC c = get();
    return c;
  }

  /// CHECK-START: Final Main.inlineGenericsFinal() builder (after)
  /// CHECK:      <<Invoke:l\d+>>    InvokeStaticOrDirect klass:Final exact:true
  /// CHECK-NEXT:                    Return [<<Invoke>>]

  /// CHECK-START: Final Main.inlineGenericsFinal() inliner (after)
  /// CHECK:      <<BoundType:l\d+>> BoundType klass:Final exact:true
  /// CHECK:                         Return [<<BoundType>>]
  private Final inlineGenericsFinal() {
    Final f = getFinal();
    return f;
  }

  /// CHECK-START: void Main.boundOnlyOnceIfNotNull(java.lang.Object) inliner (after)
  /// CHECK:      BoundType
  /// CHECK-NOT:  BoundType
  private void boundOnlyOnceIfNotNull(Object o) {
    if (o != null) {
      o.toString();
    }
  }

  /// CHECK-START: void Main.boundOnlyOnceIfInstanceOf(java.lang.Object) inliner (after)
  /// CHECK:      BoundType
  /// CHECK-NOT:  BoundType
  private void boundOnlyOnceIfInstanceOf(Object o) {
    if (o instanceof Main) {
      o.toString();
    }
  }

  /// CHECK-START: Final Main.boundOnlyOnceCheckCast(Generic) inliner (after)
  /// CHECK:      BoundType
  /// CHECK-NOT:  BoundType
  private Final boundOnlyOnceCheckCast(Generic<Final> o) {
    Final f = o.get();
    return f;
  }

  private Super getSuper() {
    return new SubclassA();
  }

  /// CHECK-START: void Main.updateNodesInTheSameBlockAsPhi(boolean) builder (after)
  /// CHECK:      <<Phi:l\d+>> Phi klass:Super
  /// CHECK:                   NullCheck [<<Phi>>] klass:Super

  /// CHECK-START: void Main.updateNodesInTheSameBlockAsPhi(boolean) inliner (after)
  /// CHECK:      <<Phi:l\d+>> Phi klass:SubclassA
  /// CHECK:                   NullCheck [<<Phi>>] klass:SubclassA
  private void updateNodesInTheSameBlockAsPhi(boolean cond) {
    Super s = getSuper();
    if (cond) {
      s = new SubclassA();
    }
    s.$noinline$f();
  }

  /// CHECK-START: java.lang.String Main.checkcastPreserveNullCheck(java.lang.Object) inliner (after)
  /// CHECK:      <<This:l\d+>>     ParameterValue
  /// CHECK:      <<Param:l\d+>>    ParameterValue
  /// CHECK:      <<Clazz:l\d+>>    LoadClass
  /// CHECK:                        CheckCast [<<Param>>,<<Clazz>>]
  /// CHECK:                        BoundType [<<Param>>] can_be_null:true

  /// CHECK-START: java.lang.String Main.checkcastPreserveNullCheck(java.lang.Object) instruction_simplifier (after)
  /// CHECK:      <<This:l\d+>>     ParameterValue
  /// CHECK:      <<Param:l\d+>>    ParameterValue
  /// CHECK:      <<Clazz:l\d+>>    LoadClass
  /// CHECK:                        CheckCast [<<Param>>,<<Clazz>>]
  /// CHECK:      <<Bound:l\d+>>    BoundType [<<Param>>]
  /// CHECK:                        NullCheck [<<Bound>>]
  public String checkcastPreserveNullCheck(Object a) {
    return ((SubclassA)a).toString();
  }


  /// CHECK-START: void Main.argumentCheck(Super, double, SubclassA, Final) builder (after)
  /// CHECK:      ParameterValue klass:Main can_be_null:false exact:false
  /// CHECK:      ParameterValue klass:Super can_be_null:true exact:false
  /// CHECK:      ParameterValue
  /// CHECK:      ParameterValue klass:SubclassA can_be_null:true exact:false
  /// CHECK:      ParameterValue klass:Final can_be_null:true exact:true
  /// CHECK-NOT:  ParameterValue
  private void argumentCheck(Super s, double d, SubclassA a, Final f) {
  }

  private Main getNull() {
    return null;
  }

  private int mainField = 0;

  /// CHECK-START: SuperInterface Main.getWiderType(boolean, Interface, OtherInterface) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private SuperInterface getWiderType(boolean cond, Interface a, OtherInterface b) {
    return cond ? a : b;
  }

  /// CHECK-START: void Main.testInlinerWidensReturnType(boolean, Interface, OtherInterface) inliner (before)
  /// CHECK:      <<Invoke:l\d+>>    InvokeStaticOrDirect klass:SuperInterface
  /// CHECK:      <<NullCheck:l\d+>> NullCheck [<<Invoke>>] klass:SuperInterface exact:false
  /// CHECK:                         InvokeInterface [<<NullCheck>>]

  /// CHECK-START: void Main.testInlinerWidensReturnType(boolean, Interface, OtherInterface) inliner (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:      <<NullCheck:l\d+>> NullCheck [<<Phi>>] klass:SuperInterface exact:false
  /// CHECK:                         InvokeInterface [<<NullCheck>>]
  private void testInlinerWidensReturnType(boolean cond, Interface a, OtherInterface b) {
    getWiderType(cond, a, b).superInterfaceMethod();
  }

  /// CHECK-START: void Main.testInlinerReturnsNull() inliner (before)
  /// CHECK:      <<Int:i\d+>>       IntConstant 0
  /// CHECK:      <<Invoke:l\d+>>    InvokeStaticOrDirect klass:Main
  /// CHECK:      <<NullCheck:l\d+>> NullCheck [<<Invoke>>] klass:Main exact:false
  /// CHECK:                         InstanceFieldSet [<<NullCheck>>,<<Int>>]

  /// CHECK-START: void Main.testInlinerReturnsNull() inliner (after)
  /// CHECK:      <<Int:i\d+>>       IntConstant 0
  /// CHECK:      <<Null:l\d+>>      NullConstant klass:java.lang.Object
  /// CHECK:      <<NullCheck:l\d+>> NullCheck [<<Null>>] klass:Main exact:false
  /// CHECK:                         InstanceFieldSet [<<NullCheck>>,<<Int>>]
  private void testInlinerReturnsNull() {
    Main o = getNull();
    o.mainField = 0;
  }

  /// CHECK-START: void Main.testThisArgumentMoreSpecific(boolean) inliner (before)
  /// CHECK-DAG:     <<Arg:l\d+>>   NewInstance
  /// CHECK-DAG:                    InvokeVirtual [<<Arg>>,{{z\d+}}] method_name:Super.$inline$h

  /// CHECK-START: void Main.testThisArgumentMoreSpecific(boolean) inliner (after)
  /// CHECK-DAG:     <<Arg:l\d+>>   NewInstance
  /// CHECK-DAG:     <<Null:l\d+>>  NullConstant
  /// CHECK-DAG:     <<Phi:l\d+>>   Phi [<<Arg>>,<<Null>>] klass:SubclassA
  /// CHECK-DAG:     <<NCPhi:l\d+>> NullCheck [<<Phi>>]
  /// CHECK-DAG:                    InvokeVirtual [<<NCPhi>>] method_name:java.lang.Object.hashCode

  public void testThisArgumentMoreSpecific(boolean cond) {
    // Inlining method from Super will build it with `this` typed as Super.
    // Running RTP will sharpen it to SubclassA.
    SubclassA obj = new SubclassA();
    ((Super) obj).$inline$h(cond);
  }

  public static int $inline$hashCode(Super obj) {
    return obj.hashCode();
  }

  /// CHECK-START: void Main.testExplicitArgumentMoreSpecific(SubclassA) inliner (before)
  /// CHECK-DAG:     <<Arg:l\d+>>   ParameterValue klass:SubclassA
  // Note: The ArtMethod* (typed as int or long) is optional after sharpening.
  /// CHECK-DAG:                    InvokeStaticOrDirect [<<Arg>>{{(,[ij]\d+)?}}] method_name:Main.$inline$hashCode

  /// CHECK-START: void Main.testExplicitArgumentMoreSpecific(SubclassA) inliner (after)
  /// CHECK-DAG:     <<Arg:l\d+>>   ParameterValue klass:SubclassA
  /// CHECK-DAG:     <<NCArg:l\d+>> NullCheck [<<Arg>>] klass:SubclassA
  /// CHECK-DAG:                    InvokeVirtual [<<NCArg>>] method_name:java.lang.Object.hashCode

  public void testExplicitArgumentMoreSpecific(SubclassA obj) {
    // Inlining a method will build it with reference types from its signature,
    // here the callee graph is built with Super as the type of its only argument.
    // Running RTP after its ParameterValue instructions are replaced with actual
    // arguments will type the inner graph more precisely.
    $inline$hashCode(obj);
  }

  /// CHECK-START: void Main.testPhiHasOnlyNullInputs(boolean) inliner (before)
  /// CHECK:      <<Int:i\d+>>       IntConstant 0
  /// CHECK:      <<Phi:l\d+>>       Phi klass:Main exact:false
  /// CHECK:      <<NullCheck:l\d+>> NullCheck [<<Phi>>] klass:Main exact:false
  /// CHECK:                         InstanceFieldSet [<<NullCheck>>,<<Int>>]

  /// CHECK-START: void Main.testPhiHasOnlyNullInputs(boolean) inliner (after)
  /// CHECK:      <<Int:i\d+>>       IntConstant 0
  /// CHECK:      <<Null:l\d+>>      NullConstant klass:java.lang.Object
  /// CHECK:      <<Phi:l\d+>>       Phi [<<Null>>,<<Null>>] klass:java.lang.Object exact:false
  /// CHECK:      <<NullCheck:l\d+>> NullCheck [<<Phi>>] klass:java.lang.Object exact:false
  /// CHECK:                         InstanceFieldSet [<<NullCheck>>,<<Int>>]
  private void testPhiHasOnlyNullInputs(boolean cond) {
    Main o = cond ? null : getNull();
    o.mainField = 0;
    // getSuper() will force a type propagation after inlining
    // because returns a more precise type.
    getSuper();
  }

  /// CHECK-START: void Main.testLoopPhiWithNullFirstInput(boolean) builder (after)
  /// CHECK-DAG:  <<Null:l\d+>>      NullConstant
  /// CHECK-DAG:  <<Main:l\d+>>      NewInstance klass:Main exact:true
  /// CHECK-DAG:  <<LoopPhi:l\d+>>   Phi [<<Null>>,<<LoopPhi>>,<<Main>>] klass:Main exact:true
  private void testLoopPhiWithNullFirstInput(boolean cond) {
    Main a = null;
    while (a == null) {
      if (cond) {
        a = new Main();
      }
    }
  }

  /// CHECK-START: java.lang.Object[] Main.testInstructionsWithUntypedParent() builder (after)
  /// CHECK-DAG:  <<Null:l\d+>>      NullConstant
  /// CHECK-DAG:  <<LoopPhi:l\d+>>   Phi [<<Null>>,<<Phi:l\d+>>] klass:java.lang.Object[] exact:true
  /// CHECK-DAG:  <<Array:l\d+>>     NewArray klass:java.lang.Object[] exact:true
  /// CHECK-DAG:  <<Phi>>            Phi [<<Array>>,<<LoopPhi>>] klass:java.lang.Object[] exact:true
  /// CHECK-DAG:  <<NC:l\d+>>        NullCheck [<<LoopPhi>>] klass:java.lang.Object[] exact:true
  /// CHECK-DAG:                     ArrayGet [<<NC>>,{{i\d+}}] klass:java.lang.Object exact:false
  private Object[] testInstructionsWithUntypedParent() {
    Object[] array = null;
    boolean cond = true;
    for (int i = 0; i < 10; ++i) {
      if (cond) {
        array = new Object[10];
        array[0] = new Object();
        cond = false;
      } else {
        array[i] = array[0];
      }
    }
    return array;
  }

  public static void main(String[] args) {
  }
}
