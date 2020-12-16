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

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.Callable;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * A test driver for JIT compiling methods and looking for JIT
 * cache issues.
 */
public class JitCacheChurnTest {
  /* The name of methods to JIT */
  private static final String JITTED_METHOD = "$noinline$Call";

  /* The number of cores to oversubscribe load by. */
  private static final int OVERSUBSCRIBED_CORES = 1;

  /* The number of concurrent executions of methods to be JIT compiled. */
  private static final int CONCURRENCY =
      Runtime.getRuntime().availableProcessors() + OVERSUBSCRIBED_CORES;

  /* The number of times the methods to be JIT compiled should be executed per thread. */
  private static final int METHOD_ITERATIONS = 10;

  /* Number of test iterations JIT methods and removing methods from JIT cache. */
  private static final int TEST_ITERATIONS = 512;

  /* Tasks to run and generate compiled code of various sizes */
  private static final BaseTask [] TASKS = {
    new TaskOne(), new TaskTwo(), new TaskThree(), new TaskFour(), new TaskFive(), new TaskSix(),
    new TaskSeven(), new TaskEight(), new TaskNine(), new TaskTen()
  };
  private static final int TASK_BITMASK = (1 << TASKS.length) - 1;

  private final ExecutorService executorService;
  private int runMask = 0;

  private JitCacheChurnTest() {
    this.executorService = new ThreadPoolExecutor(CONCURRENCY, CONCURRENCY, 5000,
        TimeUnit.MILLISECONDS,new LinkedBlockingQueue<>());
  }

  private void shutdown() {
    this.executorService.shutdown();
  }

  private void runTasks(Callable<Integer> task) {
    // Force JIT compilation of tasks method.
    ensureJitCompiled(task.getClass(), JITTED_METHOD);

    // Launch worker threads to run JIT compiled method.
    try {
      ArrayList<Callable<Integer>> tasks = new ArrayList<>(CONCURRENCY);
      for (int i = 0; i < CONCURRENCY; ++i) {
        tasks.add(i, task);
      }

      List<Future<Integer>> results = executorService.invokeAll(tasks);
      for (Future<?> result : results) {
        result.get();
      }
    } catch (InterruptedException | ExecutionException e) {
      System.err.println(e);
      System.exit(-1);
    }
  }

  private static abstract class BaseTask implements Callable<Integer> {
    private static CyclicBarrier barrier = new CyclicBarrier(CONCURRENCY);

    public Integer call() throws Exception {
      barrier.await();
      int iterations = METHOD_ITERATIONS + 1;
      for (int i = 0; i < iterations; ++i) {
        $noinline$Call();
      }
      return $noinline$Call();
    }

    protected abstract Integer $noinline$Call();
  }

