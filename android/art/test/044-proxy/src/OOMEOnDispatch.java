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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;

/**
 * Ensure that one can dispatch without aborting when the heap is full.
 */
public class OOMEOnDispatch implements InvocationHandler {

    static ArrayList<Object> storage = new ArrayList<>(100000);

    public static void main(String[] args) {
        InvocationHandler handler = new OOMEOnDispatch();
        OOMEInterface inf = (OOMEInterface)Proxy.newProxyInstance(
                OOMEInterface.class.getClassLoader(), new Class[] { OOMEInterface.class },
                handler);

        // Stop the JIT to be sure nothing is running that could be resolving classes or causing
        // verification.
        Main.stopJit();
        Main.waitForCompilation();

        int l = 1024 * 1024;
        while (l > 8) {
          try {
            storage.add(new byte[l]);
          } catch (OutOfMemoryError e) {
            l = l/2;
          }
        }

        try {
            inf.foo();
            storage.clear();
            System.out.println("Did not receive OOME!");
        } catch (OutOfMemoryError oome) {
            storage.clear();
            System.out.println("Received OOME");
        }

        Main.startJit();
    }

    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        storage.clear();
        System.out.println("Should not have reached OOMEOnDispatch.invoke!");
        return null;
    }
}

interface OOMEInterface {
    public void foo();
}
