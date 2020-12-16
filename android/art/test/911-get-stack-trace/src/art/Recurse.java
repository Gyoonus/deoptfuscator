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

public class Recurse {
  public static int foo(int x, int start, int max, ControlData data) {
    bar(x, start, max, data);
    return 0;
  }

  private static long bar(int x, int start, int max, ControlData data) {
    baz(x, start, max, data);
    return 0;
  }

  private static Object baz(int x, int start, int max, ControlData data) {
    if (x == 0) {
      printOrWait(start, max, data);
    } else {
      foo(x - 1, start, max, data);
    }
    return null;
  }

  private static void printOrWait(int start, int max, ControlData data) {
    if (data == null) {
      PrintThread.print(Thread.currentThread(), start, max);
    } else {
      if (data.waitFor != null) {
        synchronized (data.waitFor) {
          data.reached.countDown();
          try {
            data.waitFor.wait();  // Use wait() as it doesn't have a "hidden" Java call-graph.
          } catch (Throwable t) {
            throw new RuntimeException(t);
          }
        }
      } else {
        data.reached.countDown();
        while (!data.stop) {
          // Busy-loop.
        }
      }
    }
  }
}