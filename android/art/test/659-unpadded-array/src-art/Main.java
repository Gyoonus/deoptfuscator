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

import dalvik.system.VMRuntime;

public class Main {
  public static void main(String[] args) {
    // Call our optimization API, we used to have a bug in the RegionSpace on large
    // objects allocated through it.
    Object[] o = (Object[]) VMRuntime.getRuntime().newUnpaddedArray(Object.class, 70000);

    // Make the test run for 30 seconds to be less dependent on GC heuristics.
    long time = System.currentTimeMillis();
    int i = 1;
    do {
      allocateIntArray(i);
      for (int j = 0; j < o.length; j++) {
        if (o[j] != null) {
          // Just print, not throw, to get into "interesting" issues (eg the first
          // element that will not be null is the class of the object, the second is
          // actually the first element of the int array).
          System.out.println("Unexpected value: " + o[j]);
        }
      }
      if (i < 100000) {
        i++;
      } else {
        i = 0;
      }
    } while (System.currentTimeMillis() - time < 30000);
  }

  static void allocateIntArray(int i) {
    int[] intArray = new int[i];
    for (int j = 0; j < intArray.length; j++) {
      intArray[j] = 1;
    }
  }
}
