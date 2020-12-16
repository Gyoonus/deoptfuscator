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

import java.util.Arrays;

public class Test927 {
  public static void run() throws Exception {
    doTest();
  }

  private static void doTest() {
    int all1 = Runtime.getRuntime().availableProcessors();
    int all2 = getAvailableProcessors();
    if (all1 != all2) {
      throw new RuntimeException("Available processors doesn't match: " + all1 + " vs " + all2);
    }
    System.out.println("availableProcessors OK");

    Object info[] = getTimerInfo();
    System.out.println(Arrays.toString(info));

    // getTime checks.
    // Note: there isn't really much to check independent from the implementation. So we check
    //       a few details of the ART implementation. This may fail on other runtimes.
    long time1 = getTime();
    long time2 = getTime();

    // Under normal circumstances, time1 <= time2.
    if (time2 < time1) {
      throw new RuntimeException("Time unexpectedly decreased: " + time1 + " vs " + time2);
    }

    long time3 = System.nanoTime();
    long time4 = getTime();

    final long MINUTE = 60l * 1000 * 1000 * 1000;
    if (time4 < time3 || (time4 - time3 > MINUTE)) {
      throw new RuntimeException("Time unexpectedly divergent: " + time3 + " vs " + time4);
    }

    System.out.println("Time OK");
  }

  private static native int getAvailableProcessors();
  private static native Object[] getTimerInfo();
  private static native long getTime();
}
