/*
 * Copyright (C) 2015 The Android Open Source Project
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

class Main {
  public static void main(String[] args) {
    System.out.println(test(false, 5));
  }

  public static int test(boolean b, int i1) {
    int j=4;
    int s1=26294;

    for (int i = 25; i > 1; --i) {
      if (b) continue;
      // javac/dx will remove the catch information, but
      // keep the catch code around. The optimizing compiler
      // used to crash in the presence of dead blocks like the
      // code in catch.
      try {
        i1 = i1 * 26295 + (s1 / 26295);
      } catch (Throwable exc2) {
        for (j = 1; j < 39; ++j) {
          j++;
        }
      }
    }
    return i1;
  }
}
