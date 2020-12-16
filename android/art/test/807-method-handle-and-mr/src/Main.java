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

// This test was inspired by benchmarks.MicroMethodHandles.java.MicroMethodHandles.

import java.io.PrintStream;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

class A {
  public Long binaryFunction(int x, double y) {
    return 1000l;
  }
}

class Test {
  Test() throws Throwable {
    this.handle = MethodHandles.lookup().findVirtual(A.class, "binaryFunction",
                                                     MethodType.methodType(Long.class, int.class,
                                                                           double.class));
    this.a = new A();
    this.x = new Integer(72);
    this.y = new Double(-1.39e-31);
  }

  void execute() {
    try {
      executeFor(2000);
      System.out.println(getName());
    } catch (Throwable t) {
      System.err.println("Exception during the execution of " + getName());
      System.err.println(t);
      t.printStackTrace(new PrintStream(System.err));
      System.exit(1);
    }
  }

  void executeFor(long timeMinimumMillis) throws Throwable {
    long startTime = System.currentTimeMillis();
    long elapsed = 0;
    while (elapsed < timeMinimumMillis) {
      exercise();
      elapsed = System.currentTimeMillis() - startTime;
    }
  }

  void exercise() throws Throwable {
    for (int i = 0; i < EXERCISE_ITERATIONS; ++i) {
      run();
    }
  }

  void run() throws Throwable {
    long result = (long) handle.invoke(a, x, y);
  }

  String getName() {
    return getClass().getSimpleName();
  }

  private static final int EXERCISE_ITERATIONS = 500;

  private MethodHandle handle;
  private A a;
  private Integer x;
  private Double y;
}

public class Main {
  public static void main(String[] args) throws Throwable {
    Test[] tests = new Test[] { new Test(), new Test(), new Test() };
    for (Test test : tests) {
      test.execute();
    }
    System.out.println("passed");
  }
}
