/*
 * Copyright (C) 2014 The Android Open Source Project
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
    if ($opt$inline$returnInt() != 4) {
      throw new Error();
    }

    if ($opt$inline$returnParameter(42) != 42) {
      throw new Error();
    }

    if ($opt$inline$returnWide() != 12L) {
      throw new Error();
    }

    if ($opt$inline$returnWideParameter(0x100000001L) != 0x100000001L) {
      throw new Error();
    }

    if ($opt$inline$returnReferenceParameter(Main.class) != Main.class) {
      throw new Error();
    }

    $opt$inline$returnVoid();
    $opt$inline$returnVoidWithOneParameter(32);

    if ($opt$inline$returnAdd(42, 1) != 43) {
      throw new Error();
    }

    if ($opt$inline$returnSub(42, 1) != 41) {
      throw new Error();
    }

    // Some architectures used to not be able to allocate registers with
    // floating point operations. This call is a regression test that we don't
    // try inlining methods with floats in it on such architectures. The
    // compiler used to crash after inlining a method it cannot allocate
    // registers for.
    tryInlineFloat();
  }

  public static int tryInlineFloat() {
    return useFloatMethod();
  }

  public static float staticFloat = 42.0f;

  public static int useFloatMethod() {
    return (int)staticFloat;
  }

  public static int $opt$inline$returnParameter(int a) {
    return a;
  }

  public static int $opt$inline$returnAdd(int a, int b) {
    return a + b;
  }

  public static int $opt$inline$returnSub(int a, int b) {
    return a - b;
  }

  public static int $opt$inline$returnInt() {
    return 4;
  }

  public static long $opt$inline$returnWideParameter(long a) {
    return a;
  }

  public static long $opt$inline$returnWide() {
    return 12L;
  }

  public static Object $opt$inline$returnReferenceParameter(Object o) {
    return o;
  }

  public static void $opt$inline$returnVoid() {
    return;
  }

  public static void $opt$inline$returnVoidWithOneParameter(int a) {
    return;
  }
}
