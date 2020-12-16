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

import java.lang.ref.WeakReference;

public class Main {
  static final class GcWatcher {
    protected void finalize() throws Throwable {
        watcher = new WeakReference<GcWatcher>(new GcWatcher());
        ++finalizeCounter;
    }
  }
  static WeakReference<GcWatcher> watcher = new WeakReference<GcWatcher>(new GcWatcher());
  static Object o = new Object();
  static int finalizeCounter = 0;

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    backgroundProcessState();
    try {
        Runtime.getRuntime().gc();
        for (int i = 0; i < 10; ++i) {
            o = new Object();
            Thread.sleep(1000);
        }
    } catch (Exception e) {}
    System.out.println("Finalize count too large: " +
            ((finalizeCounter >= 15) ? Integer.toString(finalizeCounter) : "false"));
  }

  private static native void backgroundProcessState();
}
