/*
 * Copyright (C) 2018 The Android Open Source Project
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

public class Main {
  public static void $noinline$hotnessCount() {
  }

  public static void $noinline$hotnessCountWithLoop() {
    for (int i = 0; i < 100; i++) {
      $noinline$hotnessCount();
    }
  }

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    if (!isAotCompiled(Main.class, "main")) {
      return;
    }
    $noinline$hotnessCount();
    int counter = getHotnessCounter(Main.class, "$noinline$hotnessCount");
    if (counter == 0) {
      throw new Error("Expected hotness counter to be updated");
    }

    $noinline$hotnessCountWithLoop();
    if (getHotnessCounter(Main.class, "$noinline$hotnessCountWithLoop") <= counter) {
      throw new Error("Expected hotness counter of a loop to be greater than without loop");
    }
  }

  public static native int getHotnessCounter(Class<?> cls, String methodName);
  public static native boolean isAotCompiled(Class<?> cls, String methodName);
}
