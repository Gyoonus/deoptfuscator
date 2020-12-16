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

package art;

import java.util.ArrayList;
import java.util.Arrays;

public class Test905 {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    // Use a list to ensure objects must be allocated.
    ArrayList<Object> l = new ArrayList<>(100);

    setupObjectFreeCallback();

    enableFreeTracking(true);
    run(l);

    enableFreeTracking(false);
    run(l);

    enableFreeTracking(true);
    stress();
  }

  private static void run(ArrayList<Object> l) {
    allocate(l, 1);
    l.clear();

    Runtime.getRuntime().gc();

    getAndPrintTags();
    System.out.println("---");

    // Note: the reporting will not depend on the heap layout (which could be unstable). Walking
    //       the tag table should give us a stable output order.
    for (int i = 10; i <= 1000; i *= 10) {
      allocate(l, i);
    }
    l.clear();

    Runtime.getRuntime().gc();

    getAndPrintTags();
    System.out.println("---");

    Runtime.getRuntime().gc();

    getAndPrintTags();
    System.out.println("---");
  }

  private static void stressAllocate(int i) {
    Object obj = new Object();
    Main.setTag(obj, i);
    setTag2(obj, i + 1);
  }

  private static void stress() {
    getCollectedTags(0);
    getCollectedTags(1);
    // Allocate objects.
    for (int i = 1; i <= 100000; ++i) {
      stressAllocate(i);
    }
    Runtime.getRuntime().gc();
    long[] freedTags1 = getCollectedTags(0);
    long[] freedTags2 = getCollectedTags(1);
    System.out.println("Free counts " + freedTags1.length + " " + freedTags2.length);
    for (int i = 0; i < freedTags1.length; ++i) {
      if (freedTags1[i] + 1 != freedTags2[i]) {
        System.out.println("Mismatched tags " + freedTags1[i] + " " + freedTags2[i]);
      }
    }
  }

  private static void allocate(ArrayList<Object> l, long tag) {
    Object obj = new Object();
    l.add(obj);
    Main.setTag(obj, tag);
  }

  private static void getAndPrintTags() {
    long[] freedTags = getCollectedTags(0);
    Arrays.sort(freedTags);
    System.out.println(Arrays.toString(freedTags));
  }

  private static native void setupObjectFreeCallback();
  private static native void enableFreeTracking(boolean enable);
  private static native long[] getCollectedTags(int index);
  private static native void setTag2(Object o, long tag);
}
