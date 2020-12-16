/*
 * Copyright (C) 2014 The Android Open Source Project
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

import java.lang.reflect.*;

/**
 * Test java.lang.reflect.Proxy
 */
public class FloatSelect {

    public interface FloatSelectI {
      public float method(float a, float b);
    }

    static class FloatSelectIInvoke1 implements InvocationHandler {
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            return args[1];
        }
    }

    public static void main(String[] args) {
        FloatSelectI proxyObject = (FloatSelectI) Proxy.newProxyInstance(
            FloatSelectI.class.getClassLoader(),
            new Class<?>[] { FloatSelectI.class },
            new FloatSelectIInvoke1());

        float floatResult = proxyObject.method(2.1f, 5.8f);
        System.out.println(floatResult);
    }
}
