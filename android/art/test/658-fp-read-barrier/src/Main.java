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

public class Main {
  static volatile boolean done = false;

  public static void main(String[] args) {
    // Run a thread for 30 seconds, allocating memory and triggering garbage
    // collection.
    // Time is limited to 30 seconds to not make this test too long. The test used
    // to trigger the failure around 1 every 10 runs.
    Thread t = new Thread() {
      public void run() {
        long time = System.currentTimeMillis();
        while (System.currentTimeMillis() - time < 30000) {
          for (int j = 0; j < 10000; j++) {
            o = new Object[1000];
          }
          Runtime.getRuntime().gc();
          Thread.yield();
        }
        Main.done = true;
      }
      Object o;
    };
    // Make the thread a daemon to quit early in case of an
    // exception thrown below.
    t.setDaemon(true);
    t.start();

    // Run 'foo' as long as the test runs.
    while (!done) {
      double res = foo(staticMain);
      if (res != 529.0) {
        throw new Error("Unexpected result " + res);
      }
    }
  }

  public static double foo(Main main) {
    // Use up all D registers on arm64.
    double d1 = main.field1;
    double d2 = main.field2;
    double d3 = main.field3;
    double d4 = main.field4;
    double d5 = main.field5;
    double d6 = main.field6;
    double d7 = main.field7;
    double d8 = main.field8;
    double d9 = main.field9;
    double d10 = main.field10;
    double d11 = main.field11;
    double d12 = main.field12;
    double d13 = main.field13;
    double d14 = main.field14;
    double d15 = main.field15;
    double d16 = main.field16;
    double d17 = main.field17;
    double d18 = main.field18;
    double d19 = main.field19;
    double d20 = main.field20;
    double d21 = main.field21;
    double d22 = main.field22;
    double d23 = main.field23;
    double d24 = main.field24;
    double d25 = main.field25;
    double d26 = main.field26;
    double d27 = main.field27;
    double d28 = main.field28;
    double d29 = main.field29;
    double d30 = main.field30;
    double d31 = main.field31;
    double d32 = main.field32;

    // Trigger a read barrier. This used to make the test trip on ARM64 as
    // the read barrier stub used to not restore the D registers.
    double p = main.objectField.field1;

    return p + d1 + d2 + d3 + d4 + d5 + d6 + d7 + d8 + d9 + d10 + d11 + d12 +
        d13 + d14 + d15 + d16 + d17 + d18 + d19 + d20 + d21 + d22 + d23 + d24 +
        d25 + d26 + d27 + d28 + d29 + d30 + d31 + d32;
  }

  // Initialize objects here and not in 'main' to avoid having
  // these objects in roots.
  public static Main staticMain = new Main();
  static {
    staticMain.objectField = new Main();
  }

  public Main objectField;

  public double field1 = 1.0;
  public double field2 = 2.0;
  public double field3 = 3.0;
  public double field4 = 4.0;
  public double field5 = 5.0;
  public double field6 = 6.0;
  public double field7 = 7.0;
  public double field8 = 8.0;
  public double field9 = 9.0;
  public double field10 = 10.0;
  public double field11 = 11.0;
  public double field12 = 12.0;
  public double field13 = 13.0;
  public double field14 = 14.0;
  public double field15 = 15.0;
  public double field16 = 16.0;
  public double field17 = 17.0;
  public double field18 = 18.0;
  public double field19 = 19.0;
  public double field20 = 20.0;
  public double field21 = 21.0;
  public double field22 = 22.0;
  public double field23 = 23.0;
  public double field24 = 24.0;
  public double field25 = 25.0;
  public double field26 = 26.0;
  public double field27 = 27.0;
  public double field28 = 28.0;
  public double field29 = 29.0;
  public double field30 = 30.0;
  public double field31 = 31.0;
  public double field32 = 32.0;
}
