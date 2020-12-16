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

import dexfuzz.listeners.BaseListener;

/**
 * For timing splits of program execution.
 */
public class Timer {
  /**
   * The name of the timer, the phase of the program it is intended to time.
   */
  private String name;

  /**
   * A point in time remembered when start() is called.
   */
  private long startPoint;

  /**
   * A cumulative count of how much time has elapsed. Updated each time
   * stop() is called.
   */
  private long elapsedTime;

  /**
   * Initialise a new timer with the provided name.
   */
  public Timer(String name) {
    this.name = name;
    this.elapsedTime = 0L;
  }

  /**
   * Start timing.
   */
  public void start() {
    startPoint = System.currentTimeMillis();
  }

  /**
   * Stop timing, update how much time has elapsed.
   */
  public void stop() {
    long endPoint = System.currentTimeMillis();
    elapsedTime += (endPoint - startPoint);
  }

  /**
   * Log the elapsed time this timer has recorded.
   */
  public void printTime(BaseListener listener) {
    listener.handleTiming(name, ((float)elapsedTime) / 1000.0f);
  }
}
