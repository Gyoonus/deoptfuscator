/*
 * Copyright 2016 The Android Open Source Project
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

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * Tests proxies when used with constructor methods.
 */
class ConstructorProxy implements InvocationHandler {
  public static void main() {
    try {
      new ConstructorProxy().runTest();
    } catch (Exception e) {
      System.out.println("Unexpected failure occured");
      e.printStackTrace(System.out);
    }
  }

  public void runTest() throws Exception {
    Class<?> proxyClass = Proxy.getProxyClass(
            getClass().getClassLoader(),
            new Class<?>[] { Runnable.class }
    );
    Constructor<?> constructor = proxyClass.getConstructor(InvocationHandler.class);
    System.out.println("Found constructor.");
    // We used to crash when asking the exception types of the constructor, because the runtime was
    // not using the non-proxy ArtMethod
    Object[] exceptions = constructor.getExceptionTypes();
    System.out.println("Found constructors with " + exceptions.length + " exceptions");
  }

  @Override
  public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
    return args[0];
  }
}

