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

import java.lang.reflect.Method;
import java.util.Map;

public class Main implements Runnable {
    static final int numberOfThreads = 4;
    static final int totalOperations = 1000;
    static Method enableAllocTrackingMethod;
    static Object holder;
    static volatile boolean trackingThreadDone = false;
    int threadIndex;

    Main(int index) {
        threadIndex = index;
    }

    public static void main(String[] args) throws Exception {
      Class<?> klass = Class.forName("org.apache.harmony.dalvik.ddmc.DdmVmInternal");
      if (klass == null) {
          throw new AssertionError("Couldn't find DdmVmInternal class");
      }
      enableAllocTrackingMethod = klass.getDeclaredMethod("enableRecentAllocations",
              Boolean.TYPE);
      if (enableAllocTrackingMethod == null) {
          throw new AssertionError("Couldn't find enableRecentAllocations method");
      }

      final Thread[] threads = new Thread[numberOfThreads];
      for (int t = 0; t < threads.length; t++) {
          threads[t] = new Thread(new Main(t));
          threads[t].start();
      }
      for (Thread t : threads) {
          t.join();
      }
      System.out.println("Finishing");
    }

    public void run() {
        if (threadIndex == 0) {
            for (int i = 0; i < totalOperations; ++i) {
                try {
                    enableAllocTrackingMethod.invoke(null, true);
                    holder = new Object();
                    enableAllocTrackingMethod.invoke(null, false);
                } catch (Exception e) {
                    System.out.println(e);
                    return;
                }
            }
            trackingThreadDone = true;
        } else {
            while (!trackingThreadDone) {
                holder = new Object();
            }
        }
    }
}
