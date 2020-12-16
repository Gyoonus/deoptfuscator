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
import java.util.ArrayList;

class OOMEHelper {
    int nullField;
}

/**
 * Test that null field access under an OOME situation works.
 *
 * The test relies on compile-time verification. This class is compile-time verifiable, so when
 * loaded at runtime will not transitively load referenced types eagerly. In that case, our code
 * to give descriptive NullPointerExceptions for the field access to the null "instance" of
 * OOMEHelper in nullAccess() will be the first attempting to load the class, and, under the
 * induced low-memory situation, will throw itself an OutOfMemoryError.
 */
public class OOMEOnNullAccess {

    static ArrayList<Object> storage = new ArrayList<>(100000);

    public static void main(String[] args) {
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
            nullAccess(null);
            storage.clear();
            throw new RuntimeException("Did not receive exception!");
        } catch (OutOfMemoryError oome) {
            storage.clear();
            System.err.println("Received OOME");
        } finally {
            // Even if it's an unexpected error, clear so that we can print things later.
            storage.clear();
        }

        Main.startJit();
    }

    public static int nullAccess(OOMEHelper nullInstance) {
        // Under AOT, this access is the first one to actually load the OOMEHelper class, so
        // we can pretty print the name and such.
        return nullInstance.nullField;
    }
}
