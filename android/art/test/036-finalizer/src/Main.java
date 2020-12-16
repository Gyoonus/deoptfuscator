/*
 * Copyright (C) 2008 The Android Open Source Project
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

import java.lang.ref.Reference;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * Some finalizer tests.
 *
 * This only works if System.runFinalization() causes finalizers to run
 * immediately or very soon.
 */
public class Main {
    private final static boolean isDalvik = System.getProperty("java.vm.name").equals("Dalvik");

    private static void snooze(int ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ie) {
            System.out.println("Snooze: " + ie.getMessage());
        }
    }

    public static WeakReference<FinalizerTest> makeRef() {
        FinalizerTest ft = new FinalizerTest("wahoo");
        WeakReference<FinalizerTest> ref = new WeakReference<FinalizerTest>(ft);
        ft = null;
        return ref;
    }

    public static String wimpString(final WeakReference<FinalizerTest> wimp) {
        /*
         * Do the work in another thread, so there is no danger of a
         * conservative reference to ft leaking onto the main thread's
         * stack.
         */

        final String[] s = new String[1];
        Thread t = new Thread() {
                public void run() {
                    FinalizerTest ref = wimp.get();
                    if (ref != null) {
                        s[0] = ref.toString();
                    }
                }
            };

        t.start();

        try {
            t.join();
        } catch (InterruptedException ie) {
            throw new RuntimeException(ie);
        }

        return s[0];
    }

    private static void printWeakReference(WeakReference<FinalizerTest> wimp) {
        // Reference ft so we are sure the WeakReference cannot be cleared.
        FinalizerTest keepLive = wimp.get();
        System.out.println("wimp: " + wimpString(wimp));
        Reference.reachabilityFence(keepLive);
    }

    public static void main(String[] args) {
        WeakReference<FinalizerTest> wimp = makeRef();
        printWeakReference(wimp);

        /* this will try to collect and finalize ft */
        System.out.println("gc");
        Runtime.getRuntime().gc();

        System.out.println("wimp: " + wimpString(wimp));
        System.out.println("finalize");
        System.runFinalization();
        System.out.println("wimp: " + wimpString(wimp));

        System.out.println("sleep");
        snooze(1000);

        System.out.println("reborn: " + FinalizerTest.mReborn);
        System.out.println("wimp: " + wimpString(wimp));
        System.out.println("reset reborn");
        Runtime.getRuntime().gc();
        FinalizerTest.mReborn = FinalizerTest.mNothing;
        System.out.println("gc + finalize");
        System.gc();
        System.runFinalization();

        System.out.println("sleep");
        snooze(1000);

        System.out.println("reborn: " + FinalizerTest.mReborn);
        System.out.println("wimp: " + wimpString(wimp));
        // Test runFinalization with multiple objects.
        runFinalizationTest();
    }

    static class FinalizeCounter {
      public static final int maxCount = 1024;
      public static boolean finalized[] = new boolean[maxCount];
      private static Object finalizeLock = new Object();
      private static volatile int finalizeCount = 0;
      private int index;
      static int getCount() {
        return finalizeCount;
      }
      static void printNonFinalized() {
        for (int i = 0; i < maxCount; ++i) {
          if (!FinalizeCounter.finalized[i]) {
            System.out.println("Element " + i + " was not finalized");
          }
        }
      }
      FinalizeCounter(int index) {
        this.index = index;
      }
      protected void finalize() {
        synchronized(finalizeLock) {
          ++finalizeCount;
          finalized[index] = true;
        }
      }
    }

    private static void allocFinalizableObjects(int count) {
      Object[] objs = new Object[count];
      for (int i = 0; i < count; ++i) {
        objs[i] = new FinalizeCounter(i);
      }
    }

    private static void runFinalizationTest() {
      allocFinalizableObjects(FinalizeCounter.maxCount);
      Runtime.getRuntime().gc();
      System.runFinalization();
      if (FinalizeCounter.getCount() != FinalizeCounter.maxCount) {
        if (isDalvik) {
          // runFinalization is "expend effort", only ART makes a strong effort all finalizers ran.
          System.out.println("Finalized " + FinalizeCounter.getCount() + " / "  + FinalizeCounter.maxCount);
          // Print out all the finalized elements.
          FinalizeCounter.printNonFinalized();
        }
        // Try to sleep for a couple seconds to see if the objects became finalized after.
        try {
          java.lang.Thread.sleep(2000);
        } catch (InterruptedException e) {
          throw new AssertionError(e);
        }
      }
      System.out.println("After sleep finalized " + FinalizeCounter.getCount() + " / "  + FinalizeCounter.maxCount);
      FinalizeCounter.printNonFinalized();
    }

    public static class FinalizerTest {
        public static FinalizerTest mNothing = new FinalizerTest("nothing");
        public static FinalizerTest mReborn = mNothing;

        private final String message;
        private boolean finalized = false;

        public FinalizerTest(String message) {
            this.message = message;
        }

        public String toString() {
            return "[FinalizerTest message=" + message +
                    ", finalized=" + finalized + "]";
        }

        protected void finalize() {
            finalized = true;
            mReborn = this;
        }
    }
}
