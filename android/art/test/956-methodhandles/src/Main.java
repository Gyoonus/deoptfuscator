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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleInfo;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import other.Chatty;

public class Main {

  public static class A {
    public A() {}

    public void foo() {
      System.out.println("foo_A");
    }

    public static final Lookup lookup = MethodHandles.lookup();
  }

  public static class B extends A {
    public void foo() {
      System.out.println("foo_B");
    }

    public static final Lookup lookup = MethodHandles.lookup();
  }

  public static class C extends B {
    public static final Lookup lookup = MethodHandles.lookup();
  }

  public static class D {
    private final void privateRyan() {
      System.out.println("privateRyan_D");
    }

    public static final Lookup lookup = MethodHandles.lookup();
  }

  public static class E extends D {
    public static final Lookup lookup = MethodHandles.lookup();
  }

  private interface F {
    public default void sayHi() {
      System.out.println("F.sayHi()");
    }
  }

  public static class G implements F {
    public void sayHi() {
      System.out.println("G.sayHi()");
    }
    public MethodHandles.Lookup getLookup() {
      return MethodHandles.lookup();
    }
  }

  public static class H implements Chatty {
    public void chatter() {
      System.out.println("H.chatter()");
    }
    public MethodHandles.Lookup getLookup() {
      return MethodHandles.lookup();
    }
  }

  public static void main(String[] args) throws Throwable {
    testfindSpecial_invokeSuperBehaviour();
    testfindSpecial_invokeDirectBehaviour();
    testExceptionDetailMessages();
    testfindVirtual();
    testfindStatic();
    testUnreflects();
    testAsType();
    testConstructors();
    testStringConstructors();
    testReturnValueConversions();
    testVariableArity();
    testVariableArity_MethodHandles_bind();
    testRevealDirect();
    testReflectiveCalls();
  }

  public static void testfindSpecial_invokeSuperBehaviour() throws Throwable {
    // This is equivalent to an invoke-super instruction where the referrer
    // is B.class.
    MethodHandle mh1 = B.lookup.findSpecial(A.class /* refC */, "foo",
        MethodType.methodType(void.class), B.class /* specialCaller */);

    A aInstance = new A();
    B bInstance = new B();
    C cInstance = new C();

    // This should be as if an invoke-super was called from one of B's methods.
    mh1.invokeExact(bInstance);
    mh1.invoke(bInstance);

    // This should not work. The receiver type in the handle will be suitably
    // restricted to B and subclasses.
    try {
      mh1.invoke(aInstance);
      System.out.println("mh1.invoke(aInstance) should not succeeed");
    } catch (ClassCastException expected) {
    }

    try {
      mh1.invokeExact(aInstance);
      System.out.println("mh1.invoke(aInstance) should not succeeed");
    } catch (WrongMethodTypeException expected) {
    }

    // This should *still* be as if an invoke-super was called from one of C's
    // methods, despite the fact that we're operating on a C.
    mh1.invoke(cInstance);

    // Now that C is the special caller, the next invoke will call B.foo.
    MethodHandle mh2 = C.lookup.findSpecial(A.class /* refC */, "foo",
        MethodType.methodType(void.class), C.class /* specialCaller */);
    mh2.invokeExact(cInstance);

    // Shouldn't allow invoke-super semantics from an unrelated special caller.
    try {
      C.lookup.findSpecial(A.class, "foo",
        MethodType.methodType(void.class), D.class /* specialCaller */);
      System.out.println("findSpecial(A.class, foo, .. D.class) unexpectedly succeeded.");
    } catch (IllegalAccessException expected) {
    }

    // Check return type matches for find.
    try {
      B.lookup.findSpecial(A.class /* refC */, "foo",
                           MethodType.methodType(int.class), B.class /* specialCaller */);
      fail();
    } catch (NoSuchMethodException e) {}
    // Check constructors
    try {
      B.lookup.findSpecial(A.class /* refC */, "<init>",
                           MethodType.methodType(void.class), B.class /* specialCaller */);
      fail();
    } catch (NoSuchMethodException e) {}
  }

  public static void testfindSpecial_invokeDirectBehaviour() throws Throwable {
    D dInstance = new D();

    MethodHandle mh3 = D.lookup.findSpecial(D.class, "privateRyan",
        MethodType.methodType(void.class), D.class /* specialCaller */);
    mh3.invoke(dInstance);

    // The private method shouldn't be accessible from any special caller except
    // itself...
    try {
      D.lookup.findSpecial(D.class, "privateRyan", MethodType.methodType(void.class), C.class);
      System.out.println("findSpecial(privateRyan, C.class) unexpectedly succeeded");
    } catch (IllegalAccessException expected) {
    }

    // ... or from any lookup context except its own.
    try {
      E.lookup.findSpecial(D.class, "privateRyan", MethodType.methodType(void.class), E.class);
      System.out.println("findSpecial(privateRyan, E.class) unexpectedly succeeded");
    } catch (IllegalAccessException expected) {
    }
  }

  public static void testExceptionDetailMessages() throws Throwable {
    MethodHandle handle = MethodHandles.lookup().findVirtual(String.class, "concat",
        MethodType.methodType(String.class, String.class));

    try {
      handle.invokeExact("a", new Object());
      System.out.println("invokeExact(\"a\", new Object()) unexpectedly succeeded.");
    } catch (WrongMethodTypeException ex) {
      System.out.println("Received WrongMethodTypeException exception");
    }
  }

  public interface Foo {
    public String foo();
  }

  public interface Bar extends Foo {
    public String bar();
  }

  public static abstract class BarAbstractSuper {
    public abstract String abstractSuperPublicMethod();
  }

  public static class BarSuper extends BarAbstractSuper {
    public String superPublicMethod() {
      return "superPublicMethod";
    }

    protected String superProtectedMethod() {
      return "superProtectedMethod";
    }

    public String abstractSuperPublicMethod() {
      return "abstractSuperPublicMethod";
    }

    String superPackageMethod() {
      return "superPackageMethod";
    }
  }

  public static class BarImpl extends BarSuper implements Bar {
    public BarImpl() {
    }

    @Override
    public String foo() {
      return "foo";
    }

    @Override
    public String bar() {
      return "bar";
    }

    public String add(int x, int y) {
      return Arrays.toString(new int[] { x, y });
    }

    private String privateMethod() { return "privateMethod"; }

    public static String staticMethod() { return staticString; }

    private static String staticString;

    {
      // Static constructor
      staticString = Long.toString(System.currentTimeMillis());
    }

    static final MethodHandles.Lookup lookup = MethodHandles.lookup();
  }

