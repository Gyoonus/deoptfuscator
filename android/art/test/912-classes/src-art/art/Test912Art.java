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

import java.lang.ref.Reference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;

public class Test912Art {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    testClassEvents();
  }

  private static void testClassEvents() throws Exception {
    // Note: the JIT part of this test is about the JIT pulling in a class not yet touched by
    //       anything else in the system. This could be the verifier or the interpreter. We
    //       block the interpreter by calling ensureJitCompiled. The verifier, however, must
    //       run in configurations where dex2oat didn't verify the class itself. So explicitly
    //       check whether the class has been already loaded, and skip then.
    // TODO: Add multiple configurations to the run script once that becomes easier to do.
    if (hasJit() && !isLoadedClass("Lart/Test912Art$ClassD;")) {
      testClassEventsJit();
    }
  }

  private static void testClassEventsJit() throws Exception {
    enableClassLoadSeenEvents(true);

    testClassEventsJitImpl();

    enableClassLoadSeenEvents(false);

    if (!hadLoadEvent()) {
      throw new RuntimeException("Did not get expected load event.");
    }
  }

  private static void testClassEventsJitImpl() throws Exception {
    ensureJitCompiled(Test912Art.class, "testClassEventsJitImpl");

    if (ClassD.x != 1) {
      throw new RuntimeException("Unexpected value");
    }
  }

  private static native void ensureJitCompiled(Class<?> c, String name);

  private static native boolean hasJit();
  private static native boolean isLoadedClass(String name);
  private static native void enableClassLoadSeenEvents(boolean b);
  private static native boolean hadLoadEvent();

  public static class ClassD {
    static int x = 1;
  }
}
