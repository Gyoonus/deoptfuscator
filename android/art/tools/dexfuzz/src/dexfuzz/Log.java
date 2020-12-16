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

package dexfuzz;

/**
 * Provides access to the logging facilities of dexfuzz.
 */
public class Log {
  private static LogTag threshold = LogTag.ERROR;

  // Disable the constructor for this class.
  private Log() { }

  public static enum LogTag {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    ALWAYS
  }

  public static void setLoggingLevel(LogTag tag) {
    threshold = tag;
  }

  public static boolean likelyToLog() {
    return (threshold.ordinal() < LogTag.ERROR.ordinal());
  }

  public static void debug(String msg) {
    log(LogTag.DEBUG, msg);
  }

  public static void info(String msg) {
    log(LogTag.INFO, msg);
  }

  public static void warn(String msg) {
    log(LogTag.WARN, msg);
  }

  public static void error(String msg) {
    log(LogTag.ERROR, msg);
  }

  public static void always(String msg) {
    System.out.println(msg);
  }

  private static void log(LogTag tag, String msg) {
    if (tag.ordinal() >= threshold.ordinal()) {
      System.out.println("[" + tag.toString() + "] " + msg);
    }
  }

  /**
   * Reports error and then terminates the program.
   */
  public static void errorAndQuit(String msg) {
    error(msg);
    // TODO: Signal sleeping threads.
    System.exit(1);
  }
}
