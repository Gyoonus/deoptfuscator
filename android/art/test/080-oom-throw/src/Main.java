/*
 * Copyright (C) 2009 The Android Open Source Project
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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
    static class ArrayMemEater {
        static boolean sawOome;

        static void blowup(char[][] holder) {
            try {
                for (int i = 0; i < holder.length; ++i) {
                    holder[i] = new char[1024 * 1024];
                }
            } catch (OutOfMemoryError oome) {
                ArrayMemEater.sawOome = true;
            }
        }
    }

    static class InstanceMemEater {
        static boolean sawOome;
        static InstanceMemEater hook;

        InstanceMemEater next;
        double d1, d2, d3, d4, d5, d6, d7, d8; // Bloat this object so we fill the heap faster.

        static InstanceMemEater allocate() {
            try {
                return new InstanceMemEater();
            } catch (OutOfMemoryError e) {
                InstanceMemEater.sawOome = true;
                return null;
            }
        }

        static void confuseCompilerOptimization(InstanceMemEater instance) {
          hook = instance;
        }
    }

    public static Object eatAllMemory() {
        Object[] result = null;
        int size = 1000000;
        while (result == null && size != 0) {
            try {
                result = new Object[size];
            } catch (OutOfMemoryError oome) {
                size /= 2;
            }
        }
        if (result != null) {
            int index = 0;
            while (index != result.length && size != 0) {
                try {
                    result[index] = new byte[size];
                    ++index;
                } catch (OutOfMemoryError oome) {
                    size /= 2;
                }
            }
        }
        return result;
    }

    static boolean triggerArrayOOM() {
        ArrayMemEater.blowup(new char[128 * 1024][]);
        return ArrayMemEater.sawOome;
    }

    static boolean triggerInstanceOOM() {
        InstanceMemEater memEater = InstanceMemEater.allocate();
        InstanceMemEater lastMemEater = memEater;
        do {
            lastMemEater.next = InstanceMemEater.allocate();
            lastMemEater = lastMemEater.next;
        } while (lastMemEater != null);
        memEater.confuseCompilerOptimization(memEater);
        InstanceMemEater.hook = null;
        return InstanceMemEater.sawOome;
    }

    public static void main(String[] args) {
        if (triggerReflectionOOM()) {
            System.out.println("Test reflection correctly threw");
        }
        if (triggerReflectionOOM2()) {
            System.out.println("Test reflection2 correctly threw");
        }

        if (triggerArrayOOM()) {
            System.out.println("NEW_ARRAY correctly threw OOME");
        }

        if (triggerInstanceOOM()) {
            System.out.println("NEW_INSTANCE correctly threw OOME");
        }
    }

    static Object[] holder;

    public static void blowup() throws Exception {
        int size = 2 * 1024 * 1024;
        for (int i = 0; i < holder.length; ) {
            try {
                holder[i] = new char[size];
                i++;
            } catch (OutOfMemoryError oome) {
                size = size / 16;
                if (size == 0) {
                     break;
                }
            }
        }
        holder[0] = new char[100000];
    }

    static boolean triggerReflectionOOM() {
        try {
            Class<?> c = Main.class;
            Method m = c.getMethod("blowup");
            holder = new Object[1000000];
            m.invoke(null);
            holder = null;
            System.out.println("Didn't throw from blowup");
        } catch (OutOfMemoryError e) {
            holder = null;
        } catch (InvocationTargetException e) {
            holder = null;
            if (!(e.getCause() instanceof OutOfMemoryError)) {
                System.out.println("InvocationTargetException cause not OOME " + e.getCause());
                return false;
            }
        } catch (Exception e) {
            holder = null;
            System.out.println("Unexpected exception " + e);
            return false;
        }
        return true;
    }

    static boolean triggerReflectionOOM2() {
        Object memory = eatAllMemory();
        boolean result = false;
        try {
            Main.class.getDeclaredMethods();
        } catch (OutOfMemoryError e) {
            result = true;
        }
        if (!result) {
            boolean memoryWasAllocated = (memory != null);
            memory = null;
            System.out.println("memoryWasAllocated = " + memoryWasAllocated);
        }
        return result;
    }
}
