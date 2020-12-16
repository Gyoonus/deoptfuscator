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

import java.lang.reflect.Field;
import java.util.concurrent.atomic.AtomicBoolean;

import sun.misc.Unsafe;

/**
 * Checker test on the 1.8 unsafe operations. Note, this is by no means an
 * exhaustive unit test for these CAS (compare-and-swap) and fence operations.
 * Instead, this test ensures the methods are recognized as intrinsic and behave
 * as expected.
 */
public class Main {

  private static final Unsafe unsafe = getUnsafe();

  private static Thread[] sThreads = new Thread[10];

  //
  // Fields accessed by setters and adders, and by memory fence tests.
  //

  public int i = 0;
  public long l = 0;
  public Object o = null;

  public int x_value;
  public int y_value;
  public volatile boolean running;

  //
  // Setters.
  //

  /// CHECK-START: int Main.set32(java.lang.Object, long, int) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:i\d+>> InvokeVirtual intrinsic:UnsafeGetAndSetInt
  /// CHECK-DAG:                 Return [<<Result>>]
  private static int set32(Object o, long offset, int newValue) {
    return unsafe.getAndSetInt(o, offset, newValue);
  }

  /// CHECK-START: long Main.set64(java.lang.Object, long, long) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:j\d+>> InvokeVirtual intrinsic:UnsafeGetAndSetLong
  /// CHECK-DAG:                 Return [<<Result>>]
  private static long set64(Object o, long offset, long newValue) {
    return unsafe.getAndSetLong(o, offset, newValue);
  }

  /// CHECK-START: java.lang.Object Main.setObj(java.lang.Object, long, java.lang.Object) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:l\d+>> InvokeVirtual intrinsic:UnsafeGetAndSetObject
  /// CHECK-DAG:                 Return [<<Result>>]
  private static Object setObj(Object o, long offset, Object newValue) {
    return unsafe.getAndSetObject(o, offset, newValue);
  }

  //
  // Adders.
  //

  /// CHECK-START: int Main.add32(java.lang.Object, long, int) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:i\d+>> InvokeVirtual intrinsic:UnsafeGetAndAddInt
  /// CHECK-DAG:                 Return [<<Result>>]
  private static int add32(Object o, long offset, int delta) {
    return unsafe.getAndAddInt(o, offset, delta);
  }

  /// CHECK-START: long Main.add64(java.lang.Object, long, long) intrinsics_recognition (after)
  /// CHECK-DAG: <<Result:j\d+>> InvokeVirtual intrinsic:UnsafeGetAndAddLong
  /// CHECK-DAG:                 Return [<<Result>>]
  private static long add64(Object o, long offset, long delta) {
    return unsafe.getAndAddLong(o, offset, delta);
  }

  //
  // Fences (native).
  //

  /// CHECK-START: void Main.load() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeVirtual intrinsic:UnsafeLoadFence
  //
  /// CHECK-START: void Main.load() instruction_simplifier (after)
  /// CHECK-NOT: InvokeVirtual intrinsic:UnsafeLoadFence
  //
  /// CHECK-START: void Main.load() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:LoadAny
  private static void load() {
    unsafe.loadFence();
  }

  /// CHECK-START: void Main.store() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeVirtual intrinsic:UnsafeStoreFence
  //
  /// CHECK-START: void Main.store() instruction_simplifier (after)
  /// CHECK-NOT: InvokeVirtual intrinsic:UnsafeStoreFence
  //
  /// CHECK-START: void Main.store() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:AnyStore
  private static void store() {
    unsafe.storeFence();
  }

  /// CHECK-START: void Main.full() intrinsics_recognition (after)
  /// CHECK-DAG: InvokeVirtual intrinsic:UnsafeFullFence
  //
  /// CHECK-START: void Main.full() instruction_simplifier (after)
  /// CHECK-NOT: InvokeVirtual intrinsic:UnsafeFullFence
  //
  /// CHECK-START: void Main.full() instruction_simplifier (after)
  /// CHECK-DAG: MemoryBarrier kind:AnyAny
  private static void full() {
    unsafe.fullFence();
  }

  //
  // Thread fork/join.
  //

  private static void fork(Runnable r) {
    for (int i = 0; i < 10; i++) {
      sThreads[i] = new Thread(r);
    }
    // Start the threads only after the full array has been written with new threads,
    // because one test relies on the contents of this array to be consistent.
    for (int i = 0; i < 10; i++) {
      sThreads[i].start();
    }
  }

  private static void join() {
    try {
      for (int i = 0; i < 10; i++) {
        sThreads[i].join();
      }
    } catch (InterruptedException e) {
      throw new Error("Failed join: " + e);
    }
  }

  //
  // Driver.
  //

