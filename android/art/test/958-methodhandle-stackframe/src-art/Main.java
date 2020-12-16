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
import java.lang.invoke.Transformers.Transformer;

import dalvik.system.EmulatedStackFrame;

public class Main {

  public static void testDelegate_allTypes(boolean z, char a, short b, int c, long d,
                                           float e, double f, String g, Object h) {
    System.out.println("boolean: " + z);
    System.out.println("char: " + a);
    System.out.println("short: " + b);
    System.out.println("int: " + c);
    System.out.println("long: " + d);
    System.out.println("float: " + e);
    System.out.println("double: " + f);
    System.out.println("String: " + g);
    System.out.println("Object: " + h);
  }

  public static boolean testDelegate_returnBoolean() {
    return true;
  }

  public static char testDelegate_returnChar() {
    return 'a';
  }

  public static int testDelegate_returnInt() {
    return 42;
  }

  public static long testDelegate_returnLong() {
    return 43;
  }

  public static float testDelegate_returnFloat() {
    return 43.0f;
  }

  public static double testDelegate_returnDouble() {
    return 43.0;
  }

  public static String testDelegate_returnString() {
    return "plank";
  }

  public static class DelegatingTransformer extends Transformer {
    private final MethodHandle delegate;

    public DelegatingTransformer(MethodHandle delegate) {
      super(delegate.type());
      this.delegate = delegate;
    }

    @Override
    public void transform(EmulatedStackFrame stackFrame) throws Throwable {
      delegate.invoke(stackFrame);
    }
  }

  public static void main(String[] args) throws Throwable {
    MethodHandle specialFunctionHandle = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_allTypes", MethodType.methodType(void.class,
          new Class<?>[] { boolean.class, char.class, short.class, int.class, long.class,
            float.class, double.class, String.class, Object.class }));

    MethodHandle delegate = new DelegatingTransformer(specialFunctionHandle);

    // Test an exact invoke.
    //
    // Note that the shorter form below doesn't work and must be
    // investigated on the jack side :  b/32536744
    //
    // delegate.invokeExact(false, 'h', (short) 56, 72, Integer.MAX_VALUE + 42l,
    //    0.56f, 100.0d, "hello", (Object) "goodbye");

    Object obj = "goodbye";
    delegate.invokeExact(false, 'h', (short) 56, 72, Integer.MAX_VALUE + 42l,
        0.56f, 100.0d, "hello", obj);

    // Test a non exact invoke with one int -> long conversion and a float -> double
    // conversion.
    delegate.invoke(false, 'h', (short) 56, 72, 73,
        0.56f, 100.0f, "hello", "goodbye");

    // Should throw a WrongMethodTypeException if the types don't align.
    try {
      delegate.invoke(false);
      throw new AssertionError("Call to invoke unexpectedly succeeded");
    } catch (WrongMethodTypeException expected) {
    }

    // Test return values.

    // boolean.
    MethodHandle returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnBoolean", MethodType.methodType(boolean.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((boolean) delegate.invoke());
    System.out.println((boolean) delegate.invokeExact());

    // char.
    returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnChar", MethodType.methodType(char.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((char) delegate.invoke());
    System.out.println((char) delegate.invokeExact());

    // int.
    returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnInt", MethodType.methodType(int.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((int) delegate.invoke());
    System.out.println((int) delegate.invokeExact());

    // long.
    returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnLong", MethodType.methodType(long.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((long) delegate.invoke());
    System.out.println((long) delegate.invokeExact());

    // float.
    returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnFloat", MethodType.methodType(float.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((float) delegate.invoke());
    System.out.println((float) delegate.invokeExact());

    // double.
    returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnDouble", MethodType.methodType(double.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((double) delegate.invoke());
    System.out.println((double) delegate.invokeExact());

    // references.
    returner = MethodHandles.lookup().findStatic(
        Main.class, "testDelegate_returnString", MethodType.methodType(String.class));
    delegate = new DelegatingTransformer(returner);

    System.out.println((String) delegate.invoke());
    System.out.println((String) delegate.invokeExact());
  }
}