  private static class TaskOne extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      return null;
    }
  }

  private static class TaskTwo extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      return 0;
    }
  }

  private static class TaskThree extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      int sum = 0;
      for (int i = 0; i < 3; ++i) {
        sum = i * (i + 1);
      }
      return sum;
    }
  }

  private static class TaskFour extends BaseTask {
    @Override
    protected Integer $noinline$Call() {
      int sum = 0;
      for (int i = 0; i < 10; ++i) {
        int bits = i;
        bits = ((bits >>> 1) & 0x55555555) | ((bits << 1) & 0x55555555);
        bits = ((bits >>> 2) & 0x33333333) | ((bits << 2) & 0x33333333);
        bits = ((bits >>> 4) & 0x0f0f0f0f) | ((bits << 4) & 0x0f0f0f0f);
        bits = ((bits >>> 8) & 0x00ff00ff) | ((bits << 8) & 0x00ff00ff);
        bits = (bits >>> 16) | (bits << 16);
        sum += bits;
      }
      return sum;
    }
  }

  private static class TaskFive extends BaseTask {
    static final AtomicInteger instances = new AtomicInteger(0);
    int instance;
    TaskFive() {
      instance = instances.getAndIncrement();
    }
    protected Integer $noinline$Call() {
      return instance;
    }
  }

  private static class TaskSix extends TaskFive {
    protected Integer $noinline$Call() {
      return instance + 1;
    }
  }

  private static class TaskSeven extends TaskFive {
    protected Integer $noinline$Call() {
      return 2 * instance + 1;
    }
  }

  private static class TaskEight extends TaskFive {
    protected Integer $noinline$Call() {
      double a = Math.cosh(2.22 * instance);
      double b = a / 2;
      double c = b * 3;
      double d = a + b + c;
      if (d > 42) {
        d *= Math.max(Math.sin(d), Math.sinh(d));
        d *= Math.max(1.33, 0.17 * Math.sinh(d));
        d *= Math.max(1.34, 0.21 * Math.sinh(d));
        d *= Math.max(1.35, 0.32 * Math.sinh(d));
        d *= Math.max(1.36, 0.41 * Math.sinh(d));
        d *= Math.max(1.37, 0.57 * Math.sinh(d));
        d *= Math.max(1.38, 0.61 * Math.sinh(d));
        d *= Math.max(1.39, 0.79 * Math.sinh(d));
        d += Double.parseDouble("3.711e23");
      }

      if (d > 3) {
        return (int) a;
      } else {
        return (int) b;
      }
    }
  }

  private static class TaskNine extends TaskFive {
    private final String [] numbers = { "One", "Two", "Three", "Four", "Five", "Six" };

    protected Integer $noinline$Call() {
      String number = numbers[instance % numbers.length];
      return number.length();
    }
  }

  private static class TaskTen extends TaskFive {
    private final String [] numbers = { "12345", "23451", "34512", "78901", "89012" };

    protected Integer $noinline$Call() {
      int odd = 0;
      String number = numbers[instance % numbers.length];
      for (int i = 0; i < number.length(); i += 2) {
        odd += Integer.parseInt(numbers[i]);
      }
      odd *= 3;

      int even = 0;
      for (int i = 1; i < number.length(); i += 2) {
        even += Integer.parseInt(numbers[i]);
      }
      return (odd + even) % 10;
    }
  }

  private void runAndJitMethods(int mask) {
    runMask |= mask;
    for (int index = 0; mask != 0; mask >>= 1, index++) {
      if ((mask & 1) == 1) {
        runTasks(TASKS[index]);
      }
    }
  }

  private static void ensureJitCompiled(Class<?> klass, String name) {
    Main.ensureJitCompiled(klass, name);
  }

  private void removeJittedMethod(Class<?> klass, String name) {
    Method method = null;
    try {
      method = klass.getDeclaredMethod(name);
    } catch (NoSuchMethodException e) {
      System.err.println(e);
      System.exit(-1);
    }
    removeJitCompiledMethod(method, false);
  }

  private void removeJittedMethods(int mask) {
    mask = mask & runMask;
    runMask ^= mask;
    for (int index = 0; mask != 0; mask >>= 1, index++) {
      if ((mask & 1) == 1) {
        removeJittedMethod(TASKS[index].getClass(), JITTED_METHOD);
      }
    }
  }

  private static int getMethodsAsMask(Random rng) {
    return rng.nextInt(TASK_BITMASK) + 1;
  }

  public static void run() {
    JitCacheChurnTest concurrentExecution = new JitCacheChurnTest();
    Random invokeMethodGenerator = new Random(5);
    Random removeMethodGenerator = new Random(7);
    try {
      for (int i = 0; i < TEST_ITERATIONS; ++i) {
        concurrentExecution.runAndJitMethods(getMethodsAsMask(invokeMethodGenerator));
        concurrentExecution.removeJittedMethods(getMethodsAsMask(removeMethodGenerator));
      }
    } finally {
      concurrentExecution.shutdown();
    }
  }

  private static native void removeJitCompiledMethod(Method method, boolean releaseMemory);
}
