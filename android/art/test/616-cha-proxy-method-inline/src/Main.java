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

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

class DebugProxy implements java.lang.reflect.InvocationHandler {
  private Object obj;
  static Class<?>[] interfaces = {Foo.class};

  public static Object newInstance(Object obj) {
    return java.lang.reflect.Proxy.newProxyInstance(
      Foo.class.getClassLoader(),
      interfaces,
      new DebugProxy(obj));
  }

  private DebugProxy(Object obj) {
    this.obj = obj;
  }

  public Object invoke(Object proxy, Method m, Object[] args) throws Throwable {
    Object result;
    if (obj == null) {
      return null;
    }
    try {
      System.out.println("before invoking method " + m.getName());
      result = m.invoke(obj, args);
    } catch (InvocationTargetException e) {
      throw e.getTargetException();
    } catch (Exception e) {
      throw new RuntimeException("unexpected invocation exception: " + e.getMessage());
    } finally {
      System.out.println("after invoking method " + m.getName());
    }
    return result;
  }
}

public class Main {
  public static void call(Foo foo) {
    if (foo == null) {
      return;
    }
    foo.bar(null);
  }

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    Foo foo = (Foo)DebugProxy.newInstance(null);
    ensureJitCompiled(Main.class, "call");
    call(foo);
  }

  private static native void ensureJitCompiled(Class<?> itf, String method_name);
}
