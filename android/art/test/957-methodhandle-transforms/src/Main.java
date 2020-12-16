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
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

public class Main {
  public static void main(String[] args) throws Throwable {
    testThrowException();
    testDropArguments();
    testCatchException();
    testGuardWithTest();
    testArrayElementGetter();
    testArrayElementSetter();
    testIdentity();
    testConstant();
    testBindTo();
    testFilterReturnValue();
    testPermuteArguments();
    testInvokers();
    testSpreaders_reference();
    testSpreaders_primitive();
    testInvokeWithArguments();
    testAsCollector();
    testFilterArguments();
    testCollectArguments();
    testInsertArguments();
    testFoldArguments();
  }

  public static void testThrowException() throws Throwable {
    MethodHandle handle = MethodHandles.throwException(String.class,
        IllegalArgumentException.class);

    if (handle.type().returnType() != String.class) {
      fail("Unexpected return type for handle: " + handle +
          " [ " + handle.type() + "]");
    }

    final IllegalArgumentException iae = new IllegalArgumentException("boo!");
    try {
      handle.invoke(iae);
      fail("Expected an exception of type: java.lang.IllegalArgumentException");
    } catch (IllegalArgumentException expected) {
      if (expected != iae) {
        fail("Wrong exception: expected " + iae + " but was " + expected);
      }
    }
  }

  public static void dropArguments_delegate(String message, long message2) {
    System.out.println("Message: " + message + ", Message2: " + message2);
  }

