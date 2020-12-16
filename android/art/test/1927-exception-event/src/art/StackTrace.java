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

package art;

import java.lang.reflect.Field;
import java.lang.reflect.Executable;

public class StackTrace {
  public static class StackFrameData {
    public final Thread thr;
    public final Executable method;
    public final long current_location;
    public final int depth;

    public StackFrameData(Thread thr, Executable e, long loc, int depth) {
      this.thr = thr;
      this.method = e;
      this.current_location = loc;
      this.depth = depth;
    }
    @Override
    public String toString() {
      return String.format(
          "StackFrameData { thr: '%s', method: '%s', loc: %d, depth: %d }",
          this.thr,
          this.method,
          this.current_location,
          this.depth);
    }
  }

  public static native int GetStackDepth(Thread thr);

  private static native StackFrameData[] nativeGetStackTrace(Thread thr);

  public static StackFrameData[] GetStackTrace(Thread thr) {
    // The RI seems to give inconsistent (and sometimes nonsensical) results if the thread is not
    // suspended. The spec says that not being suspended is fine but since we want this to be
    // consistent we will suspend for the RI.
    boolean suspend_thread =
        !System.getProperty("java.vm.name").equals("Dalvik") &&
        !thr.equals(Thread.currentThread()) &&
        !Suspension.isSuspended(thr);
    if (suspend_thread) {
      Suspension.suspend(thr);
    }
    StackFrameData[] out = nativeGetStackTrace(thr);
    if (suspend_thread) {
      Suspension.resume(thr);
    }
    return out;
  }
}

