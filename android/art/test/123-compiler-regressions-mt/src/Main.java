/*
 * Copyright (C) 2009 The Android Open Source Project
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

import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Test for Jit regressions.
 */
public class Main {
    public static void main(String args[]) throws Exception {
        b17689750TestVolatile();
        b17689750TestMonitor();
    }

    static void b17689750TestVolatile() {
        final B17689750TestVolatile test = new B17689750TestVolatile();
        new Thread() {
            public void run() {
                test.thread1();
            }
        }.start();
        try {
            test.thread2();
        } catch (NullPointerException expected) {
            System.out.println("b17689750TestVolatile passed.");
        }
    }

    static void b17689750TestMonitor() {
      final B17689750TestMonitor test = new B17689750TestMonitor();
      new Thread() {
        public void run() {
          test.thread1();
        }
      }.start();
      try {
        test.thread2();
      } catch (NullPointerException expected) {
        System.out.println("b17689750TestMonitor passed.");
      }
    }
}

class B17689750TestVolatile {
  private volatile int state = 0;
  private int[] values = { 42 };

  void thread1() {
    while (state != 1) { }  // Busy loop.
    values = null;
    state = 2;
  }

  void thread2() {
    int[] vs1 = values;
    state = 1;
    while (state != 2) { }  // Busy loop.
    int[] vs2 = values;
    int v1 = vs1[0];
    int v2 = vs2[0];
    System.out.println("b17689750TestVolatile failed: " + v1 + ", " + v2);
  }
}

class B17689750TestMonitor {
  private int state = 0;
  private Object lock = new Object();
  private int[] values = { 42 };

  void thread1() {
    int s;
    do {
      synchronized (lock) {
        s = state;
      }
    } while (s != 1);  // Busy loop.

    synchronized (lock) {
      values = null;
      state = 2;
    }
  }

  void thread2() {
    int[] vs1;
    synchronized (lock) {
      vs1 = values;
      state = 1;
    }

    int s;
    do {
      synchronized (lock) {
        s = state;
      }
    } while (s != 2);  // Busy loop.

    int[] vs2 = values;
    int v1 = vs1[0];
    int v2 = vs2[0];
    System.out.println("b17689750TestMonitor failed: " + v1 + ", " + v2);
  }
}
