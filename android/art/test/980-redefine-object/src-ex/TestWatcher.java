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

package art.test;

import java.util.concurrent.locks.ReentrantLock;

public class TestWatcher {
  // Lock to synchronize access to the static state of this class.
  private static final ReentrantLock lock = new ReentrantLock();
  private static volatile boolean criticalFailure = false;
  private static boolean reportingEnabled = true;
  private static boolean doingReport = false;

  private static void MonitorEnter() {
    lock.lock();
  }

  private static void MonitorExit() {
    // Need to do this manually since we need to notify critical failure but would deadlock if
    // waited for the unlock.
    if (!lock.isHeldByCurrentThread()) {
      NotifyCriticalFailure();
      throw new IllegalMonitorStateException("Locking error!");
    } else {
      lock.unlock();
    }
  }

  // Stops reporting. Must be paired with an EnableReporting call.
  public static void DisableReporting() {
    MonitorEnter();
    reportingEnabled = false;
  }

  // Stops reporting. Must be paired with a DisableReporting call.
  public static void EnableReporting() {
    reportingEnabled = true;
    MonitorExit();
  }

  public static void NotifyCriticalFailure() {
    criticalFailure = true;
  }

  public static void NotifyConstructed(Object o) {
    if (criticalFailure) {
      // Something went very wrong. We are probably trying to report it so don't get in the way.
      return;
    }
    MonitorEnter();
    // We could enter an infinite loop if println allocates (which it does) so we disable
    // reporting while we are doing a report. Since we are synchronized we won't miss any
    // allocations.
    if (reportingEnabled && !doingReport) {
      doingReport = true;
      System.out.println("Object allocated of type '" + o.getClass().getName() + "'");
      doingReport = false;
    }
    MonitorExit();
  }
}
