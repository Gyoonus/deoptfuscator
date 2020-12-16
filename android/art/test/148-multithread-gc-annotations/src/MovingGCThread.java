/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.util.TreeMap;

public class MovingGCThread implements Runnable {
    private static TreeMap treeMap = new TreeMap();

    public void run() {
        for (int i = 0; i < 20; i++) {
            testHomogeneousCompaction();
        }
    }

    public static void testHomogeneousCompaction() {
        final boolean supportHSC = supportHomogeneousSpaceCompact();
        if (!supportHSC) {
            return;
        }
        Object o = new Object();
        long addressBefore = objectAddress(o);
        allocateStuff();
        final boolean success = performHomogeneousSpaceCompact();
        allocateStuff();
        if (!success) {
            System.out.println("error: Expected " + supportHSC + " but got " + success);
        }
        allocateStuff();
        long addressAfter = objectAddress(o);
        // This relies on the compaction copying from one space to another space and there being
        // no overlap.
        if (addressBefore == addressAfter) {
            System.out.println("error: Expected different adddress " + addressBefore + " vs " +
                    addressAfter);
        }
    }

    private static void allocateStuff() {
        for (int i = 0; i < 1000; ++i) {
            Object o = new Object();
            treeMap.put(o.hashCode(), o);
        }
    }

    // Methods to get access to ART internals.
    private static native boolean supportHomogeneousSpaceCompact();
    private static native boolean performHomogeneousSpaceCompact();
    private static native long objectAddress(Object object);
}