  public static void main(String[] args) {
    System.out.println("starting");

    final Main m = new Main();

    // Get the offsets.

    final long intOffset, longOffset, objOffset;
    try {
      Field intField = Main.class.getDeclaredField("i");
      Field longField = Main.class.getDeclaredField("l");
      Field objField = Main.class.getDeclaredField("o");

      intOffset = unsafe.objectFieldOffset(intField);
      longOffset = unsafe.objectFieldOffset(longField);
      objOffset = unsafe.objectFieldOffset(objField);

    } catch (NoSuchFieldException e) {
      throw new Error("No offset: " + e);
    }

    // Some sanity on setters and adders within same thread.

    set32(m, intOffset, 3);
    expectEqual32(3, m.i);

    set64(m, longOffset, 7L);
    expectEqual64(7L, m.l);

    setObj(m, objOffset, m);
    expectEqualObj(m, m.o);

    add32(m, intOffset, 11);
    expectEqual32(14, m.i);

    add64(m, longOffset, 13L);
    expectEqual64(20L, m.l);

    // Some sanity on setters within different threads.

    fork(new Runnable() {
      public void run() {
        for (int i = 0; i < 10; i++)
          set32(m, intOffset, i);
      }
    });
    join();
    expectEqual32(9, m.i);  // one thread's last value wins

    fork(new Runnable() {
      public void run() {
        for (int i = 0; i < 10; i++)
          set64(m, longOffset, (long) (100 + i));
      }
    });
    join();
    expectEqual64(109L, m.l);  // one thread's last value wins

    fork(new Runnable() {
      public void run() {
        for (int i = 0; i < 10; i++)
          setObj(m, objOffset, sThreads[i]);
      }
    });
    join();
    expectEqualObj(sThreads[9], m.o);  // one thread's last value wins

    // Some sanity on adders within different threads.

    fork(new Runnable() {
      public void run() {
        for (int i = 0; i < 10; i++)
          add32(m, intOffset, i + 1);
      }
    });
    join();
    expectEqual32(559, m.i);  // all values accounted for

    fork(new Runnable() {
      public void run() {
        for (int i = 0; i < 10; i++)
          add64(m, longOffset, (long) (i + 1));
      }
    });
    join();
    expectEqual64(659L, m.l);  // all values accounted for

    // Some sanity on fences within same thread. Note that memory fences within one
    // thread make little sense, but the sanity check ensures nothing bad happens.

    m.i = -1;
    m.l = -2L;
    m.o = null;

    load();
    store();
    full();

    expectEqual32(-1, m.i);
    expectEqual64(-2L, m.l);
    expectEqualObj(null, m.o);

    // Some sanity on full fence within different threads. We write the non-volatile m.l after
    // the fork(), which means there is no happens-before relation in the Java memory model
    // with respect to the read in the threads. This relation is enforced by the memory fences
    // and the weak-set() -> get() guard. Note that the guard semantics used here are actually
    // too strong and already enforce total memory visibility, but this test illustrates what
    // should still happen if Java had a true relaxed memory guard.

    final AtomicBoolean guard1 = new AtomicBoolean();
    m.l = 0L;

    fork(new Runnable() {
      public void run() {
        while (!guard1.get());  // busy-waiting
        full();
        expectEqual64(-123456789L, m.l);
      }
    });

    m.l = -123456789L;
    full();
    while (!guard1.weakCompareAndSet(false, true));  // relaxed memory order
    join();

    // Some sanity on release/acquire fences within different threads. We write the non-volatile
    // m.l after the fork(), which means there is no happens-before relation in the Java memory
    // model with respect to the read in the threads. This relation is enforced by the memory fences
    // and the weak-set() -> get() guard. Note that the guard semantics used here are actually
    // too strong and already enforce total memory visibility, but this test illustrates what
    // should still happen if Java had a true relaxed memory guard.

    final AtomicBoolean guard2 = new AtomicBoolean();
    m.l = 0L;

    fork(new Runnable() {
      public void run() {
        while (!guard2.get());  // busy-waiting
        load();
        expectEqual64(-987654321L, m.l);
      }
    });

    m.l = -987654321L;
    store();
    while (!guard2.weakCompareAndSet(false, true));  // relaxed memory order
    join();

    // Some sanity on release/acquire fences within different threads using a test suggested by
    // Hans Boehm. Even this test remains with the realm of sanity only, since having the threads
    // read the same value consistently would be a valid outcome.

    m.x_value = -1;
    m.y_value = -1;
    m.running = true;

    fork(new Runnable() {
      public void run() {
        while (m.running) {
          for (int few_times = 0; few_times < 1000; few_times++) {
            // Read y first, then load fence, then read x.
            // They should appear in order, if seen at all.
            int local_y = m.y_value;
            load();
            int local_x = m.x_value;
            expectLessThanOrEqual32(local_y, local_x);
          }
        }
      }
    });

    for (int many_times = 0; many_times < 100000; many_times++) {
      m.x_value = many_times;
      store();
      m.y_value = many_times;
    }
    m.running = false;
    join();

    // All done!

    System.out.println("passed");
  }

  // Use reflection to implement "Unsafe.getUnsafe()";
  private static Unsafe getUnsafe() {
    try {
      Class<?> unsafeClass = Unsafe.class;
      Field f = unsafeClass.getDeclaredField("theUnsafe");
      f.setAccessible(true);
      return (Unsafe) f.get(null);
    } catch (Exception e) {
      throw new Error("Cannot get Unsafe instance");
    }
  }

  private static void expectEqual32(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectLessThanOrEqual32(int val1, int val2) {
    if (val1 > val2) {
      throw new Error("Expected: " + val1 + " <= " + val2);
    }
  }

  private static void expectEqual64(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEqualObj(Object expected, Object result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
