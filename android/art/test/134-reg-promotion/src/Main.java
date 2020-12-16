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

import java.lang.reflect.Method;

public class Main {
    static char [][] holder;
    static boolean sawOome;

    static void blowup() {
        try {
            for (int i = 0; i < holder.length; ++i) {
                holder[i] = new char[1024 * 1024];
            }
        } catch (OutOfMemoryError oome) {
            sawOome = true;
        }
    }

    public static void main(String args[]) throws Exception {
        Class<?> c = Class.forName("Test");
        Method m = c.getMethod("run");
        for (int i = 0; i < 10; i++) {
            holder = new char[128 * 1024][];
            m.invoke(null, (Object[]) null);
            holder = null;
        }
        m = c.getMethod("run2");
        for (int i = 0; i < 10; i++) {
            holder = new char[128 * 1024][];
            m.invoke(null, (Object[]) null);
            holder = null;
        }
    }
}
