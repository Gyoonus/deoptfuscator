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

import java.util.Vector;

public class Main {
    static char [][] holder;

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

    static class InstanceFinalizerMemEater {
        public void finalize() {}
    }

    static boolean triggerArrayOOM(char[][] holder) {
        ArrayMemEater.blowup(holder);
        return ArrayMemEater.sawOome;
    }

    static boolean triggerInstanceFinalizerOOM() {
        boolean sawOome = false;
        try {
            Vector v = new Vector();
            while (true) {
                v.add(new InstanceFinalizerMemEater());
            }
        } catch (OutOfMemoryError e) {
            sawOome = true;
        }
        return sawOome;
    }

    public static void main(String[] args) {
        // Keep holder alive to make instance OOM happen faster.
        holder = new char[128 * 1024][];
        if (!triggerArrayOOM(holder)) {
            // The test failed here. To avoid potential OOME during println,
            // make holder unreachable.
            holder = null;
            System.out.println("NEW_ARRAY did not throw OOME");
        }

        if (!triggerInstanceFinalizerOOM()) {
            // The test failed here. To avoid potential OOME during println,
            // make holder unreachable.
            holder = null;
            System.out.println("NEW_INSTANCE (finalize) did not throw OOME");
        }

        // Make holder unreachable here so that the Sentinel
        // allocation in runFinalization() won't fail.
        holder = null;
        System.runFinalization();
    }
}
