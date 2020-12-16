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

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class PrintThread {
  public static void print(String[][] stack) {
    System.out.println("---------");
    for (String[] stackElement : stack) {
      for (String part : stackElement) {
        System.out.print(' ');
        System.out.print(part);
      }
      System.out.println();
    }
  }

  public static void print(Thread t, int start, int max) {
    print(getStackTrace(t, start, max));
  }

  // We have to ignore some threads when printing all stack traces. These are threads that may or
  // may not exist depending on the environment.
  public final static String IGNORE_THREAD_NAME_REGEX =
      "Binder:|RenderThread|hwuiTask|Jit thread pool worker|Instr:|JDWP|Profile Saver|main|" +
      "queued-work-looper|InstrumentationConnectionThread";
  public final static Matcher IGNORE_THREADS =
      Pattern.compile(IGNORE_THREAD_NAME_REGEX).matcher("");

  // We have to skip the stack of some threads when printing all stack traces. These are threads
  // that may have a different call stack (e.g., when run as an app), or may be in a
  // non-deterministic state.
  public final static String CUT_STACK_THREAD_NAME_REGEX = "Daemon|main";
  public final static Matcher CUT_STACK_THREADS =
      Pattern.compile(CUT_STACK_THREAD_NAME_REGEX).matcher("");

  public static void printAll(Object[][] stacks) {
    List<String> stringified = new ArrayList<String>(stacks.length);

    for (Object[] stackInfo : stacks) {
      Thread t = (Thread)stackInfo[0];
      String name = (t != null) ? t.getName() : "null";
      String stackSerialization;
      if (CUT_STACK_THREADS.reset(name).find()) {
        // Do not print daemon stacks, as they're non-deterministic.
        stackSerialization = "<not printed>";
      } else if (IGNORE_THREADS.reset(name).find()) {
        // Skip IGNORE_THREADS.
        continue;
      } else {
        StringBuilder sb = new StringBuilder();
        for (String[] stackElement : (String[][])stackInfo[1]) {
          for (String part : stackElement) {
            sb.append(' ');
            sb.append(part);
          }
          sb.append('\n');
        }
        stackSerialization = sb.toString();
      }
      stringified.add(name + "\n" + stackSerialization);
    }

    Collections.sort(stringified);

    for (String s : stringified) {
      System.out.println("---------");
      System.out.println(s);
    }
  }

  public static native String[][] getStackTrace(Thread thread, int start, int max);
}
