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
import java.util.IdentityHashMap;
import dalvik.system.VMRuntime;

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    int initialSize = monitorListSize();
    IdentityHashMap<Object, Integer> all = new IdentityHashMap();
    for (int i = 0; i < 5000; ++i) {
      Object obj = new Object();
      synchronized(obj) {
        // Should force inflation.
        all.put(obj, obj.hashCode());
      }
    }
    // Since monitor deflation is delayed significantly, we believe that even with an intervening
    // GC, monitors should remain inflated.  We allow some slop for unrelated concurrent runtime
    // actions.
    int inflatedSize = monitorListSize();
    if (inflatedSize >= initialSize + 4000) {
        System.out.println("Monitor list grew by at least 4000 monitors");
    } else {
        System.out.println("Monitor list did not grow as expected");
    }
    // Encourage monitor deflation.
    // trim() (Heap::Trim()) deflates only in JANK_IMPERCEPTIBLE state.
    // Some of this mirrors code in ActivityThread.java.
    final int DALVIK_PROCESS_STATE_JANK_PERCEPTIBLE = 0;
    final int DALVIK_PROCESS_STATE_JANK_IMPERCEPTIBLE = 1;
    VMRuntime.getRuntime().updateProcessState(DALVIK_PROCESS_STATE_JANK_IMPERCEPTIBLE);
    System.gc();
    System.runFinalization();
    trim();
    VMRuntime.getRuntime().updateProcessState(DALVIK_PROCESS_STATE_JANK_PERCEPTIBLE);
    int finalSize = monitorListSize();
    if (finalSize > initialSize + 1000) {
        System.out.println("Monitor list failed to shrink properly");
    } else {
        System.out.println("Monitor list shrank correctly");
    }
    int j = 0;
    for (Object obj: all.keySet()) {
      ++j;
      if (obj.hashCode() != all.get(obj)) {
        throw new AssertionError("Failed hashcode test!");
      }
    }
    System.out.println("Finished first check");
    for (Object obj: all.keySet()) {
      ++j;
      synchronized(obj) {
        if (obj.hashCode() != all.get(obj)) {
          throw new AssertionError("Failed hashcode test!");
        }
      }
    }
    System.out.println("Finished second check");
    System.out.println("Total checks: " + j);
  }

  private static native void trim();

  private static native int monitorListSize();
}