  public static void testDropArguments() throws Throwable {
    MethodHandle delegate = MethodHandles.lookup().findStatic(Main.class,
        "dropArguments_delegate",
        MethodType.methodType(void.class, new Class<?>[] { String.class, long.class }));

    MethodHandle transform = MethodHandles.dropArguments(delegate, 0, int.class, Object.class);

    // The transformer will accept two additional arguments at position zero.
    try {
      transform.invokeExact("foo", 42l);
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    transform.invokeExact(45, new Object(), "foo", 42l);
    transform.invoke(45, new Object(), "foo", 42l);

    // Additional arguments at position 1.
    transform = MethodHandles.dropArguments(delegate, 1, int.class, Object.class);
    transform.invokeExact("foo", 45, new Object(), 42l);
    transform.invoke("foo", 45, new Object(), 42l);

    // Additional arguments at position 2.
    transform = MethodHandles.dropArguments(delegate, 2, int.class, Object.class);
    transform.invokeExact("foo", 42l, 45, new Object());
    transform.invoke("foo", 42l, 45, new Object());

    // Note that we still perform argument conversions even for the arguments that
    // are subsequently dropped.
    try {
      transform.invoke("foo", 42l, 45l, new Object());
      fail();
    } catch (WrongMethodTypeException expected) {
    } catch (IllegalArgumentException expected) {
      // TODO(narayan): We currently throw the wrong type of exception here,
      // it's IAE and should be WMTE instead.
    }

    // Check that asType works as expected.
    transform = MethodHandles.dropArguments(delegate, 0, int.class, Object.class);
    transform = transform.asType(MethodType.methodType(void.class,
          new Class<?>[] { short.class, Object.class, String.class, long.class }));
    transform.invokeExact((short) 45, new Object(), "foo", 42l);

    // Invalid argument location, should not be allowed.
    try {
      MethodHandles.dropArguments(delegate, -1, int.class, Object.class);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // Invalid argument location, should not be allowed.
    try {
      MethodHandles.dropArguments(delegate, 3, int.class, Object.class);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    try {
      MethodHandles.dropArguments(delegate, 1, void.class);
      fail();
    } catch (IllegalArgumentException expected) {
    }
  }

  public static String testCatchException_target(String arg1, long arg2, String exceptionMessage)
      throws Throwable {
    if (exceptionMessage != null) {
      throw new IllegalArgumentException(exceptionMessage);
    }

    System.out.println("Target: Arg1: " + arg1 + ", Arg2: " + arg2);
    return "target";
  }

  public static String testCatchException_handler(IllegalArgumentException iae, String arg1, long arg2,
      String exMsg) {
    System.out.println("Handler: " + iae + ", Arg1: " + arg1 + ", Arg2: " + arg2 + ", ExMsg: " + exMsg);
    return "handler1";
  }

  public static String testCatchException_handler2(IllegalArgumentException iae, String arg1) {
    System.out.println("Handler: " + iae + ", Arg1: " + arg1);
    return "handler2";
  }

  public static void testCatchException() throws Throwable {
    MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
        "testCatchException_target",
        MethodType.methodType(String.class, new Class<?>[] { String.class, long.class, String.class }));

    MethodHandle handler = MethodHandles.lookup().findStatic(Main.class,
        "testCatchException_handler",
        MethodType.methodType(String.class, new Class<?>[] { IllegalArgumentException.class,
            String.class, long.class, String.class }));

    MethodHandle adapter = MethodHandles.catchException(target, IllegalArgumentException.class,
        handler);

    String returnVal = null;

    // These two should end up calling the target always. We're passing a null exception
    // message here, which means the target will not throw.
    returnVal = (String) adapter.invoke("foo", 42, null);
    assertEquals("target", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, (String) null);
    assertEquals("target", returnVal);

    // We're passing a non-null exception message here, which means the target will throw,
    // which in turn means that the handler must be called for the next two invokes.
    returnVal = (String) adapter.invoke("foo", 42, "exceptionMessage");
    assertEquals("handler1", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, "exceptionMessage");
    assertEquals("handler1", returnVal);

    handler = MethodHandles.lookup().findStatic(Main.class,
        "testCatchException_handler2",
        MethodType.methodType(String.class, new Class<?>[] { IllegalArgumentException.class,
            String.class }));
    adapter = MethodHandles.catchException(target, IllegalArgumentException.class, handler);

    returnVal = (String) adapter.invoke("foo", 42, "exceptionMessage");
    assertEquals("handler2", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, "exceptionMessage");
    assertEquals("handler2", returnVal);

    // Test that the type of the invoke doesn't matter. Here we call
    // IllegalArgumentException.toString() on the exception that was thrown by
    // the target.
    handler = MethodHandles.lookup().findVirtual(IllegalArgumentException.class,
        "toString", MethodType.methodType(String.class));
    adapter = MethodHandles.catchException(target, IllegalArgumentException.class, handler);

    returnVal = (String) adapter.invoke("foo", 42, "exceptionMessage");
    assertEquals("java.lang.IllegalArgumentException: exceptionMessage", returnVal);
    returnVal = (String) adapter.invokeExact("foo", 42l, "exceptionMessage2");
    assertEquals("java.lang.IllegalArgumentException: exceptionMessage2", returnVal);

    // Check that asType works as expected.
    adapter = MethodHandles.catchException(target, IllegalArgumentException.class,
        handler);
    adapter = adapter.asType(MethodType.methodType(String.class,
          new Class<?>[] { String.class, int.class, String.class }));
    returnVal = (String) adapter.invokeExact("foo", 42, "exceptionMessage");
    assertEquals("java.lang.IllegalArgumentException: exceptionMessage", returnVal);
  }

  public static boolean testGuardWithTest_test(String arg1, long arg2) {
    return "target".equals(arg1) && 42 == arg2;
  }

  public static String testGuardWithTest_target(String arg1, long arg2, int arg3) {
    System.out.println("target: " + arg1 + ", " + arg2  + ", " + arg3);
    return "target";
  }

  public static String testGuardWithTest_fallback(String arg1, long arg2, int arg3) {
    System.out.println("fallback: " + arg1 + ", " + arg2  + ", " + arg3);
    return "fallback";
  }

  public static void testGuardWithTest() throws Throwable {
    MethodHandle test = MethodHandles.lookup().findStatic(Main.class,
        "testGuardWithTest_test",
        MethodType.methodType(boolean.class, new Class<?>[] { String.class, long.class }));

    final MethodType type = MethodType.methodType(String.class,
        new Class<?>[] { String.class, long.class, int.class });

    final MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
        "testGuardWithTest_target", type);
    final MethodHandle fallback = MethodHandles.lookup().findStatic(Main.class,
        "testGuardWithTest_fallback", type);

    MethodHandle adapter = MethodHandles.guardWithTest(test, target, fallback);

    String returnVal = null;

    returnVal = (String) adapter.invoke("target", 42, 56);
    assertEquals("target", returnVal);
    returnVal = (String) adapter.invokeExact("target", 42l, 56);
    assertEquals("target", returnVal);

    returnVal = (String) adapter.invoke("fallback", 42l, 56);
    assertEquals("fallback", returnVal);
    returnVal = (String) adapter.invokeExact("target", 42l, 56);
    assertEquals("target", returnVal);

    // Check that asType works as expected.
    adapter = adapter.asType(MethodType.methodType(String.class,
          new Class<?>[] { String.class, int.class, int.class }));
    returnVal = (String) adapter.invokeExact("target", 42, 56);
    assertEquals("target", returnVal);
  }

  public static void testArrayElementGetter() throws Throwable {
    MethodHandle getter = MethodHandles.arrayElementGetter(int[].class);

    {
      int[] array = new int[1];
      array[0] = 42;
      int value = (int) getter.invoke(array, 0);
      if (value != 42) {
        fail("Unexpected value: " + value);
      }

      try {
        value = (int) getter.invoke(array, -1);
        fail();
      } catch (ArrayIndexOutOfBoundsException expected) {
      }

      try {
        value = (int) getter.invoke(null, -1);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(long[].class);
      long[] array = new long[1];
      array[0] = 42;
      long value = (long) getter.invoke(array, 0);
      if (value != 42l) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(short[].class);
      short[] array = new short[1];
      array[0] = 42;
      short value = (short) getter.invoke(array, 0);
      if (value != 42l) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(char[].class);
      char[] array = new char[1];
      array[0] = 42;
      char value = (char) getter.invoke(array, 0);
      if (value != 42l) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(byte[].class);
      byte[] array = new byte[1];
      array[0] = (byte) 0x8;
      byte value = (byte) getter.invoke(array, 0);
      if (value != (byte) 0x8) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(boolean[].class);
      boolean[] array = new boolean[1];
      array[0] = true;
      boolean value = (boolean) getter.invoke(array, 0);
      if (!value) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(float[].class);
      float[] array = new float[1];
      array[0] = 42.0f;
      float value = (float) getter.invoke(array, 0);
      if (value != 42.0f) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(double[].class);
      double[] array = new double[1];
      array[0] = 42.0;
      double value = (double) getter.invoke(array, 0);
      if (value != 42.0) {
        fail("Unexpected value: " + value);
      }
    }

    {
      getter = MethodHandles.arrayElementGetter(String[].class);
      String[] array = new String[3];
      array[0] = "42";
      array[1] = "48";
      array[2] = "54";
      String value = (String) getter.invoke(array, 0);
      assertEquals("42", value);
      value = (String) getter.invoke(array, 1);
      assertEquals("48", value);
      value = (String) getter.invoke(array, 2);
      assertEquals("54", value);
    }
  }

  public static void testArrayElementSetter() throws Throwable {
    MethodHandle setter = MethodHandles.arrayElementSetter(int[].class);

    {
      int[] array = new int[2];
      setter.invoke(array, 0, 42);
      setter.invoke(array, 1, 43);

      if (array[0] != 42) {
        fail("Unexpected value: " + array[0]);
      }
      if (array[1] != 43) {
        fail("Unexpected value: " + array[1]);
      }

      try {
        setter.invoke(array, -1, 42);
        fail();
      } catch (ArrayIndexOutOfBoundsException expected) {
      }

      try {
        setter.invoke(null, 0, 42);
        fail();
      } catch (NullPointerException expected) {
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(long[].class);
      long[] array = new long[1];
      setter.invoke(array, 0, 42l);
      if (array[0] != 42l) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(short[].class);
      short[] array = new short[1];
      setter.invoke(array, 0, (short) 42);
      if (array[0] != 42l) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(char[].class);
      char[] array = new char[1];
      setter.invoke(array, 0, (char) 42);
      if (array[0] != 42) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(byte[].class);
      byte[] array = new byte[1];
      setter.invoke(array, 0, (byte) 0x8);
      if (array[0] != (byte) 0x8) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(boolean[].class);
      boolean[] array = new boolean[1];
      setter.invoke(array, 0, true);
      if (!array[0]) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(float[].class);
      float[] array = new float[1];
      setter.invoke(array, 0, 42.0f);
      if (array[0] != 42.0f) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(double[].class);
      double[] array = new double[1];
      setter.invoke(array, 0, 42.0);
      if (array[0] != 42.0) {
        fail("Unexpected value: " + array[0]);
      }
    }

    {
      setter = MethodHandles.arrayElementSetter(String[].class);
      String[] array = new String[3];
      setter.invoke(array, 0, "42");
      setter.invoke(array, 1, "48");
      setter.invoke(array, 2, "54");
      assertEquals("42", array[0]);
      assertEquals("48", array[1]);
      assertEquals("54", array[2]);
    }
  }

  public static void testIdentity() throws Throwable {
    {
      MethodHandle identity = MethodHandles.identity(boolean.class);
      boolean value = (boolean) identity.invoke(false);
      if (value) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(byte.class);
      byte value = (byte) identity.invoke((byte) 0x8);
      if (value != (byte) 0x8) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(char.class);
      char value = (char) identity.invoke((char) -56);
      if (value != (char) -56) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(short.class);
      short value = (short) identity.invoke((short) -59);
      if (value != (short) -59) {
        fail("Unexpected value: " + Short.toString(value));
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(int.class);
      int value = (int) identity.invoke(52);
      if (value != 52) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(long.class);
      long value = (long) identity.invoke(-76l);
      if (value != (long) -76) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(float.class);
      float value = (float) identity.invoke(56.0f);
      if (value != (float) 56.0f) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(double.class);
      double value = (double) identity.invoke((double) 72.0);
      if (value != (double) 72.0) {
        fail("Unexpected value: " + value);
      }
    }

    {
      MethodHandle identity = MethodHandles.identity(String.class);
      String value = (String) identity.invoke("bazman");
      assertEquals("bazman", value);
    }
  }

  public static void testConstant() throws Throwable {
    // int constants.
    {
      MethodHandle constant = MethodHandles.constant(int.class, 56);
      int value = (int) constant.invoke();
      if (value != 56) {
        fail("Unexpected value: " + value);
      }

      // short constant values are converted to int.
      constant = MethodHandles.constant(int.class, (short) 52);
      value = (int) constant.invoke();
      if (value != 52) {
        fail("Unexpected value: " + value);
      }

      // char constant values are converted to int.
      constant = MethodHandles.constant(int.class, (char) 'b');
      value = (int) constant.invoke();
      if (value != (int) 'b') {
        fail("Unexpected value: " + value);
      }

      // int constant values are converted to int.
      constant = MethodHandles.constant(int.class, (byte) 0x1);
      value = (int) constant.invoke();
      if (value != 1) {
        fail("Unexpected value: " + value);
      }

      // boolean, float, double and long primitive constants are not convertible
      // to int, so the handle creation must fail with a CCE.
      try {
        MethodHandles.constant(int.class, false);
        fail();
      } catch (ClassCastException expected) {
      }

      try {
        MethodHandles.constant(int.class, 0.1f);
        fail();
      } catch (ClassCastException expected) {
      }

      try {
        MethodHandles.constant(int.class, 0.2);
        fail();
      } catch (ClassCastException expected) {
      }

      try {
        MethodHandles.constant(int.class, 73l);
        fail();
      } catch (ClassCastException expected) {
      }
    }

    // long constants.
    {
      MethodHandle constant = MethodHandles.constant(long.class, 56l);
      long value = (long) constant.invoke();
      if (value != 56l) {
        fail("Unexpected value: " + value);
      }

      constant = MethodHandles.constant(long.class, (int) 56);
      value = (long) constant.invoke();
      if (value != 56l) {
        fail("Unexpected value: " + value);
      }
    }

    // byte constants.
    {
      MethodHandle constant = MethodHandles.constant(byte.class, (byte) 0x12);
      byte value = (byte) constant.invoke();
      if (value != (byte) 0x12) {
        fail("Unexpected value: " + value);
      }
    }

    // boolean constants.
    {
      MethodHandle constant = MethodHandles.constant(boolean.class, true);
      boolean value = (boolean) constant.invoke();
      if (!value) {
        fail("Unexpected value: " + value);
      }
    }

    // char constants.
    {
      MethodHandle constant = MethodHandles.constant(char.class, 'f');
      char value = (char) constant.invoke();
      if (value != 'f') {
        fail("Unexpected value: " + value);
      }
    }

    // short constants.
    {
      MethodHandle constant = MethodHandles.constant(short.class, (short) 123);
      short value = (short) constant.invoke();
      if (value != (short) 123) {
        fail("Unexpected value: " + value);
      }
    }

    // float constants.
    {
      MethodHandle constant = MethodHandles.constant(float.class, 56.0f);
      float value = (float) constant.invoke();
      if (value != 56.0f) {
        fail("Unexpected value: " + value);
      }
    }

    // double constants.
    {
      MethodHandle constant = MethodHandles.constant(double.class, 256.0);
      double value = (double) constant.invoke();
      if (value != 256.0) {
        fail("Unexpected value: " + value);
      }
    }

    // reference constants.
    {
      MethodHandle constant = MethodHandles.constant(String.class, "256.0");
      String value = (String) constant.invoke();
      assertEquals("256.0", value);
    }
  }

  public static void testBindTo() throws Throwable {
    MethodHandle stringCharAt = MethodHandles.lookup().findVirtual(
        String.class, "charAt", MethodType.methodType(char.class, int.class));

    char value = (char) stringCharAt.invoke("foo", 0);
    if (value != 'f') {
      fail("Unexpected value: " + value);
    }

    MethodHandle bound = stringCharAt.bindTo("foo");
    value = (char) bound.invoke(0);
    if (value != 'f') {
      fail("Unexpected value: " + value);
    }

    try {
      stringCharAt.bindTo(new Object());
      fail();
    } catch (ClassCastException expected) {
    }

    bound = stringCharAt.bindTo(null);
    try {
      bound.invoke(0);
      fail();
    } catch (NullPointerException expected) {
    }

    MethodHandle integerParseInt = MethodHandles.lookup().findStatic(
        Integer.class, "parseInt", MethodType.methodType(int.class, String.class));

    bound = integerParseInt.bindTo("78452");
    int intValue = (int) bound.invoke();
    if (intValue != 78452) {
      fail("Unexpected value: " + intValue);
    }
  }

  public static String filterReturnValue_target(int a) {
    return "ReturnValue" + a;
  }

  public static boolean filterReturnValue_filter(String value) {
    return value.indexOf("42") != -1;
  }

  public static int filterReturnValue_intTarget(String a) {
    return Integer.parseInt(a);
  }

  public static int filterReturnValue_intFilter(int b) {
    return b + 1;
  }

  public static void filterReturnValue_voidTarget() {
  }

  public static int filterReturnValue_voidFilter() {
    return 42;
  }

  public static void testFilterReturnValue() throws Throwable {
    // A target that returns a reference.
    {
      final MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
          "filterReturnValue_target", MethodType.methodType(String.class, int.class));
      final MethodHandle filter = MethodHandles.lookup().findStatic(Main.class,
          "filterReturnValue_filter", MethodType.methodType(boolean.class, String.class));

      MethodHandle adapter = MethodHandles.filterReturnValue(target, filter);

      boolean value = (boolean) adapter.invoke((int) 42);
      if (!value) {
        fail("Unexpected value: " + value);
      }
      value = (boolean) adapter.invoke((int) 43);
      if (value) {
        fail("Unexpected value: " + value);
      }
    }

    // A target that returns a primitive.
    {
      final MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
          "filterReturnValue_intTarget", MethodType.methodType(int.class, String.class));
      final MethodHandle filter = MethodHandles.lookup().findStatic(Main.class,
          "filterReturnValue_intFilter", MethodType.methodType(int.class, int.class));

      MethodHandle adapter = MethodHandles.filterReturnValue(target, filter);

      int value = (int) adapter.invoke("56");
      if (value != 57) {
        fail("Unexpected value: " + value);
      }
    }

    // A target that returns void.
    {
      final MethodHandle target = MethodHandles.lookup().findStatic(Main.class,
          "filterReturnValue_voidTarget", MethodType.methodType(void.class));
      final MethodHandle filter = MethodHandles.lookup().findStatic(Main.class,
          "filterReturnValue_voidFilter", MethodType.methodType(int.class));

      MethodHandle adapter = MethodHandles.filterReturnValue(target, filter);

      int value = (int) adapter.invoke();
      if (value != 42) {
        fail("Unexpected value: " + value);
      }
    }
  }

  public static void permuteArguments_callee(boolean a, byte b, char c,
      short d, int e, long f, float g, double h) {
    if (a == true && b == (byte) 'b' && c == 'c' && d == (short) 56 &&
        e == 78 && f == (long) 97 && g == 98.0f && f == 97.0) {
      return;
    }

    fail("Unexpected arguments: " + a + ", " + b + ", " + c
        + ", " + d + ", " + e + ", " + f + ", " + g + ", " + h);
  }

  public static void permuteArguments_boxingCallee(boolean a, Integer b) {
    if (a && b.intValue() == 42) {
      return;
    }

    fail("Unexpected arguments: " + a + ", " + b);
  }

  public static void testPermuteArguments() throws Throwable {
    {
      final MethodHandle target = MethodHandles.lookup().findStatic(
          Main.class, "permuteArguments_callee",
          MethodType.methodType(void.class, new Class<?>[] {
            boolean.class, byte.class, char.class, short.class, int.class,
            long.class, float.class, double.class }));

      final MethodType newType = MethodType.methodType(void.class, new Class<?>[] {
        double.class, float.class, long.class, int.class, short.class, char.class,
        byte.class, boolean.class });

      final MethodHandle permutation = MethodHandles.permuteArguments(
          target, newType, new int[] { 7, 6, 5, 4, 3, 2, 1, 0 });

      permutation.invoke((double) 97.0, (float) 98.0f, (long) 97, 78,
          (short) 56, 'c', (byte) 'b', (boolean) true);

      // The permutation array was not of the right length.
      try {
        MethodHandles.permuteArguments(target, newType,
            new int[] { 7 });
        fail();
      } catch (IllegalArgumentException expected) {
      }

      // The permutation array has an element that's out of bounds
      // (there's no argument with idx == 8).
      try {
        MethodHandles.permuteArguments(target, newType,
            new int[] { 8, 6, 5, 4, 3, 2, 1, 0 });
        fail();
      } catch (IllegalArgumentException expected) {
      }

      // The permutation array maps to an incorrect type.
      try {
        MethodHandles.permuteArguments(target, newType,
            new int[] { 7, 7, 5, 4, 3, 2, 1, 0 });
        fail();
      } catch (IllegalArgumentException expected) {
      }
    }

    // Tests for reference arguments as well as permutations that
    // repeat arguments.
    {
      final MethodHandle target = MethodHandles.lookup().findVirtual(
          String.class, "concat", MethodType.methodType(String.class, String.class));

      final MethodType newType = MethodType.methodType(String.class, String.class,
          String.class);

      assertEquals("foobar", (String) target.invoke("foo", "bar"));

      MethodHandle permutation = MethodHandles.permuteArguments(target,
          newType, new int[] { 1, 0 });
      assertEquals("barfoo", (String) permutation.invoke("foo", "bar"));

      permutation = MethodHandles.permuteArguments(target, newType, new int[] { 0, 0 });
      assertEquals("foofoo", (String) permutation.invoke("foo", "bar"));

      permutation = MethodHandles.permuteArguments(target, newType, new int[] { 1, 1 });
      assertEquals("barbar", (String) permutation.invoke("foo", "bar"));
    }

    // Tests for boxing and unboxing.
    {
      final MethodHandle target = MethodHandles.lookup().findStatic(
          Main.class, "permuteArguments_boxingCallee",
          MethodType.methodType(void.class, new Class<?>[] { boolean.class, Integer.class }));

      final MethodType newType = MethodType.methodType(void.class,
          new Class<?>[] { Integer.class, boolean.class });

      MethodHandle permutation = MethodHandles.permuteArguments(target,
          newType, new int[] { 1, 0 });

      permutation.invoke(42, true);
      permutation.invoke(42, Boolean.TRUE);
      permutation.invoke(Integer.valueOf(42), true);
      permutation.invoke(Integer.valueOf(42), Boolean.TRUE);
    }
  }

  private static Object returnBar() {
    return "bar";
  }

  public static void testInvokers() throws Throwable {
    final MethodType targetType = MethodType.methodType(String.class, String.class);
    final MethodHandle target = MethodHandles.lookup().findVirtual(
        String.class, "concat", targetType);

    MethodHandle invoker = MethodHandles.invoker(target.type());
    assertEquals("barbar", (String) invoker.invoke(target, "bar", "bar"));
    assertEquals("barbar", (String) invoker.invoke(target, (Object) returnBar(), "bar"));
    try {
      String foo = (String) invoker.invoke(target, "bar", "bar", 24);
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    MethodHandle exactInvoker = MethodHandles.exactInvoker(target.type());
    assertEquals("barbar", (String) exactInvoker.invoke(target, "bar", "bar"));
    try {
      String foo = (String) exactInvoker.invoke(target, (Object) returnBar(), "bar");
      fail();
    } catch (WrongMethodTypeException expected) {
    }
    try {
      String foo = (String) exactInvoker.invoke(target, "bar", "bar", 24);
      fail();
    } catch (WrongMethodTypeException expected) {
    }
  }

  public static int spreadReferences(String a, String b, String c) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c);
    return 42;
  }

  public static int spreadReferences_Unbox(String a, int b) {
    System.out.println("a: " + a + ", b:" + b);
    return 43;
  }

  public static void testSpreaders_reference() throws Throwable {
    MethodType methodType = MethodType.methodType(int.class,
        new Class<?>[] { String.class, String.class, String.class });
    MethodHandle delegate = MethodHandles.lookup().findStatic(
        Main.class, "spreadReferences", methodType);

    // Basic checks on array lengths.
    //
    // Array size = 0
    MethodHandle mhAsSpreader = delegate.asSpreader(String[].class, 0);
    int ret = (int) mhAsSpreader.invoke("a", "b", "c", new String[] {});
    assertEquals(42, ret);
    // Array size = 1
    mhAsSpreader = delegate.asSpreader(String[].class, 1);
    ret = (int) mhAsSpreader.invoke("a", "b", new String[] { "c" });
    assertEquals(42, ret);
    // Array size = 2
    mhAsSpreader = delegate.asSpreader(String[].class, 2);
    ret = (int) mhAsSpreader.invoke("a", new String[] { "b", "c" });
    assertEquals(42, ret);
    // Array size = 3
    mhAsSpreader = delegate.asSpreader(String[].class, 3);
    ret = (int) mhAsSpreader.invoke(new String[] { "a", "b", "c"});
    assertEquals(42, ret);

    // Exception case, array size = 4 is illegal.
    try {
      delegate.asSpreader(String[].class, 4);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // Exception case, calling with an arg of the wrong size.
    // Array size = 3
    mhAsSpreader = delegate.asSpreader(String[].class, 3);
    try {
      ret = (int) mhAsSpreader.invoke(new String[] { "a", "b"});
    } catch (IllegalArgumentException expected) {
    }

    // Various other hijinks, pass as Object[] arrays, Object etc.
    mhAsSpreader = delegate.asSpreader(Object[].class, 2);
    ret = (int) mhAsSpreader.invoke("a", new String[] { "b", "c" });
    assertEquals(42, ret);

    mhAsSpreader = delegate.asSpreader(Object[].class, 2);
    ret = (int) mhAsSpreader.invoke("a", new Object[] { "b", "c" });
    assertEquals(42, ret);

    mhAsSpreader = delegate.asSpreader(Object[].class, 2);
    ret = (int) mhAsSpreader.invoke("a", (Object) new Object[] { "b", "c" });
    assertEquals(42, ret);

    // Test implicit unboxing.
    MethodType methodType2 = MethodType.methodType(int.class,
        new Class<?>[] { String.class, int.class });
    MethodHandle delegate2 = MethodHandles.lookup().findStatic(
        Main.class, "spreadReferences_Unbox", methodType2);

    // .. with an Integer[] array.
    mhAsSpreader = delegate2.asSpreader(Integer[].class, 1);
    ret = (int) mhAsSpreader.invoke("a", new Integer[] { 43 });
    assertEquals(43, ret);

    // .. with an Integer[] array declared as an Object[] argument type.
    mhAsSpreader = delegate2.asSpreader(Object[].class, 1);
    ret = (int) mhAsSpreader.invoke("a", new Integer[] { 43 });
    assertEquals(43, ret);

    // .. with an Object[] array.
    mhAsSpreader = delegate2.asSpreader(Object[].class, 1);
    ret = (int) mhAsSpreader.invoke("a", new Object[] { Integer.valueOf(43)});
    assertEquals(43, ret);

    // -- Part 2--
    // Run a subset of these tests on MethodHandles.spreadInvoker, which only accepts
    // a trailing argument type of Object[].
    MethodHandle spreadInvoker = MethodHandles.spreadInvoker(methodType2, 1);
    ret = (int) spreadInvoker.invoke(delegate2, "a", new Object[] { Integer.valueOf(43)});
    assertEquals(43, ret);

    ret = (int) spreadInvoker.invoke(delegate2, "a", new Integer[] { 43 });
    assertEquals(43, ret);

    // NOTE: Annoyingly, the second argument here is leadingArgCount and not
    // arrayLength.
    spreadInvoker = MethodHandles.spreadInvoker(methodType, 3);
    ret = (int) spreadInvoker.invoke(delegate, "a", "b", "c", new String[] {});
    assertEquals(42, ret);

    spreadInvoker = MethodHandles.spreadInvoker(methodType, 0);
    ret = (int) spreadInvoker.invoke(delegate, new String[] { "a", "b", "c" });
    assertEquals(42, ret);

    // Exact invokes: Double check that the expected parameter type is
    // Object[] and not T[].
    try {
      spreadInvoker.invokeExact(delegate, new String[] { "a", "b", "c" });
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    ret = (int) spreadInvoker.invoke(delegate, new Object[] { "a", "b", "c" });
    assertEquals(42, ret);
  }

  public static int spreadBoolean(String a, Boolean b, boolean c) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c);
    return 44;
  }

  public static int spreadByte(String a, Byte b, byte c,
      short d, int e, long f, float g, double h) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c +
        ", d: " + d + ", e: " + e + ", f:" + f + ", g: " + g +
        ", h: " + h);
    return 45;
  }

  public static int spreadChar(String a, Character b, char c,
      int d, long e, float f, double g) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c +
        ", d: " + d + ", e: " + e + ", f:" + f + ", g: " + g);
    return 46;
  }

  public static int spreadShort(String a, Short b, short c,
      int d, long e, float f, double g) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c +
        ", d: " + d + ", e: " + e + ", f:" + f + ", g:" + g);
    return 47;
  }

  public static int spreadInt(String a, Integer b, int c,
      long d, float e, double f) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c +
        ", d: " + d + ", e: " + e + ", f:" + f);
    return 48;
  }

  public static int spreadLong(String a, Long b, long c, float d, double e) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c +
        ", d: " + d + ", e: " + e);
    return 49;
  }

  public static int spreadFloat(String a, Float b, float c, double d) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c + ", d: " + d);
    return 50;
  }

  public static int spreadDouble(String a, Double b, double c) {
    System.out.println("a: " + a + ", b:" + b + ", c: " + c);
    return 51;
  }

  public static void testSpreaders_primitive() throws Throwable {
    // boolean[]
    // ---------------------
    MethodType type = MethodType.methodType(int.class,
        new Class<?>[] { String.class, Boolean.class, boolean.class });
    MethodHandle delegate = MethodHandles.lookup().findStatic(
        Main.class, "spreadBoolean", type);

    MethodHandle spreader = delegate.asSpreader(boolean[].class, 2);
    int ret = (int) spreader.invokeExact("a", new boolean[] { true, false });
    assertEquals(44, ret);
    ret = (int) spreader.invoke("a", new boolean[] { true, false });
    assertEquals(44, ret);

    // boolean can't be cast to String (the first argument to the method).
    try {
      delegate.asSpreader(boolean[].class, 3);
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // int can't be cast to boolean to supply the last argument to the method.
    try {
      delegate.asSpreader(int[].class, 1);
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // byte[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] {
          String.class, Byte.class, byte.class,
          short.class, int.class, long.class,
          float.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadByte", type);

    spreader = delegate.asSpreader(byte[].class, 7);
    ret = (int) spreader.invokeExact("a",
        new byte[] { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 });
    assertEquals(45, ret);
    ret = (int) spreader.invoke("a",
        new byte[] { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 });
    assertEquals(45, ret);

    // char[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] {
          String.class, Character.class,char.class,
          int.class, long.class, float.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadChar", type);

    spreader = delegate.asSpreader(char[].class, 6);
    ret = (int) spreader.invokeExact("a",
        new char[] { '1', '2', '3', '4', '5', '6' });
    assertEquals(46, ret);
    ret = (int) spreader.invokeExact("a",
        new char[] { '1', '2', '3', '4', '5', '6' });
    assertEquals(46, ret);

    // short[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] {
          String.class, Short.class, short.class,
          int.class, long.class, float.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadShort", type);

    spreader = delegate.asSpreader(short[].class, 6);
    ret = (int) spreader.invokeExact("a",
        new short[] { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6 });
    assertEquals(47, ret);
    ret = (int) spreader.invoke("a",
        new short[] { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6 });
    assertEquals(47, ret);

    // int[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] {
          String.class, Integer.class, int.class,
          long.class, float.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadInt", type);

    spreader = delegate.asSpreader(int[].class, 5);
    ret = (int) spreader.invokeExact("a", new int[] { 1, 2, 3, 4, 5 });
    assertEquals(48, ret);
    ret = (int) spreader.invokeExact("a", new int[] { 1, 2, 3, 4, 5 });
    assertEquals(48, ret);

    // long[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] {
          String.class, Long.class, long.class, float.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadLong", type);

    spreader = delegate.asSpreader(long[].class, 4);
    ret = (int) spreader.invokeExact("a",
        new long[] { 0x1, 0x2, 0x3, 0x4 });
    assertEquals(49, ret);
    ret = (int) spreader.invoke("a",
        new long[] { 0x1, 0x2, 0x3, 0x4 });
    assertEquals(49, ret);

    // float[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] {
          String.class, Float.class, float.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadFloat", type);

    spreader = delegate.asSpreader(float[].class, 3);
    ret = (int) spreader.invokeExact("a",
        new float[] { 1.0f, 2.0f, 3.0f });
    assertEquals(50, ret);
    ret = (int) spreader.invokeExact("a",
        new float[] { 1.0f, 2.0f, 3.0f });
    assertEquals(50, ret);

    // double[]
    // ---------------------
    type = MethodType.methodType(int.class,
        new Class<?>[] { String.class, Double.class, double.class });
    delegate = MethodHandles.lookup().findStatic(Main.class, "spreadDouble", type);

    spreader = delegate.asSpreader(double[].class, 2);
    ret = (int) spreader.invokeExact("a", new double[] { 1.0, 2.0 });
    assertEquals(51, ret);
    ret = (int) spreader.invokeExact("a", new double[] { 1.0, 2.0 });
    assertEquals(51, ret);
  }

  public static void testInvokeWithArguments() throws Throwable {
    MethodType methodType = MethodType.methodType(int.class,
        new Class<?>[] { String.class, String.class, String.class });
    MethodHandle handle = MethodHandles.lookup().findStatic(
        Main.class, "spreadReferences", methodType);

    Object ret = handle.invokeWithArguments(new Object[] { "a", "b", "c"});
    assertEquals(42, (int) ret);
    handle.invokeWithArguments(new String[] { "a", "b", "c" });
    assertEquals(42, (int) ret);

    // Pass in an array that's too small. Should throw an IAE.
    try {
      handle.invokeWithArguments(new Object[] { "a", "b" });
      fail();
    } catch (IllegalArgumentException expected) {
    } catch (WrongMethodTypeException expected) {
    }

    // Test implicit unboxing.
    MethodType methodType2 = MethodType.methodType(int.class,
        new Class<?>[] { String.class, int.class });
    MethodHandle handle2 = MethodHandles.lookup().findStatic(
        Main.class, "spreadReferences_Unbox", methodType2);

    ret = (int) handle2.invokeWithArguments(new Object[] { "a", 43 });
    assertEquals(43, (int) ret);
  }

  public static int collectBoolean(String a, boolean[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 44;
  }

  public static int collectByte(String a, byte[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 45;
  }

  public static int collectChar(String a, char[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 46;
  }

  public static int collectShort(String a, short[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 47;
  }

  public static int collectInt(String a, int[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 48;
  }

  public static int collectLong(String a, long[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 49;
  }

  public static int collectFloat(String a, float[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 50;
  }

  public static int collectDouble(String a, double[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 51;
  }

  public static int collectCharSequence(String a, CharSequence[] b) {
    System.out.println("a: " + a + ", b:" + b[0] + ", c: " + b[1]);
    return 99;
  }

  public static void testAsCollector() throws Throwable {
    // Reference arrays.
    // -------------------
    MethodHandle trailingRef = MethodHandles.lookup().findStatic(
        Main.class, "collectCharSequence",
        MethodType.methodType(int.class, String.class, CharSequence[].class));

    // int[] is not convertible to CharSequence[].class.
    try {
      trailingRef.asCollector(int[].class, 1);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // Object[] is not convertible to CharSequence[].class.
    try {
      trailingRef.asCollector(Object[].class, 1);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // String[].class is convertible to CharSequence.class
    MethodHandle collector = trailingRef.asCollector(String[].class, 2);
    assertEquals(99, (int) collector.invoke("a", "b", "c"));

    // Too few arguments should fail with a WMTE.
    try {
      collector.invoke("a", "b");
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // Too many arguments should fail with a WMTE.
    try {
      collector.invoke("a", "b", "c", "d");
      fail();
    } catch (WrongMethodTypeException expected) {
    }

    // Sanity checks on other array types.

    MethodHandle target = MethodHandles.lookup().findStatic(
        Main.class, "collectBoolean",
        MethodType.methodType(int.class, String.class, boolean[].class));
    assertEquals(44, (int) target.asCollector(boolean[].class, 2).invoke("a", true, false));

    target = MethodHandles.lookup().findStatic(Main.class, "collectByte",
        MethodType.methodType(int.class, String.class, byte[].class));
    assertEquals(45, (int) target.asCollector(byte[].class, 2).invoke("a", (byte) 1, (byte) 2));

    target = MethodHandles.lookup().findStatic(Main.class, "collectChar",
        MethodType.methodType(int.class, String.class, char[].class));
    assertEquals(46, (int) target.asCollector(char[].class, 2).invoke("a", 'a', 'b'));

    target = MethodHandles.lookup().findStatic(Main.class, "collectShort",
        MethodType.methodType(int.class, String.class, short[].class));
    assertEquals(47, (int) target.asCollector(short[].class, 2).invoke("a", (short) 3, (short) 4));

    target = MethodHandles.lookup().findStatic(Main.class, "collectInt",
        MethodType.methodType(int.class, String.class, int[].class));
    assertEquals(48, (int) target.asCollector(int[].class, 2).invoke("a", 42, 43));

    target = MethodHandles.lookup().findStatic(Main.class, "collectLong",
        MethodType.methodType(int.class, String.class, long[].class));
    assertEquals(49, (int) target.asCollector(long[].class, 2).invoke("a", 100, 99));

    target = MethodHandles.lookup().findStatic(Main.class, "collectFloat",
        MethodType.methodType(int.class, String.class, float[].class));
    assertEquals(50, (int) target.asCollector(float[].class, 2).invoke("a", 8.9f, 9.1f));

    target = MethodHandles.lookup().findStatic(Main.class, "collectDouble",
        MethodType.methodType(int.class, String.class, double[].class));
    assertEquals(51, (int) target.asCollector(double[].class, 2).invoke("a", 6.7, 7.8));
  }

  public static String filter1(char a) {
    return String.valueOf(a);
  }

  public static char filter2(String b) {
    return b.charAt(0);
  }

  public static String badFilter1(char a, char b) {
    return "bad";
  }

  public static int filterTarget(String a, char b, String c, char d) {
    System.out.println("a: " + a + ", b: " + b + ", c:" + c + ", d:" + d);
    return 56;
  }

  public static void testFilterArguments() throws Throwable {
    MethodHandle filter1 = MethodHandles.lookup().findStatic(
        Main.class, "filter1", MethodType.methodType(String.class, char.class));
    MethodHandle filter2 = MethodHandles.lookup().findStatic(
        Main.class, "filter2", MethodType.methodType(char.class, String.class));

    MethodHandle target = MethodHandles.lookup().findStatic(
        Main.class, "filterTarget", MethodType.methodType(int.class,
          String.class, char.class, String.class, char.class));

    // In all the cases below, the values printed will be 'a', 'b', 'c', 'd'.

    // Filter arguments [0, 1] - all other arguments are passed through
    // as is.
    MethodHandle adapter = MethodHandles.filterArguments(
        target, 0, filter1, filter2);
    assertEquals(56, (int) adapter.invokeExact('a', "bXXXX", "c", 'd'));

    // Filter arguments [1, 2].
    adapter = MethodHandles.filterArguments(target, 1, filter2, filter1);
    assertEquals(56, (int) adapter.invokeExact("a", "bXXXX", 'c', 'd'));

    // Filter arguments [2, 3].
    adapter = MethodHandles.filterArguments(target, 2, filter1, filter2);
    assertEquals(56, (int) adapter.invokeExact("a", 'b', 'c', "dXXXXX"));

    // Try out a few error cases :

    // The return types of the filter doesn't align with the expected argument
    // type of the target.
    try {
      adapter = MethodHandles.filterArguments(target, 2, filter2, filter1);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // There are more filters than arguments.
    try {
      adapter = MethodHandles.filterArguments(target, 3, filter2, filter1);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    // We pass in an obviously bogus position.
    try {
      adapter = MethodHandles.filterArguments(target, -1, filter2, filter1);
      fail();
    } catch (ArrayIndexOutOfBoundsException expected) {
    }

    // We pass in a function that has more than one argument.
    MethodHandle badFilter1 = MethodHandles.lookup().findStatic(
        Main.class, "badFilter1",
        MethodType.methodType(String.class, char.class, char.class));

    try {
      adapter = MethodHandles.filterArguments(target, 0, badFilter1, filter2);
      fail();
    } catch (IllegalArgumentException expected) {
    }
  }

  static void voidFilter(char a, char b) {
    System.out.println("voidFilter");
  }

  static String filter(char a, char b) {
    return String.valueOf(a) + "+" + b;
  }

  static char badFilter(char a, char b) {
    return 0;
  }

  static int target(String a, String b, String c) {
    System.out.println("a: " + a + ", b: " + b + ", c: " + c);
    return 57;
  }

  public static void testCollectArguments() throws Throwable {
    // Test non-void filters.
    MethodHandle filter = MethodHandles.lookup().findStatic(
        Main.class, "filter",
        MethodType.methodType(String.class, char.class, char.class));

    MethodHandle target = MethodHandles.lookup().findStatic(
        Main.class, "target",
        MethodType.methodType(int.class, String.class, String.class, String.class));

    // Filter at position 0.
    MethodHandle adapter = MethodHandles.collectArguments(target, 0, filter);
    assertEquals(57, (int) adapter.invokeExact('a', 'b', "c", "d"));

    // Filter at position 1.
    adapter = MethodHandles.collectArguments(target, 1, filter);
    assertEquals(57, (int) adapter.invokeExact("a", 'b', 'c', "d"));

    // Filter at position 2.
    adapter = MethodHandles.collectArguments(target, 2, filter);
    assertEquals(57, (int) adapter.invokeExact("a", "b", 'c', 'd'));

    // Test void filters. Note that we're passing in one more argument
    // than usual because the filter returns nothing - we have to invoke with
    // the full set of filter args and the full set of target args.
    filter = MethodHandles.lookup().findStatic(Main.class, "voidFilter",
        MethodType.methodType(void.class, char.class, char.class));
    adapter = MethodHandles.collectArguments(target, 0, filter);
    assertEquals(57, (int) adapter.invokeExact('a', 'b', "a", "b", "c"));

    adapter = MethodHandles.collectArguments(target, 1, filter);
    assertEquals(57, (int) adapter.invokeExact("a", 'a', 'b', "b", "c"));

    // Test out a few failure cases.
    filter = MethodHandles.lookup().findStatic(
        Main.class, "filter",
        MethodType.methodType(String.class, char.class, char.class));

    // Bogus filter position.
    try {
      adapter = MethodHandles.collectArguments(target, 3, filter);
      fail();
    } catch (IndexOutOfBoundsException expected) {
    }

    // Mismatch in filter return type.
    filter = MethodHandles.lookup().findStatic(
        Main.class, "badFilter",
        MethodType.methodType(char.class, char.class, char.class));
    try {
      adapter = MethodHandles.collectArguments(target, 0, filter);
      fail();
    } catch (IllegalArgumentException expected) {
    }
  }

  static int insertReceiver(String a, int b, Integer c, String d) {
    System.out.println("a: " + a + ", b:" + b + ", c:" + c + ", d:" + d);
    return 73;
  }

  public static void testInsertArguments() throws Throwable {
    MethodHandle target = MethodHandles.lookup().findStatic(
        Main.class, "insertReceiver",
        MethodType.methodType(int.class,
          String.class, int.class, Integer.class, String.class));

    // Basic single element array inserted at position 0.
    MethodHandle adapter = MethodHandles.insertArguments(
        target, 0, new Object[] { "foo" });
    assertEquals(73, (int) adapter.invokeExact(45, Integer.valueOf(56), "bar"));

    // Exercise unboxing.
    adapter = MethodHandles.insertArguments(
        target, 1, new Object[] { Integer.valueOf(56), 57 });
    assertEquals(73, (int) adapter.invokeExact("foo", "bar"));

    // Exercise a widening conversion.
    adapter = MethodHandles.insertArguments(
        target, 1, new Object[] { (short) 56, Integer.valueOf(57) });
    assertEquals(73, (int) adapter.invokeExact("foo", "bar"));

    // Insert an argument at the last position.
    adapter = MethodHandles.insertArguments(
        target, 3, new Object[] { "bar" });
    assertEquals(73, (int) adapter.invokeExact("foo", 45, Integer.valueOf(46)));

    // Exercise a few error cases.

    // A reference type that can't be cast to another reference type.
    try {
      MethodHandles.insertArguments(target, 3, new Object[] { new Object() });
      fail();
    } catch (ClassCastException expected) {
    }

    // A boxed type that can't be unboxed correctly.
    try {
      MethodHandles.insertArguments(target, 1, new Object[] { Long.valueOf(56) });
      fail();
    } catch (ClassCastException expected) {
    }
  }

  public static String foldFilter(char a, char b) {
    return String.valueOf(a) + "+" + b;
  }

  public static void voidFoldFilter(String e, char a, char b) {
    System.out.println(String.valueOf(a) + "+" + b);
  }

  public static int foldTarget(String a, char b, char c, String d) {
    System.out.println("a: " + a + " ,b:" + b + " ,c:" + c + " ,d:" + d);
    return 89;
  }

  public static void mismatchedVoidFilter(Integer a) {
  }

  public static Integer mismatchedNonVoidFilter(char a, char b) {
    return null;
  }

  public static void testFoldArguments() throws Throwable {
    // Test non-void filters.
    MethodHandle filter = MethodHandles.lookup().findStatic(
        Main.class, "foldFilter",
        MethodType.methodType(String.class, char.class, char.class));

    MethodHandle target = MethodHandles.lookup().findStatic(
        Main.class, "foldTarget",
        MethodType.methodType(int.class, String.class,
          char.class, char.class, String.class));

    // Folder with a non-void type.
    MethodHandle adapter = MethodHandles.foldArguments(target, filter);
    assertEquals(89, (int) adapter.invokeExact('c', 'd', "e"));

    // Folder with a void type.
    filter = MethodHandles.lookup().findStatic(
        Main.class, "voidFoldFilter",
        MethodType.methodType(void.class, String.class, char.class, char.class));
    adapter = MethodHandles.foldArguments(target, filter);
    assertEquals(89, (int) adapter.invokeExact("a", 'c', 'd', "e"));

    // Test a few erroneous cases.

    filter = MethodHandles.lookup().findStatic(
        Main.class, "mismatchedVoidFilter",
        MethodType.methodType(void.class, Integer.class));
    try {
      adapter = MethodHandles.foldArguments(target, filter);
      fail();
    } catch (IllegalArgumentException expected) {
    }

    filter = MethodHandles.lookup().findStatic(
        Main.class, "mismatchedNonVoidFilter",
        MethodType.methodType(Integer.class, char.class, char.class));
    try {
      adapter = MethodHandles.foldArguments(target, filter);
      fail();
    } catch (IllegalArgumentException expected) {
    }
  }

  public static void fail() {
    System.out.println("FAIL");
    Thread.dumpStack();
  }

  public static void fail(String message) {
    System.out.println("fail: " + message);
    Thread.dumpStack();
  }

  public static void assertEquals(int i1, int i2) {
    if (i1 != i2) throw new AssertionError("Expected: " + i1 + " was " + i2);
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
}