  public static void testfindVirtual() throws Throwable {
    // Virtual lookups on static methods should not succeed.
    try {
        MethodHandles.lookup().findVirtual(
            BarImpl.class,  "staticMethod", MethodType.methodType(String.class));
        System.out.println("findVirtual(staticMethod) unexpectedly succeeded");
    } catch (IllegalAccessException expected) {
    }

    // Virtual lookups on private methods should not succeed, unless the Lookup
    // context had sufficient privileges.
    try {
        MethodHandles.lookup().findVirtual(
            BarImpl.class,  "privateMethod", MethodType.methodType(String.class));
        System.out.println("findVirtual(privateMethod) unexpectedly succeeded");
    } catch (IllegalAccessException expected) {
    }

    // Virtual lookup on a private method with a context that *does* have sufficient
    // privileges.
    MethodHandle mh = BarImpl.lookup.findVirtual(
            BarImpl.class,  "privateMethod", MethodType.methodType(String.class));
    String str = (String) mh.invoke(new BarImpl());
    if (!"privateMethod".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#privateMethod: " + str);
    }

    // Find virtual must find interface methods defined by interfaces implemented
    // by the class.
    mh = MethodHandles.lookup().findVirtual(BarImpl.class, "foo",
        MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"foo".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#foo: " + str);
    }

    // Find virtual should check rtype.
    try {
      mh = MethodHandles.lookup().findVirtual(BarImpl.class, "foo",
                                              MethodType.methodType(void.class));
      fail();
    } catch (NoSuchMethodException e) {}

    // And ptypes
    mh = MethodHandles.lookup().findVirtual(
        BarImpl.class, "add", MethodType.methodType(String.class, int.class, int.class));
    try {
      mh = MethodHandles.lookup().findVirtual(
          BarImpl.class, "add", MethodType.methodType(String.class, Integer.class, int.class));
    } catch (NoSuchMethodException e) {}

    // .. and their super-interfaces.
    mh = MethodHandles.lookup().findVirtual(BarImpl.class, "bar",
        MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"bar".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#bar: " + str);
    }

    mh = MethodHandles.lookup().findVirtual(Bar.class, "bar",
                                            MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"bar".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#bar: " + str);
    }

    mh = MethodHandles.lookup().findVirtual(BarAbstractSuper.class, "abstractSuperPublicMethod",
        MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"abstractSuperPublicMethod".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#abstractSuperPublicMethod: " + str);
    }

    // We should also be able to lookup public / protected / package methods in
    // the super class, given sufficient access privileges.
    mh = MethodHandles.lookup().findVirtual(BarImpl.class, "superPublicMethod",
        MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"superPublicMethod".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#superPublicMethod: " + str);
    }

    mh = MethodHandles.lookup().findVirtual(BarImpl.class, "superProtectedMethod",
        MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"superProtectedMethod".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#superProtectedMethod: " + str);
    }

    mh = MethodHandles.lookup().findVirtual(BarImpl.class, "superPackageMethod",
        MethodType.methodType(String.class));
    str = (String) mh.invoke(new BarImpl());
    if (!"superPackageMethod".equals(str)) {
      System.out.println("Unexpected return value for BarImpl#superPackageMethod: " + str);
    }

    try {
      MethodHandles.lookup().findVirtual(BarImpl.class, "<init>",
                                        MethodType.methodType(void.class));
      fail();
    } catch (NoSuchMethodException e) {}
  }

  public static void testfindStatic() throws Throwable {
    MethodHandles.lookup().findStatic(BarImpl.class, "staticMethod",
                                      MethodType.methodType(String.class));
    try {
      MethodHandles.lookup().findStatic(BarImpl.class, "staticMethod",
                                        MethodType.methodType(void.class));
      fail();
    } catch (NoSuchMethodException e) {}
    try {
      MethodHandles.lookup().findStatic(BarImpl.class, "staticMethod",
                                        MethodType.methodType(String.class, int.class));
      fail();
    } catch (NoSuchMethodException e) {}
    try {
      MethodHandles.lookup().findStatic(BarImpl.class, "<clinit>",
                                        MethodType.methodType(void.class));
      fail();
    } catch (NoSuchMethodException e) {}
    try {
      MethodHandles.lookup().findStatic(BarImpl.class, "<init>",
                                        MethodType.methodType(void.class));
      fail();
    } catch (NoSuchMethodException e) {}
  }

  static class UnreflectTester {
    public String publicField;
    private String privateField;

    public static String publicStaticField = "publicStaticValue";
    private static String privateStaticField = "privateStaticValue";

    private UnreflectTester(String val) {
      publicField = val;
      privateField = val;
    }

    // NOTE: The boolean constructor argument only exists to give this a
    // different signature.
    public UnreflectTester(String val, boolean unused) {
      this(val);
    }

    private static String privateStaticMethod() {
      return "privateStaticMethod";
    }

    private String privateMethod() {
      return "privateMethod";
    }

    public static String publicStaticMethod() {
      return "publicStaticMethod";
    }

    public String publicMethod() {
      return "publicMethod";
    }

    public String publicVarArgsMethod(String... args) {
      return "publicVarArgsMethod";
    }
  }

  public static void testUnreflects() throws Throwable {
    UnreflectTester instance = new UnreflectTester("unused");
    Method publicMethod = UnreflectTester.class.getMethod("publicMethod");

    MethodHandle mh = MethodHandles.lookup().unreflect(publicMethod);
    assertEquals("publicMethod", (String) mh.invoke(instance));
    assertEquals("publicMethod", (String) mh.invokeExact(instance));

    Method publicStaticMethod = UnreflectTester.class.getMethod("publicStaticMethod");
    mh = MethodHandles.lookup().unreflect(publicStaticMethod);
    assertEquals("publicStaticMethod", (String) mh.invoke());
    assertEquals("publicStaticMethod", (String) mh.invokeExact());

    Method privateMethod = UnreflectTester.class.getDeclaredMethod("privateMethod");
    try {
      mh = MethodHandles.lookup().unreflect(privateMethod);
      fail();
    } catch (IllegalAccessException expected) {}

    privateMethod.setAccessible(true);
    mh = MethodHandles.lookup().unreflect(privateMethod);
    assertEquals("privateMethod", (String) mh.invoke(instance));
    assertEquals("privateMethod", (String) mh.invokeExact(instance));

    Method privateStaticMethod = UnreflectTester.class.getDeclaredMethod("privateStaticMethod");
    try {
      mh = MethodHandles.lookup().unreflect(privateStaticMethod);
      fail();
    } catch (IllegalAccessException expected) {}

    privateStaticMethod.setAccessible(true);
    mh = MethodHandles.lookup().unreflect(privateStaticMethod);
    assertEquals("privateStaticMethod", (String) mh.invoke());
    assertEquals("privateStaticMethod", (String) mh.invokeExact());

    Constructor privateConstructor = UnreflectTester.class.getDeclaredConstructor(String.class);
    try {
      mh = MethodHandles.lookup().unreflectConstructor(privateConstructor);
      fail();
    } catch (IllegalAccessException expected) {}

    privateConstructor.setAccessible(true);
    mh = MethodHandles.lookup().unreflectConstructor(privateConstructor);
    instance = (UnreflectTester) mh.invokeExact("abc");
    assertEquals("abc", instance.publicField);
    instance = (UnreflectTester) mh.invoke("def");
    assertEquals("def", instance.publicField);
    Constructor publicConstructor = UnreflectTester.class.getConstructor(String.class,
        boolean.class);
    mh = MethodHandles.lookup().unreflectConstructor(publicConstructor);
    instance = (UnreflectTester) mh.invokeExact("abc", false);
    assertEquals("abc", instance.publicField);
    instance = (UnreflectTester) mh.invoke("def", true);
    assertEquals("def", instance.publicField);

    // TODO(narayan): Non exact invokes for field sets/gets are not implemented yet.
    //
    // assertEquals("instanceValue", (String) mh.invoke(new UnreflectTester("instanceValue")));
    Field publicField = UnreflectTester.class.getField("publicField");
    mh = MethodHandles.lookup().unreflectGetter(publicField);
    instance = new UnreflectTester("instanceValue");
    assertEquals("instanceValue", (String) mh.invokeExact(instance));

    mh = MethodHandles.lookup().unreflectSetter(publicField);
    instance = new UnreflectTester("instanceValue");
    mh.invokeExact(instance, "updatedInstanceValue");
    assertEquals("updatedInstanceValue", instance.publicField);

    Field publicStaticField = UnreflectTester.class.getField("publicStaticField");
    mh = MethodHandles.lookup().unreflectGetter(publicStaticField);
    UnreflectTester.publicStaticField = "updatedStaticValue";
    assertEquals("updatedStaticValue", (String) mh.invokeExact());

    mh = MethodHandles.lookup().unreflectSetter(publicStaticField);
    UnreflectTester.publicStaticField = "updatedStaticValue";
    mh.invokeExact("updatedStaticValue2");
    assertEquals("updatedStaticValue2", UnreflectTester.publicStaticField);

    Field privateField = UnreflectTester.class.getDeclaredField("privateField");
    try {
      mh = MethodHandles.lookup().unreflectGetter(privateField);
      fail();
    } catch (IllegalAccessException expected) {
    }
    try {
      mh = MethodHandles.lookup().unreflectSetter(privateField);
      fail();
    } catch (IllegalAccessException expected) {
    }

    privateField.setAccessible(true);

    mh = MethodHandles.lookup().unreflectGetter(privateField);
    instance = new UnreflectTester("instanceValue");
    assertEquals("instanceValue", (String) mh.invokeExact(instance));

    mh = MethodHandles.lookup().unreflectSetter(privateField);
    instance = new UnreflectTester("instanceValue");
    mh.invokeExact(instance, "updatedInstanceValue");
    assertEquals("updatedInstanceValue", instance.privateField);

    Field privateStaticField = UnreflectTester.class.getDeclaredField("privateStaticField");
    try {
      mh = MethodHandles.lookup().unreflectGetter(privateStaticField);
      fail();
    } catch (IllegalAccessException expected) {
    }
    try {
      mh = MethodHandles.lookup().unreflectSetter(privateStaticField);
      fail();
    } catch (IllegalAccessException expected) {
    }

    privateStaticField.setAccessible(true);
    mh = MethodHandles.lookup().unreflectGetter(privateStaticField);
    privateStaticField.set(null, "updatedStaticValue");
    assertEquals("updatedStaticValue", (String) mh.invokeExact());

    mh = MethodHandles.lookup().unreflectSetter(privateStaticField);
    privateStaticField.set(null, "updatedStaticValue");
    mh.invokeExact("updatedStaticValue2");
    assertEquals("updatedStaticValue2", (String) privateStaticField.get(null));

    // unreflectSpecial testing - F is an interface that G implements

    G g = new G();
    g.sayHi();  // prints "G.sayHi()"

    MethodHandles.Lookup lookupInG = g.getLookup();
    Method methodInG = G.class.getDeclaredMethod("sayHi");
    lookupInG.unreflectSpecial(methodInG, G.class).invoke(g); // prints "G.sayHi()"

    Method methodInF = F.class.getDeclaredMethod("sayHi");
    lookupInG.unreflect(methodInF).invoke(g);  // prints "G.sayHi()"
    lookupInG.in(G.class).unreflectSpecial(methodInF, G.class).invoke(g);  // prints "F.sayHi()"
    lookupInG.unreflectSpecial(methodInF, G.class).bindTo(g).invokeWithArguments();

    // unreflectSpecial testing - other.Chatty is an interface that H implements

    H h = new H();
    h.chatter();

    MethodHandles.Lookup lookupInH = h.getLookup();
    Method methodInH = H.class.getDeclaredMethod("chatter");
    lookupInH.unreflectSpecial(methodInH, H.class).invoke(h);

    Method methodInChatty = Chatty.class.getDeclaredMethod("chatter");
    lookupInH.unreflect(methodInChatty).invoke(h);
    lookupInH.in(H.class).unreflectSpecial(methodInChatty, H.class).invoke(h);
    lookupInH.unreflectSpecial(methodInChatty, H.class).bindTo(h).invokeWithArguments();
  }

