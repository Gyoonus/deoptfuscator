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

public class Test911 {
  public static void run() throws Exception {
    Thread t = new Thread("Test911") {
      @Override
      public void run() {
        try {
          SameThread.doTest();

          System.out.println();

          OtherThread.doTestOtherThreadWait();

          System.out.println();

          OtherThread.doTestOtherThreadBusyLoop();

          System.out.println();

          AllTraces.doTest();

          System.out.println();

          ThreadListTraces.doTest();

          System.out.println();

          Frames.doTest();
        } catch (Exception e) {
          throw new RuntimeException(e);
        }
      }
    };
    t.start();
    t.join();

    System.out.println("Done");
  }
}
