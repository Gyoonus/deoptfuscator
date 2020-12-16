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

import java.lang.annotation.Annotation;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Arrays;
import java.util.Comparator;

/**
 * Test invoking a proxy method from native code.
 */

interface NativeInterface {
    public void callback();
}

public class NativeProxy {

    public static void main(String[] args) {
        System.loadLibrary(args[0]);

        try {
            NativeInterface inf = (NativeInterface)Proxy.newProxyInstance(
                    NativeProxy.class.getClassLoader(),
                    new Class<?>[] { NativeInterface.class },
                    new NativeInvocationHandler());

            nativeCall(inf);
        } catch (Exception exc) {
            throw new RuntimeException(exc);
        }
    }

    public static class NativeInvocationHandler implements InvocationHandler {
        public Object invoke(final Object proxy,
                             final Method method,
                             final Object[] args) throws Throwable {
            System.out.println(method.getName());
            return null;
        }
    }

    public static native void nativeCall(NativeInterface inf);
}
