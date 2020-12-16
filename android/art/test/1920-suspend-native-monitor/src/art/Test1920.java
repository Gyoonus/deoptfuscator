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

package art;

public class Test1920 {
  public static void run() throws Exception {
    final Thread spinner = new Thread(() -> {
      nativeSpin();
    }, "Spinner");

    final Thread resumer = new Thread(() -> {
      String me = Thread.currentThread().getName();

      // wait for the other thread to start spinning.
      while (!isNativeThreadSpinning()) { }

      System.out.println(me + ": isNativeThreadSpinning() = " + isNativeThreadSpinning());
      System.out.println(me + ": isSuspended(spinner) = " + Suspension.isSuspended(spinner));

      // Make the native thread wait before calling monitor-enter.
      pause();
      // Reset the marker bit.
      reset();
      // Suspend it from java.
      Suspension.suspend(spinner);
      // Let the thread try to lock the monitor.
      resume();

      // Wait for the other thread to do something.
      try { Thread.sleep(1000); } catch (Exception e) {}

      System.out.println(me + ": Suspended spinner while native spinning");
      System.out.println(me + ": isNativeThreadSpinning() = " + isNativeThreadSpinning());
      System.out.println(me + ": isSuspended(spinner) = " + Suspension.isSuspended(spinner));

      // Resume it from java. It is still native spinning.
      Suspension.resume(spinner);

      System.out.println(me + ": resumed spinner while native spinning");

      // wait for the other thread to start spinning again.
      while (!isNativeThreadSpinning()) { }

      System.out.println(me + ": isNativeThreadSpinning() = " + isNativeThreadSpinning());
      System.out.println(me + ": isSuspended(spinner) = " + Suspension.isSuspended(spinner));
      nativeFinish();
    }, "Resumer");

    spinner.start();
    resumer.start();

    spinner.join();
    resumer.join();
  }

  public static native void nativeSpin();
  public static native void nativeFinish();
  public static native void reset();
  public static native void pause();
  public static native void resume();
  public static native boolean isNativeThreadSpinning();
}