  // This method only exists to fool Jack's handling of types. See b/32536744.
  public static CharSequence getSequence() {
    return "foo";
  }

  public static void testAsType() throws Throwable {
    // The type of this handle is (String, String)String.
    MethodHandle mh = MethodHandles.lookup().findVirtual(String.class,
        "concat", MethodType.methodType(String.class, String.class));

    // Change it to (CharSequence, String)Object.
    MethodHandle asType = mh.asType(
        MethodType.methodType(Object.class, CharSequence.class, String.class));

    Object obj = asType.invokeExact((CharSequence) getSequence(), "bar");
    assertEquals("foobar", (String) obj);

    // Should fail due to a wrong return type.
    try {
      String str = (String) asType.invokeExact((CharSequence) getSequence(), "bar");
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // Should fail due to a wrong argument type (String instead of Charsequence).
    try {
      String str = (String) asType.invokeExact("baz", "bar");
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // Calls to asType should fail if the types are not convertible.
    //
    // Bad return type conversion.
    try {
      mh.asType(MethodType.methodType(int.class, String.class, String.class));
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // Bad argument conversion.
    try {
      mh.asType(MethodType.methodType(String.class, int.class, String.class));
      fail();
    } catch (WrongMethodTypeException expected) {
    }
  }

  public static void assertTrue(boolean value) {
    if (!value) {
      throw new AssertionError("assertTrue value: " + value);
    }
  }

  public static void assertFalse(boolean value) {
    if (value) {
      throw new AssertionError("assertTrue value: " + value);
    }
  }

  public static void assertEquals(int i1, int i2) {
    if (i1 == i2) { return; }
    throw new AssertionError("assertEquals i1: " + i1 + ", i2: " + i2);
  }

  public static void assertEquals(long i1, long i2) {
    if (i1 == i2) { return; }
    throw new AssertionError("assertEquals l1: " + i1 + ", l2: " + i2);
  }

  public static void assertEquals(Object o, Object p) {
    if (o == p) { return; }
    if (o != null && p != null && o.equals(p)) { return; }
    throw new AssertionError("assertEquals: o1: " + o + ", o2: " + p);
  }

  public static void assertEquals(String s1, String s2) {
    if (s1 == s2) {
      return;
    }

    if (s1 != null && s2 != null && s1.equals(s2)) {
      return;
    }

    throw new AssertionError("assertEquals s1: " + s1 + ", s2: " + s2);
  }

  public static void fail() {
    System.out.println("fail");
    Thread.dumpStack();
  }

  public static void fail(String message) {
    System.out.println("fail: " + message);
    Thread.dumpStack();
  }

  public static void testConstructors() throws Throwable {
    MethodHandle mh =
        MethodHandles.lookup().findConstructor(Float.class,
                                               MethodType.methodType(void.class,
                                                                     float.class));
    Float value = (Float) mh.invokeExact(0.33f);
    if (value.floatValue() != 0.33f) {
      fail("Unexpected float value from invokeExact " + value.floatValue());
    }

    value = (Float) mh.invoke(3.34f);
    if (value.floatValue() != 3.34f) {
      fail("Unexpected float value from invoke " + value.floatValue());
    }

    mh = MethodHandles.lookup().findConstructor(Double.class,
                                                MethodType.methodType(void.class, String.class));
    Double d = (Double) mh.invoke("8.45e3");
    if (d.doubleValue() != 8.45e3) {
      fail("Unexpected double value from Double(String) " + value.doubleValue());
    }

    mh = MethodHandles.lookup().findConstructor(Double.class,
                                                MethodType.methodType(void.class, double.class));
    d = (Double) mh.invoke(8.45e3);
    if (d.doubleValue() != 8.45e3) {
      fail("Unexpected double value from Double(double) " + value.doubleValue());
    }

    // Primitive type
    try {
      mh = MethodHandles.lookup().findConstructor(int.class, MethodType.methodType(void.class));
      fail("Unexpected lookup success for primitive constructor");
    } catch (NoSuchMethodException e) {}

    // Interface
    try {
      mh = MethodHandles.lookup().findConstructor(Readable.class,
                                                  MethodType.methodType(void.class));
      fail("Unexpected lookup success for interface constructor");
    } catch (NoSuchMethodException e) {}

    // Abstract
    mh = MethodHandles.lookup().findConstructor(Process.class, MethodType.methodType(void.class));
    try {
      mh.invoke();
      fail("Unexpected ability to instantiate an abstract class");
    } catch (InstantiationException e) {}

    // Non-existent
    try {
        MethodHandle bad = MethodHandles.lookup().findConstructor(
            String.class, MethodType.methodType(String.class, Float.class));
        fail("Unexpected success for non-existent constructor");
    } catch (NoSuchMethodException e) {}

    // Non-void constructor search. (I)I instead of (I)V.
    try {
        MethodHandle foo = MethodHandles.lookup().findConstructor(
            Integer.class, MethodType.methodType(Integer.class, Integer.class));
        fail("Unexpected success for non-void type for findConstructor");
    } catch (NoSuchMethodException e) {}

    // Array class constructor.
    try {
        MethodHandle foo = MethodHandles.lookup().findConstructor(
            Object[].class, MethodType.methodType(void.class));
        fail("Unexpected success for array class type for findConstructor");
    } catch (NoSuchMethodException e) {}
  }

  public static void testStringConstructors() throws Throwable {
    final String testPattern = "The system as we know it is broken";

    // String()
    MethodHandle mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class));
    String s = (String) mh.invokeExact();
    if (!s.equals("")) {
      fail("Unexpected empty string constructor result: '" + s + "'");
    }

    // String(String)
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, String.class));
    s = (String) mh.invokeExact(testPattern);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(char[])
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, char[].class));
    s = (String) mh.invokeExact(testPattern.toCharArray());
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(char[], int, int)
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, char[].class, int.class, int.class));
    s = (String) mh.invokeExact(new char [] { 'a', 'b', 'c', 'd', 'e'}, 2, 3);
    if (!s.equals("cde")) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(int[] codePoints, int offset, int count)
    StringBuffer sb = new StringBuffer(testPattern);
    int[] codePoints = new int[sb.codePointCount(0, sb.length())];
    for (int i = 0; i < sb.length(); ++i) {
      codePoints[i] = sb.codePointAt(i);
    }
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, int[].class, int.class, int.class));
    s = (String) mh.invokeExact(codePoints, 0, codePoints.length);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte ascii[], int hibyte, int offset, int count)
    byte [] ascii = testPattern.getBytes(StandardCharsets.US_ASCII);
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, byte[].class, int.class, int.class));
    s = (String) mh.invokeExact(ascii, 0, ascii.length);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte bytes[], int offset, int length, String charsetName)
    mh = MethodHandles.lookup().findConstructor(
        String.class,
        MethodType.methodType(void.class, byte[].class, int.class, int.class, String.class));
    s = (String) mh.invokeExact(ascii, 0, 5, StandardCharsets.US_ASCII.name());
    if (!s.equals(testPattern.substring(0, 5))) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte bytes[], int offset, int length, Charset charset)
    mh = MethodHandles.lookup().findConstructor(
        String.class,
        MethodType.methodType(void.class, byte[].class, int.class, int.class, Charset.class));
    s = (String) mh.invokeExact(ascii, 0, 5, StandardCharsets.US_ASCII);
    if (!s.equals(testPattern.substring(0, 5))) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte bytes[], String charsetName)
    mh = MethodHandles.lookup().findConstructor(
        String.class,
        MethodType.methodType(void.class, byte[].class, String.class));
    s = (String) mh.invokeExact(ascii, StandardCharsets.US_ASCII.name());
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte bytes[], Charset charset)
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, byte[].class, Charset.class));
    s = (String) mh.invokeExact(ascii, StandardCharsets.US_ASCII);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte bytes[], int offset, int length)
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, byte[].class, int.class, int.class));
    s = (String) mh.invokeExact(ascii, 1, ascii.length - 2);
    s = testPattern.charAt(0) + s + testPattern.charAt(testPattern.length() - 1);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(byte bytes[])
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, byte[].class));
    s = (String) mh.invokeExact(ascii);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    // String(StringBuffer buffer)
    mh = MethodHandles.lookup().findConstructor(
        String.class, MethodType.methodType(void.class, StringBuffer.class));
    s = (String) mh.invokeExact(sb);
    if (!s.equals(testPattern)) {
      fail("Unexpected string constructor result: '" + s + "'");
    }

    System.out.println("String constructors done.");
  }

  private static void testReferenceReturnValueConversions() throws Throwable {
    MethodHandle mh = MethodHandles.lookup().findStatic(
        Float.class, "valueOf", MethodType.methodType(Float.class, String.class));

    // No conversion
    Float f = (Float) mh.invokeExact("1.375");
    if (f.floatValue() != 1.375) {
      fail();
    }
    f = (Float) mh.invoke("1.875");
    if (f.floatValue() != 1.875) {
      fail();
    }

    // Bad conversion
    try {
      int i = (int) mh.invokeExact("7.77");
      fail();
    } catch (WrongMethodTypeException e) {}

    try {
      int i = (int) mh.invoke("7.77");
      fail();
    } catch (WrongMethodTypeException e) {}

    // Assignment to super-class.
    Number n = (Number) mh.invoke("1.11");
    try {
      Number o = (Number) mh.invokeExact("1.11");
      fail();
    } catch (WrongMethodTypeException e) {}

    // Assignment to widened boxed primitive class.
    try {
      Double u = (Double) mh.invoke("1.11");
      fail();
    } catch (ClassCastException e) {}

    try {
      Double v = (Double) mh.invokeExact("1.11");
      fail();
    } catch (WrongMethodTypeException e) {}

    // Unboxed
    float p = (float) mh.invoke("1.11");
    if (p != 1.11f) {
      fail();
    }

    // Unboxed and widened
    double d = (double) mh.invoke("2.5");
    if (d != 2.5) {
      fail();
    }

    // Interface
    Comparable<Float> c = (Comparable<Float>) mh.invoke("2.125");
    if (c.compareTo(new Float(2.125f)) != 0) {
      fail();
    }

    System.out.println("testReferenceReturnValueConversions done.");
  }

  private static void testPrimitiveReturnValueConversions() throws Throwable {
    MethodHandle mh = MethodHandles.lookup().findStatic(
        Math.class, "min", MethodType.methodType(int.class, int.class, int.class));

    final int SMALL = -8972;
    final int LARGE = 7932529;

    // No conversion
    if ((int) mh.invokeExact(LARGE, SMALL) != SMALL) {
      fail();
    } else if ((int) mh.invoke(LARGE, SMALL) != SMALL) {
      fail();
    } else if ((int) mh.invokeExact(SMALL, LARGE) != SMALL) {
      fail();
    } else if ((int) mh.invoke(SMALL, LARGE) != SMALL) {
      fail();
    }

    // int -> long
    try {
      if ((long) mh.invokeExact(LARGE, SMALL) != (long) SMALL) {}
        fail();
    } catch (WrongMethodTypeException e) {}

    if ((long) mh.invoke(LARGE, SMALL) != (long) SMALL) {
      fail();
    }

    // int -> short
    try {
      if ((short) mh.invokeExact(LARGE, SMALL) != (short) SMALL) {}
      fail();
    } catch (WrongMethodTypeException e) {}

    try {
      if ((short) mh.invoke(LARGE, SMALL) != (short) SMALL) {
        fail();
      }
    } catch (WrongMethodTypeException e) {}

    // int -> Integer
    try {
      if (!((Integer) mh.invokeExact(LARGE, SMALL)).equals(new Integer(SMALL))) {}
      fail();
    } catch (WrongMethodTypeException e) {}

    if (!((Integer) mh.invoke(LARGE, SMALL)).equals(new Integer(SMALL))) {
      fail();
    }

    // int -> Long
    try {
      Long l = (Long) mh.invokeExact(LARGE, SMALL);
      fail();
    } catch (WrongMethodTypeException e) {}

    try {
      Long l = (Long) mh.invoke(LARGE, SMALL);
      fail();
    } catch (WrongMethodTypeException e) {}

    // int -> Short
    try {
      Short s = (Short) mh.invokeExact(LARGE, SMALL);
      fail();
    } catch (WrongMethodTypeException e) {}

    try {
      Short s = (Short) mh.invoke(LARGE, SMALL);
      fail();
    } catch (WrongMethodTypeException e) {}

    // int -> Process
    try {
      Process p = (Process) mh.invokeExact(LARGE, SMALL);
      fail();
    } catch (WrongMethodTypeException e) {}

    try {
      Process p = (Process) mh.invoke(LARGE, SMALL);
      fail();
    } catch (WrongMethodTypeException e) {}

    // void -> Object
    mh = MethodHandles.lookup().findStatic(System.class, "gc", MethodType.methodType(void.class));
    Object o = (Object) mh.invoke();
    if (o != null) fail();

    // void -> long
    long l = (long) mh.invoke();
    if (l != 0) fail();

    // boolean -> Boolean
    mh = MethodHandles.lookup().findStatic(Boolean.class, "parseBoolean",
                                           MethodType.methodType(boolean.class, String.class));
    Boolean z = (Boolean) mh.invoke("True");
    if (!z.booleanValue()) fail();

    // boolean -> int
    try {
        int dummy = (int) mh.invoke("True");
        fail();
    } catch (WrongMethodTypeException e) {}

    // boolean -> Integer
    try {
        Integer dummy = (Integer) mh.invoke("True");
        fail();
    } catch (WrongMethodTypeException e) {}

    // Boolean -> boolean
    mh = MethodHandles.lookup().findStatic(Boolean.class, "valueOf",
                                           MethodType.methodType(Boolean.class, boolean.class));
    boolean w = (boolean) mh.invoke(false);
    if (w) fail();

    // Boolean -> int
    try {
        int dummy = (int) mh.invoke(false);
        fail();
    } catch (WrongMethodTypeException e) {}

    // Boolean -> Integer
    try {
        Integer dummy = (Integer) mh.invoke("True");
        fail();
    } catch (WrongMethodTypeException e) {}

    System.out.println("testPrimitiveReturnValueConversions done.");
  }

  public static void testReturnValueConversions() throws Throwable {
    testReferenceReturnValueConversions();
    testPrimitiveReturnValueConversions();
  }

  public static class BaseVariableArityTester {
    public String update(Float f0, Float... floats) {
      return "base " + f0 + ", " + Arrays.toString(floats);
    }
  }

  public static class VariableArityTester extends BaseVariableArityTester {
    private String lastResult;

    // Constructors
    public VariableArityTester() {}
    public VariableArityTester(boolean... booleans) { update(booleans); }
    public VariableArityTester(byte... bytes) { update(bytes); }
    public VariableArityTester(char... chars) { update(chars); }
    public VariableArityTester(short... shorts) { update(shorts); }
    public VariableArityTester(int... ints) { update(ints); }
    public VariableArityTester(long... longs) { update(longs); }
    public VariableArityTester(float... floats) { update(floats); }
    public VariableArityTester(double... doubles) { update(doubles); }
    public VariableArityTester(Float f0, Float... floats) { update(f0, floats); }
    public VariableArityTester(String s0, String... strings) { update(s0, strings); }
    public VariableArityTester(char c, Number... numbers) { update(c, numbers); }
    @SafeVarargs
    public VariableArityTester(ArrayList<Integer> l0, ArrayList<Integer>... lists) {
      update(l0, lists);
    }
    public VariableArityTester(List l0, List... lists) { update(l0, lists); }

    // Methods
    public String update(boolean... booleans) { return lastResult = tally(booleans); }
    public String update(byte... bytes) { return lastResult = tally(bytes); }
    public String update(char... chars) { return lastResult = tally(chars); }
    public String update(short... shorts) { return lastResult = tally(shorts); }
    public String update(int... ints) {
      lastResult = tally(ints);
      return lastResult;
    }
    public String update(long... longs) { return lastResult = tally(longs); }
    public String update(float... floats) { return lastResult = tally(floats); }
    public String update(double... doubles) { return lastResult = tally(doubles); }
    @Override
    public String update(Float f0, Float... floats) { return lastResult = tally(f0, floats); }
    public String update(String s0, String... strings) { return lastResult = tally(s0, strings); }
    public String update(char c, Number... numbers) { return lastResult = tally(c, numbers); }
    @SafeVarargs
    public final String update(ArrayList<Integer> l0, ArrayList<Integer>... lists) {
      lastResult = tally(l0, lists);
      return lastResult;
    }
    public String update(List l0, List... lists) { return lastResult = tally(l0, lists); }

    public String arrayMethod(Object[] o) {
      return Arrays.deepToString(o);
    }

    public String lastResult() { return lastResult; }

    // Static Methods
    public static String tally(boolean... booleans) { return Arrays.toString(booleans); }
    public static String tally(byte... bytes) { return Arrays.toString(bytes); }
    public static String tally(char... chars) { return Arrays.toString(chars); }
    public static String tally(short... shorts) { return Arrays.toString(shorts); }
    public static String tally(int... ints) { return Arrays.toString(ints); }
    public static String tally(long... longs) { return Arrays.toString(longs); }
    public static String tally(float... floats) { return Arrays.toString(floats); }
    public static String tally(double... doubles) { return Arrays.toString(doubles); }
    public static String tally(Float f0, Float... floats) {
      return f0 + ", " + Arrays.toString(floats);
    }
    public static String tally(String s0, String... strings) {
      return s0 + ", " + Arrays.toString(strings);
    }
    public static String tally(char c, Number... numbers) {
      return c + ", " + Arrays.toString(numbers);
    }
    @SafeVarargs
    public static String tally(ArrayList<Integer> l0, ArrayList<Integer>... lists) {
      return Arrays.toString(l0.toArray()) + ", " + Arrays.deepToString(lists);
    }
    public static String tally(List l0, List... lists) {
      return Arrays.deepToString(l0.toArray()) + ", " + Arrays.deepToString(lists);
    }
    public static void foo(int... ints) { System.out.println(Arrays.toString(ints)); }
    public static long sumToPrimitive(int... ints) {
      long result = 0;
      for (int i : ints) result += i;
      return result;
    }
    public static Long sumToReference(int... ints) {
      System.out.println("Hi");
      return new Long(sumToPrimitive(ints));
    }
    public static MethodHandles.Lookup lookup() {
      return MethodHandles.lookup();
    }
  }

  // This method only exists to fool Jack's handling of types. See b/32536744.
  public static Object getAsObject(String[] strings) {
    return (Object) strings;
  }

  public static void testVariableArity() throws Throwable {
    MethodHandle mh;
    VariableArityTester vat = new VariableArityTester();

    assertEquals("[1]", vat.update(1));
    assertEquals("[1, 1]", vat.update(1, 1));
    assertEquals("[1, 1, 1]", vat.update(1, 1, 1));

    // Methods - boolean
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, boolean[].class));
    assertTrue(mh.isVarargsCollector());
    assertFalse(mh.asFixedArity().isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[true, false, true]", mh.invoke(vat, true, false, true));
    assertEquals("[true, false, true]", mh.invoke(vat, new boolean[] { true, false, true}));
    assertEquals("[false, true]", mh.invoke(vat, Boolean.valueOf(false), Boolean.valueOf(true)));
    try {
      mh.invoke(vat, true, true, 0);
      fail();
    } catch (WrongMethodTypeException e) {}
    try {
      assertEquals("[false, true]", mh.invoke(vat, Boolean.valueOf(false), (Boolean) null));
      fail();
    } catch (NullPointerException e) {}

    // Methods - byte
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, byte[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[32, 64, 97]", mh.invoke(vat, (byte) 32, Byte.valueOf((byte) 64), (byte) 97));
    assertEquals("[32, 64, 97]", mh.invoke(vat, new byte[] {(byte) 32, (byte) 64, (byte) 97}));
    try {
      mh.invoke(vat, (byte) 1, Integer.valueOf(3), (byte) 0);
      fail();
    } catch (WrongMethodTypeException e) {}

    // Methods - char
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, char[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[A, B, C]", mh.invoke(vat, 'A', Character.valueOf('B'), 'C'));
    assertEquals("[W, X, Y, Z]", mh.invoke(vat, new char[] { 'W', 'X', 'Y', 'Z' }));

    // Methods - short
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, short[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[32767, -32768, 0]",
                 mh.invoke(vat, Short.MAX_VALUE, Short.MIN_VALUE, Short.valueOf((short) 0)));
    assertEquals("[1, -1]", mh.invoke(vat, new short[] { (short) 1, (short) -1 }));

    // Methods - int
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, int[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[0, 2147483647, -2147483648, 0]",
                 mh.invoke(vat, Integer.valueOf(0), Integer.MAX_VALUE, Integer.MIN_VALUE, 0));
    assertEquals("[0, -1, 1, 0]", mh.invoke(vat, new int[] { 0, -1, 1, 0 }));

    assertEquals("[5, 4, 3, 2, 1]", (String) mh.invokeExact(vat, new int [] { 5, 4, 3, 2, 1 }));
    try {
      assertEquals("[5, 4, 3, 2, 1]", (String) mh.invokeExact(vat, 5, 4, 3, 2, 1));
      fail();
    } catch (WrongMethodTypeException e) {}
    assertEquals("[5, 4, 3, 2, 1]", (String) mh.invoke(vat, 5, 4, 3, 2, 1));

    // Methods - long
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, long[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[0, 9223372036854775807, -9223372036854775808]",
                 mh.invoke(vat, Long.valueOf(0), Long.MAX_VALUE, Long.MIN_VALUE));
    assertEquals("[0, -1, 1, 0]", mh.invoke(vat, new long[] { 0, -1, 1, 0 }));

    // Methods - float
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, float[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[0.0, 1.25, -1.25]",
                 mh.invoke(vat, 0.0f, Float.valueOf(1.25f), Float.valueOf(-1.25f)));
    assertEquals("[0.0, -1.0, 1.0, 0.0]",
                 mh.invoke(vat, new float[] { 0.0f, -1.0f, 1.0f, 0.0f }));

    // Methods - double
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, double[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke(vat));
    assertEquals("[0.0, 1.25, -1.25]",
                 mh.invoke(vat, 0.0, Double.valueOf(1.25), Double.valueOf(-1.25)));
    assertEquals("[0.0, -1.0, 1.0, 0.0]",
                 mh.invoke(vat, new double[] { 0.0, -1.0, 1.0, 0.0 }));
    mh.invoke(vat, 0.3f, 1.33, 1.33);

    // Methods - String
    mh = MethodHandles.lookup().
        findVirtual(VariableArityTester.class, "update",
                    MethodType.methodType(String.class, String.class, String[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("Echidna, []", mh.invoke(vat, "Echidna"));
    assertEquals("Bongo, [Jerboa, Okapi]",
                 mh.invoke(vat, "Bongo", "Jerboa", "Okapi"));

    // Methods - Float
    mh = MethodHandles.lookup().
        findVirtual(VariableArityTester.class, "update",
                    MethodType.methodType(String.class, Float.class, Float[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invoke(vat, Float.valueOf(9.99f),
                                    new Float[] { Float.valueOf(0.0f),
                                                  Float.valueOf(0.1f),
                                                  Float.valueOf(1.1f) }));
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invoke(vat, Float.valueOf(9.99f), Float.valueOf(0.0f),
                                    Float.valueOf(0.1f), Float.valueOf(1.1f)));
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invoke(vat, Float.valueOf(9.99f), 0.0f, 0.1f, 1.1f));
    try {
      assertEquals("9.99, [77.0, 33.0, 64.0]",
                   (String) mh.invoke(vat, Float.valueOf(9.99f), 77, 33, 64));
      fail();
    } catch (WrongMethodTypeException e) {}
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invokeExact(vat, Float.valueOf(9.99f),
                                         new Float[] { Float.valueOf(0.0f),
                                                       Float.valueOf(0.1f),
                                                       Float.valueOf(1.1f) }));
    assertEquals("9.99, [0.0, null, 1.1]",
                 (String) mh.invokeExact(vat, Float.valueOf(9.99f),
                                         new Float[] { Float.valueOf(0.0f),
                                                       null,
                                                       Float.valueOf(1.1f) }));
    try {
      assertEquals("9.99, [0.0, 0.1, 1.1]",
                   (String) mh.invokeExact(vat, Float.valueOf(9.99f), 0.0f, 0.1f, 1.1f));
      fail();
    } catch (WrongMethodTypeException e) {}

    // Methods - Number
    mh = MethodHandles.lookup().
        findVirtual(VariableArityTester.class, "update",
                    MethodType.methodType(String.class, char.class, Number[].class));
    assertTrue(mh.isVarargsCollector());
    assertFalse(mh.asFixedArity().isVarargsCollector());
    assertEquals("x, []",  (String) mh.invoke(vat, 'x'));
    assertEquals("x, [3.141]", (String) mh.invoke(vat, 'x', 3.141));
    assertEquals("x, [null, 3.131, 37]",
                 (String) mh.invoke(vat, 'x', null, 3.131, new Integer(37)));
    try {
      assertEquals("x, [null, 3.131, bad, 37]",
                   (String) mh.invoke(vat, 'x', null, 3.131, "bad", new Integer(37)));
      assertTrue(false);
      fail();
    } catch (ClassCastException e) {}
    try {
      assertEquals("x, [null, 3.131, bad, 37]",
                   (String) mh.invoke(
                       vat, 'x', (Process) null, 3.131, "bad", new Integer(37)));
      assertTrue(false);
      fail();
    } catch (ClassCastException e) {}

    // Methods - an array method that is not variable arity.
    mh = MethodHandles.lookup().findVirtual(
        VariableArityTester.class, "arrayMethod",
        MethodType.methodType(String.class, Object[].class));
    assertFalse(mh.isVarargsCollector());
    mh.invoke(vat, new Object[] { "123" });
    try {
      assertEquals("-", mh.invoke(vat, new Float(3), new Float(4)));
      fail();
    } catch (WrongMethodTypeException e) {}
    mh = mh.asVarargsCollector(Object[].class);
    assertTrue(mh.isVarargsCollector());
    assertEquals("[3.0, 4.0]", (String) mh.invoke(vat, new Float(3), new Float(4)));

    // Constructors - default
    mh = MethodHandles.lookup().findConstructor(
        VariableArityTester.class, MethodType.methodType(void.class));
    assertFalse(mh.isVarargsCollector());

    // Constructors - boolean
    mh = MethodHandles.lookup().findConstructor(
        VariableArityTester.class, MethodType.methodType(void.class, boolean[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[true, true, false]",
                 ((VariableArityTester) mh.invoke(new boolean[] {true, true, false})).lastResult());
    assertEquals("[true, true, false]",
                 ((VariableArityTester) mh.invoke(true, true, false)).lastResult());
    try {
      assertEquals("[true, true, false]",
                   ((VariableArityTester) mh.invokeExact(true, true, false)).lastResult());
      fail();
    } catch (WrongMethodTypeException e) {}

    // Constructors - byte
    mh = MethodHandles.lookup().findConstructor(
        VariableArityTester.class, MethodType.methodType(void.class, byte[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("[55, 66, 60]",
                 ((VariableArityTester)
                  mh.invoke(new byte[] {(byte) 55, (byte) 66, (byte) 60})).lastResult());
    assertEquals("[55, 66, 60]",
                 ((VariableArityTester) mh.invoke(
                     (byte) 55, (byte) 66, (byte) 60)).lastResult());
    try {
      assertEquals("[55, 66, 60]",
                   ((VariableArityTester) mh.invokeExact(
                       (byte) 55, (byte) 66, (byte) 60)).lastResult());
      fail();
    } catch (WrongMethodTypeException e) {}
    try {
      assertEquals("[3, 3]",
                   ((VariableArityTester) mh.invoke(
                       new Number[] { Byte.valueOf((byte) 3), (byte) 3})).lastResult());
      fail();
    } catch (WrongMethodTypeException e) {}

    // Constructors - String (have a different path than other reference types).
    mh = MethodHandles.lookup().findConstructor(
        VariableArityTester.class, MethodType.methodType(void.class, String.class, String[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("x, []", ((VariableArityTester) mh.invoke("x")).lastResult());
    assertEquals("x, [y]", ((VariableArityTester) mh.invoke("x", "y")).lastResult());
    assertEquals("x, [y, z]",
                 ((VariableArityTester) mh.invoke("x", new String[] { "y", "z" })).lastResult());
    try {
      assertEquals("x, [y]", ((VariableArityTester) mh.invokeExact("x", "y")).lastResult());
      fail();
    } catch (WrongMethodTypeException e) {}
    assertEquals("x, [null, z]",
                 ((VariableArityTester) mh.invoke("x", new String[] { null, "z" })).lastResult());

    // Constructors - Number
    mh = MethodHandles.lookup().findConstructor(
        VariableArityTester.class, MethodType.methodType(void.class, char.class, Number[].class));
    assertTrue(mh.isVarargsCollector());
    assertFalse(mh.asFixedArity().isVarargsCollector());
    assertEquals("x, []", ((VariableArityTester) mh.invoke('x')).lastResult());
    assertEquals("x, [3.141]", ((VariableArityTester) mh.invoke('x', 3.141)).lastResult());
    assertEquals("x, [null, 3.131, 37]",
                 ((VariableArityTester) mh.invoke('x', null, 3.131, new Integer(37))).lastResult());
    try {
      assertEquals("x, [null, 3.131, bad, 37]",
                   ((VariableArityTester) mh.invoke(
                       'x', null, 3.131, "bad", new Integer(37))).lastResult());
      assertTrue(false);
      fail();
    } catch (ClassCastException e) {}
    try {
      assertEquals("x, [null, 3.131, bad, 37]",
                   ((VariableArityTester) mh.invoke(
                       'x', (Process) null, 3.131, "bad", new Integer(37))).lastResult());
      assertTrue(false);
      fail();
    } catch (ClassCastException e) {}

    // Static Methods - Float
    mh = MethodHandles.lookup().
        findStatic(VariableArityTester.class, "tally",
                   MethodType.methodType(String.class, Float.class, Float[].class));
    assertTrue(mh.isVarargsCollector());
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invoke(Float.valueOf(9.99f),
                                    new Float[] { Float.valueOf(0.0f),
                                                  Float.valueOf(0.1f),
                                                  Float.valueOf(1.1f) }));
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invoke(Float.valueOf(9.99f), Float.valueOf(0.0f),
                                    Float.valueOf(0.1f), Float.valueOf(1.1f)));
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invoke(Float.valueOf(9.99f), 0.0f, 0.1f, 1.1f));
    try {
      assertEquals("9.99, [77.0, 33.0, 64.0]",
                   (String) mh.invoke(Float.valueOf(9.99f), 77, 33, 64));
      fail();
    } catch (WrongMethodTypeException e) {}
    assertEquals("9.99, [0.0, 0.1, 1.1]",
                 (String) mh.invokeExact(Float.valueOf(9.99f),
                                         new Float[] { Float.valueOf(0.0f),
                                                       Float.valueOf(0.1f),
                                                       Float.valueOf(1.1f) }));
    assertEquals("9.99, [0.0, null, 1.1]",
                 (String) mh.invokeExact(Float.valueOf(9.99f),
                                         new Float[] { Float.valueOf(0.0f),
                                                       null,
                                                       Float.valueOf(1.1f) }));
    try {
      assertEquals("9.99, [0.0, 0.1, 1.1]",
                   (String) mh.invokeExact(Float.valueOf(9.99f), 0.0f, 0.1f, 1.1f));
      fail();
    } catch (WrongMethodTypeException e) {}

    // Special methods - Float
    mh = VariableArityTester.lookup().
            findSpecial(BaseVariableArityTester.class, "update",
                        MethodType.methodType(String.class, Float.class, Float[].class),
                        VariableArityTester.class);
    assertTrue(mh.isVarargsCollector());
    assertEquals("base 9.99, [0.0, 0.1, 1.1]",
    (String) mh.invoke(vat,
                       Float.valueOf(9.99f),
                       new Float[] { Float.valueOf(0.0f),
                                     Float.valueOf(0.1f),
                                     Float.valueOf(1.1f) }));
    assertEquals("base 9.99, [0.0, 0.1, 1.1]",
    (String) mh.invoke(vat, Float.valueOf(9.99f), Float.valueOf(0.0f),
                       Float.valueOf(0.1f), Float.valueOf(1.1f)));

    // Return value conversions.
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, int[].class));
    assertEquals("[1, 2, 3]", (String) mh.invoke(vat, 1, 2, 3));
    assertEquals("[1, 2, 3]", (Object) mh.invoke(vat, 1, 2, 3));
    try {
      assertEquals("[1, 2, 3, 4]", (long) mh.invoke(vat, 1, 2, 3));
      fail();
    } catch (WrongMethodTypeException e) {}
    assertEquals("[1, 2, 3]", vat.lastResult());
    mh = MethodHandles.lookup().findStatic(VariableArityTester.class, "sumToPrimitive",
                                           MethodType.methodType(long.class, int[].class));
    assertEquals(10l, (long) mh.invoke(1, 2, 3, 4));
    assertEquals(Long.valueOf(10l), (Long) mh.invoke(1, 2, 3, 4));
    mh = MethodHandles.lookup().findStatic(VariableArityTester.class, "sumToReference",
                                           MethodType.methodType(Long.class, int[].class));
    Object o = mh.invoke(1, 2, 3, 4);
    long l = (long) mh.invoke(1, 2, 3, 4);
    assertEquals(10l, (long) mh.invoke(1, 2, 3, 4));
    assertEquals(Long.valueOf(10l), (Long) mh.invoke(1, 2, 3, 4));
    try {
      // WrongMethodTypeException should be raised before invoke here.
      System.out.print("Expect Hi here: ");
      assertEquals(Long.valueOf(10l), (Byte) mh.invoke(1, 2, 3, 4));
      fail();
    } catch (ClassCastException e) {}
    try {
      // WrongMethodTypeException should be raised before invoke here.
      System.out.println("Don't expect Hi now");
      byte b = (byte) mh.invoke(1, 2, 3, 4);
      fail();
    } catch (WrongMethodTypeException e) {}

    // Return void produces 0 / null.
    mh = MethodHandles.lookup().findStatic(VariableArityTester.class, "foo",
                                           MethodType.methodType(void.class, int[].class));
    assertEquals(null, (Object) mh.invoke(3, 2, 1));
    assertEquals(0l, (long) mh.invoke(1, 2, 3));

    // Combinators
    mh = MethodHandles.lookup().findVirtual(VariableArityTester.class, "update",
                                            MethodType.methodType(String.class, boolean[].class));
    assertTrue(mh.isVarargsCollector());
    mh = mh.bindTo(vat);
    assertFalse(mh.isVarargsCollector());
    mh = mh.asVarargsCollector(boolean[].class);
    assertTrue(mh.isVarargsCollector());
    assertEquals("[]", mh.invoke());
    assertEquals("[true, false, true]", mh.invoke(true, false, true));
    assertEquals("[true, false, true]", mh.invoke(new boolean[] { true, false, true}));
    assertEquals("[false, true]", mh.invoke(Boolean.valueOf(false), Boolean.valueOf(true)));
    try {
      mh.invoke(true, true, 0);
      fail();
    } catch (WrongMethodTypeException e) {}
  }

  // The same tests as the above, except that we use use MethodHandles.bind instead of
  // MethodHandle.bindTo.
  public static void testVariableArity_MethodHandles_bind() throws Throwable {
    VariableArityTester vat = new VariableArityTester();
    MethodHandle mh = MethodHandles.lookup().bind(vat, "update",
            MethodType.methodType(String.class, boolean[].class));
    assertTrue(mh.isVarargsCollector());

    assertEquals("[]", mh.invoke());
    assertEquals("[true, false, true]", mh.invoke(true, false, true));
    assertEquals("[true, false, true]", mh.invoke(new boolean[] { true, false, true}));
    assertEquals("[false, true]", mh.invoke(Boolean.valueOf(false), Boolean.valueOf(true)));

    try {
      mh.invoke(true, true, 0);
      fail();
    } catch (WrongMethodTypeException e) {}
  }

  public static void testRevealDirect() throws Throwable {
    // Test with a virtual method :
    MethodType type = MethodType.methodType(String.class);
    MethodHandle handle = MethodHandles.lookup().findVirtual(
        UnreflectTester.class, "publicMethod", type);

    // Comparisons with an equivalent member obtained via reflection :
    MethodHandleInfo info = MethodHandles.lookup().revealDirect(handle);
    Method meth = UnreflectTester.class.getMethod("publicMethod");

    assertEquals(MethodHandleInfo.REF_invokeVirtual, info.getReferenceKind());
    assertEquals("publicMethod", info.getName());
    assertTrue(UnreflectTester.class == info.getDeclaringClass());
    assertFalse(info.isVarArgs());
    assertEquals(meth, info.reflectAs(Method.class, MethodHandles.lookup()));
    assertEquals(type, info.getMethodType());

    // Resolution via a public lookup should fail because the method in question
    // isn't public.
    try {
      info.reflectAs(Method.class, MethodHandles.publicLookup());
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // Test with a static method :
    handle = MethodHandles.lookup().findStatic(UnreflectTester.class,
        "publicStaticMethod",
        MethodType.methodType(String.class));

    info = MethodHandles.lookup().revealDirect(handle);
    meth = UnreflectTester.class.getMethod("publicStaticMethod");
    assertEquals(MethodHandleInfo.REF_invokeStatic, info.getReferenceKind());
    assertEquals("publicStaticMethod", info.getName());
    assertTrue(UnreflectTester.class == info.getDeclaringClass());
    assertFalse(info.isVarArgs());
    assertEquals(meth, info.reflectAs(Method.class, MethodHandles.lookup()));
    assertEquals(type, info.getMethodType());

    // Test with a var-args method :
    type = MethodType.methodType(String.class, String[].class);
    handle = MethodHandles.lookup().findVirtual(UnreflectTester.class,
        "publicVarArgsMethod", type);

    info = MethodHandles.lookup().revealDirect(handle);
    meth = UnreflectTester.class.getMethod("publicVarArgsMethod", String[].class);
    assertEquals(MethodHandleInfo.REF_invokeVirtual, info.getReferenceKind());
    assertEquals("publicVarArgsMethod", info.getName());
    assertTrue(UnreflectTester.class == info.getDeclaringClass());
    assertTrue(info.isVarArgs());
    assertEquals(meth, info.reflectAs(Method.class, MethodHandles.lookup()));
    assertEquals(type, info.getMethodType());

    // Test with a constructor :
    Constructor cons = UnreflectTester.class.getConstructor(String.class, boolean.class);
    type = MethodType.methodType(void.class, String.class, boolean.class);
    handle = MethodHandles.lookup().findConstructor(UnreflectTester.class, type);

    info = MethodHandles.lookup().revealDirect(handle);
    assertEquals(MethodHandleInfo.REF_newInvokeSpecial, info.getReferenceKind());
    assertEquals("<init>", info.getName());
    assertTrue(UnreflectTester.class == info.getDeclaringClass());
    assertFalse(info.isVarArgs());
    assertEquals(cons, info.reflectAs(Constructor.class, MethodHandles.lookup()));
    assertEquals(type, info.getMethodType());

    // Test with a static field :
    Field field = UnreflectTester.class.getField("publicStaticField");

    handle = MethodHandles.lookup().findStaticSetter(
        UnreflectTester.class, "publicStaticField", String.class);

    info = MethodHandles.lookup().revealDirect(handle);
    assertEquals(MethodHandleInfo.REF_putStatic, info.getReferenceKind());
    assertEquals("publicStaticField", info.getName());
    assertTrue(UnreflectTester.class == info.getDeclaringClass());
    assertFalse(info.isVarArgs());
    assertEquals(field, info.reflectAs(Field.class, MethodHandles.lookup()));
    assertEquals(MethodType.methodType(void.class, String.class), info.getMethodType());

    // Test with a setter on the same field, the type of the handle should change
    // but everything else must remain the same.
    handle = MethodHandles.lookup().findStaticGetter(
        UnreflectTester.class, "publicStaticField", String.class);
    info = MethodHandles.lookup().revealDirect(handle);
    assertEquals(MethodHandleInfo.REF_getStatic, info.getReferenceKind());
    assertEquals(field, info.reflectAs(Field.class, MethodHandles.lookup()));
    assertEquals(MethodType.methodType(String.class), info.getMethodType());

    // Test with an instance field :
    field = UnreflectTester.class.getField("publicField");

    handle = MethodHandles.lookup().findSetter(
        UnreflectTester.class, "publicField", String.class);

    info = MethodHandles.lookup().revealDirect(handle);
    assertEquals(MethodHandleInfo.REF_putField, info.getReferenceKind());
    assertEquals("publicField", info.getName());
    assertTrue(UnreflectTester.class == info.getDeclaringClass());
    assertFalse(info.isVarArgs());
    assertEquals(field, info.reflectAs(Field.class, MethodHandles.lookup()));
    assertEquals(MethodType.methodType(void.class, String.class), info.getMethodType());

    // Test with a setter on the same field, the type of the handle should change
    // but everything else must remain the same.
    handle = MethodHandles.lookup().findGetter(
        UnreflectTester.class, "publicField", String.class);
    info = MethodHandles.lookup().revealDirect(handle);
    assertEquals(MethodHandleInfo.REF_getField, info.getReferenceKind());
    assertEquals(field, info.reflectAs(Field.class, MethodHandles.lookup()));
    assertEquals(MethodType.methodType(String.class), info.getMethodType());
  }

  public static void testReflectiveCalls() throws Throwable {
    String[] methodNames = { "invoke", "invokeExact" };
    for (String methodName : methodNames) {
      Method invokeMethod = MethodHandle.class.getMethod(methodName, Object[].class);
      MethodHandle instance =
          MethodHandles.lookup().findVirtual(java.io.PrintStream.class, "println",
                                             MethodType.methodType(void.class, String.class));
      try {
        invokeMethod.invoke(instance, new Object[] { new Object[] { Integer.valueOf(1) } } );
        fail();
      } catch (InvocationTargetException ite) {
        assertEquals(ite.getCause().getClass(), UnsupportedOperationException.class);
      }
    }
  }
}
