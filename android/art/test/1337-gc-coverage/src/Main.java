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

import java.util.TreeMap;

public class Main {
  private static TreeMap treeMap = new TreeMap();

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    testHomogeneousCompaction();
    testCollectorTransitions();
    System.out.println("Done.");
  }

  private static void allocateStuff() {
    for (int i = 0; i < 1000; ++i) {
      Object o = new Object();
      treeMap.put(o.hashCode(), o);
    }
  }

  public static void testHomogeneousCompaction() {
    System.out.println("Attempting homogeneous compaction");
    final boolean supportHSC = supportHomogeneousSpaceCompact();
    Object o = new Object();
    long addressBefore = objectAddress(o);
    long addressAfter;
    allocateStuff();
    final boolean success = performHomogeneousSpaceCompact();
    allocateStuff();
    System.out.println("Homogeneous compaction support=" + supportHSC + " success=" + success);
    if (supportHSC != success) {
      System.out.println("error: Expected " + supportHSC + " but got " + success);
    }
    if (success) {
      allocateStuff();
      addressAfter = objectAddress(o);
      // This relies on the compaction copying from one space to another space and there being no
      // overlap.
      if (addressBefore == addressAfter) {
        System.out.println("error: Expected different adddress " + addressBefore + " vs " +
            addressAfter);
      }
    }
    if (supportHSC) {
      incrementDisableMovingGC();
      if (performHomogeneousSpaceCompact()) {
        System.out.println("error: Compaction succeeded when moving GC is disabled");
      }
      decrementDisableMovingGC();
      if (!performHomogeneousSpaceCompact()) {
        System.out.println("error: Compaction failed when moving GC is enabled");
      }
    }
  }

  private static void testCollectorTransitions() {
    if (supportCollectorTransition()) {
      Object o = new Object();
      // Transition to semi-space collector.
      allocateStuff();
      transitionToSS();
      allocateStuff();
      long addressBefore = objectAddress(o);
      Runtime.getRuntime().gc();
      long addressAfter = objectAddress(o);
      if (addressBefore == addressAfter) {
        System.out.println("error: Expected different adddress " + addressBefore + " vs " +
            addressAfter);
      }
      // Transition back to CMS.
      transitionToCMS();
      allocateStuff();
      addressBefore = objectAddress(o);
      Runtime.getRuntime().gc();
      addressAfter = objectAddress(o);
      if (addressBefore != addressAfter) {
        System.out.println("error: Expected same adddress " + addressBefore + " vs " +
            addressAfter);
      }
    }
  }

  // Methods to get access to ART internals.
  private static native boolean supportHomogeneousSpaceCompact();
  private static native boolean performHomogeneousSpaceCompact();
  private static native void incrementDisableMovingGC();
  private static native void decrementDisableMovingGC();
  private static native long objectAddress(Object object);
  private static native boolean supportCollectorTransition();
  private static native void transitionToSS();
  private static native void transitionToCMS();
}
