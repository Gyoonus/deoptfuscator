/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

interface TestInterface {
  void method0();
  void method1(String arg);
  void method10(String arg1, String arg2, String arg3, String arg4, String arg5,
                String arg6, String arg7, String arg8, String arg9, String arg10);
  void method10Even(byte arg1, String arg2, short arg3, String arg4, int arg5,
                    String arg6, long arg7, String arg8, double arg9, String arg10);
}

class TestInvocationHandler implements InvocationHandler {
  @Override
  public Object invoke(Object proxy, Method method, Object[] args) {
    // Force garbage collection to try to make `proxy` move in memory
    // (in the case of a moving garbage collector).
    System.gc();

    System.out.println("Proxy for " + TestInterface.class + "." + method.getName());
    if (method.getName().equals("method0")) {
      testMethod0(proxy, args);
    } else if (method.getName().equals("method1")) {
      testMethod1(proxy, args);
    } else if (method.getName().equals("method10")) {
      testMethod10(proxy, args);
    } else if (method.getName().equals("method10Even")) {
      testMethod10Even(proxy, args);
    }
    return null;
  }

  private void testMethod0(Object proxy, Object[] args) {
    // Get argument 0 (method target) from the proxy method frame ($Proxy0.method0 activation).
    Object arg0 = getProxyMethodArgument(0);
    System.out.println("  arg0: " + arg0.getClass().getName());
    Main.assertEquals(proxy, arg0);
  }

  private void testMethod1(Object proxy, Object[] args) {
    // Get argument 0 (method target) from the proxy method frame ($Proxy0.method0 activation).
    Object arg0 = getProxyMethodArgument(0);
    System.out.println("  arg0: " + arg0.getClass().getName());
    Main.assertEquals(proxy, arg0);
    // Get argument 1 from the proxy method frame ($Proxy0.method1 activation).
    String arg1 = (String) getProxyMethodArgument(1);
    System.out.println("  arg1: " + arg1.getClass().getName() + " \"" + arg1 + "\"");
    Main.assertEquals(args[0], arg1);
  }

  private void testMethod10(Object proxy, Object[] args) {
    // Get argument 0 (method target) from the proxy method frame ($Proxy0.method10 activation).
    Object arg0 = getProxyMethodArgument(0);
    System.out.println("  arg0: " + arg0.getClass().getName());
    Main.assertEquals(proxy, arg0);
    // Get argument `i` from the proxy method frame ($Proxy0.method10 activation).
    for (int i = 0; i < 10; ++i) {
      int arg_pos = i + 1;
      String arg = (String) getProxyMethodArgument(arg_pos);
      System.out.println("  arg" + arg_pos + ": " + arg.getClass().getName() + " \"" + arg + "\"");
      Main.assertEquals(args[i], arg);
    }
  }

  private void testMethod10Even(Object proxy, Object[] args) {
    // Get argument 0 (method target) from the proxy method frame ($Proxy0.method10Even
    // activation).
    Object arg0 = getProxyMethodArgument(0);
    System.out.println("  arg0: " + arg0.getClass().getName());
    Main.assertEquals(proxy, arg0);
    // Get argument `i` from the proxy method frame ($Proxy0.method10Even activation).
    for (int i = 1; i < 10; i += 2) {
      int arg_pos = i + 1;
      String arg = (String) getProxyMethodArgument(arg_pos);
      System.out.println("  arg" + arg_pos + ": " + arg.getClass().getName() + " \"" + arg + "\"");
      Main.assertEquals(args[i], arg);
    }
  }

  // Get reference argument at position `arg_pos` in proxy frame.
  // This method should only be called from one of the
  // `TestInvocationHandler.testMethod*` methods via `TestInvocationHandler.invoke`.
  private Object getProxyMethodArgument(int arg_pos) {
    // Find proxy frame in stack (from a testMethod* method).
    //
    //     depth  method
    //     ----------------------------------------------------------------------
    //     0      TestInvocationHandler.getArgument             (outermost frame)
    //     1      TestInvocationHandler.getProxyMethodArgument
    //     2      TestInvocationHandler.testMethod*
    //     3      TestInvocationHandler.invoke
    //     4      java.lang.reflect.Proxy.invoke
    //  -> 5      TestInterface.method*                         (proxy method)
    //     6      Main.main                                     (innermost frame)
    //
    int proxy_method_frame_depth = 5;
    return getArgument(arg_pos, proxy_method_frame_depth);
  }

  // Get reference argument at position `arg_pos` in frame at depth `frame_depth`.
  private native Object getArgument(int arg_pos, int frame_depth);
}

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    TestInvocationHandler invocationHandler = new TestInvocationHandler();
    TestInterface proxy = (TestInterface) Proxy.newProxyInstance(
        Main.class.getClassLoader(),
        new Class<?>[] { TestInterface.class },
        invocationHandler);
    System.out.println("proxy: " + proxy.getClass().getName());

    proxy.method0();
    proxy.method1("a");
    proxy.method10("one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten");
    proxy.method10Even((byte) 1, "two", (short) 3, "four", 5, "six", 7L, "eight", 9.0, "ten");
  }

  public static void assertEquals(Object expected, Object actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected  + ", got " + actual);
    }
  }

  public static void assertEquals(String expected, String actual) {
    if (expected != actual) {
      throw new Error("Expected \"" + expected  + "\", got \"" + actual + "\"");
    }
  }
}
