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

public class Main {
  public static void main(String[] args) {
    for (int i = 0; i < 20000; ++i) {
      $noinline$testInTryCatch(new Main(), i);
      $noinline$testInTryCatch(new SubMain(), i);
    }
  }

  public static void $noinline$testInTryCatch(Main m, int i) {
    final int value;
    try {
      throw new Exception();
    } catch (Exception e) {
      // The polymorphic inlining of 'willInlineVoid' used to generate an
      // incorrect graph, by setting the inlined blocks as catch blocks.
      m.willInlineVoid(i);
      return;
    }
  }

  public void willInlineVoid(int i) {
    if (i == 0) {
      $noinline$foo();
    } else {
      $noinline$foo();
      $noinline$foo();
    }
  }

  public static void $noinline$foo() {
    if (doThrow) throw new Error("");
  }

  public static boolean doThrow;
}

class SubMain extends Main {
  public void willInlineVoid(int i) {
  }
}
