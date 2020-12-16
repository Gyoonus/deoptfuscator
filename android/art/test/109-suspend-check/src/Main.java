/*
 * Copyright (C) 2013 The Android Open Source Project
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
    private static final int TEST_TIME = 5;

    public static void main(String[] args) {
        System.out.println("Running (" + TEST_TIME + " seconds) ...");
        InfiniteDoWhileLoopWithLong doWhileLoopWithLong = new InfiniteDoWhileLoopWithLong();
        SimpleLoopThread[] simpleLoops = {
                new InfiniteForLoop(),
                new InfiniteWhileLoop(),
                new InfiniteWhileLoopWithIntrinsic(),
                new InfiniteDoWhileLoop(),
                new MakeGarbage(),
                new InfiniteWhileLoopWithSpecialReturnArgOrConst(new SpecialMethods1()),
                new InfiniteWhileLoopWithSpecialReturnArgOrConst(new SpecialMethods2()),
                new InfiniteWhileLoopWithSpecialPutOrNop(new SpecialMethods1()),
                new InfiniteWhileLoopWithSpecialPutOrNop(new SpecialMethods2()),
                new InfiniteWhileLoopWithSpecialConstOrIGet(new SpecialMethods1()),
                new InfiniteWhileLoopWithSpecialConstOrIGet(new SpecialMethods2()),
                new InfiniteWhileLoopWithSpecialConstOrIGetInTryCatch(new SpecialMethods1()),
                new InfiniteWhileLoopWithSpecialConstOrIGetInTryCatch(new SpecialMethods2()),
        };
        doWhileLoopWithLong.start();
        for (SimpleLoopThread loop : simpleLoops) {
            loop.start();
        }
        for (int i = 0; i < TEST_TIME; i++) {
          Runtime.getRuntime().gc();
          System.out.println(".");
          sleep(1000);
        }
        doWhileLoopWithLong.stopNow();
        for (SimpleLoopThread loop : simpleLoops) {
            loop.stopNow();
        }
        System.out.println("Done.");
    }

    public static void sleep(int ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException ie) {
            System.out.println("sleep was interrupted");
        }
    }
}

class SimpleLoopThread extends Thread {
  volatile protected boolean keepGoing = true;
  public void stopNow() {
    keepGoing = false;
  }
}

interface SpecialMethodInterface {
  long ReturnArgOrConst(long arg);
  void PutOrNop(long arg);
  long ConstOrIGet();
}

class SpecialMethods1 implements SpecialMethodInterface {
  public long ReturnArgOrConst(long arg) {
    return 42L;
  }
  public void PutOrNop(long arg) {
  }
  public long ConstOrIGet() {
    return 42L;
  }
}

class SpecialMethods2 implements SpecialMethodInterface {
  public long value = 42L;
  public long ReturnArgOrConst(long arg) {
    return arg;
  }
  public void PutOrNop(long arg) {
    value = arg;
  }
  public long ConstOrIGet() {
    return value;
  }
}

class InfiniteWhileLoopWithSpecialReturnArgOrConst extends SimpleLoopThread {
  private SpecialMethodInterface smi;
  public InfiniteWhileLoopWithSpecialReturnArgOrConst(SpecialMethodInterface smi) {
    this.smi = smi;
  }
  public void run() {
    long i = 0L;
    while (keepGoing) {
      i += smi.ReturnArgOrConst(i);
    }
  }
}

class InfiniteWhileLoopWithSpecialPutOrNop extends SimpleLoopThread {
  private SpecialMethodInterface smi;
  public InfiniteWhileLoopWithSpecialPutOrNop(SpecialMethodInterface smi) {
    this.smi = smi;
  }
  public void run() {
    long i = 0L;
    while (keepGoing) {
      smi.PutOrNop(i);
      i++;
    }
  }
}

class InfiniteWhileLoopWithSpecialConstOrIGet extends SimpleLoopThread {
  private SpecialMethodInterface smi;
  public InfiniteWhileLoopWithSpecialConstOrIGet(SpecialMethodInterface smi) {
    this.smi = smi;
  }
  public void run() {
    long i = 0L;
    while (keepGoing) {
      i += smi.ConstOrIGet();
    }
  }
}

class InfiniteWhileLoopWithSpecialConstOrIGetInTryCatch extends SimpleLoopThread {
  private SpecialMethodInterface smi;
  public InfiniteWhileLoopWithSpecialConstOrIGetInTryCatch(SpecialMethodInterface smi) {
    this.smi = smi;
  }
  public void run() {
    try {
      long i = 0L;
      while (keepGoing) {
        i += smi.ConstOrIGet();
      }
    } catch (Throwable ignored) { }
  }
}

class InfiniteWhileLoopWithIntrinsic extends SimpleLoopThread {
  private String[] strings = { "a", "b", "c", "d" };
  private int sum = 0;
  public void run() {
    int i = 0;
    while (keepGoing) {
      i++;
      sum += strings[i & 3].length();
    }
  }
}

class InfiniteDoWhileLoopWithLong extends Thread {
  volatile private long keepGoing = 7L;
  public void run() {
    int i = 0;
    do {
      i++;
    } while (keepGoing >= 4L);
  }
  public void stopNow() {
    keepGoing = 1L;
  }
}

class InfiniteWhileLoop extends SimpleLoopThread {
  public void run() {
    int i = 0;
    while (keepGoing) {
      i++;
    }
  }
}

class InfiniteDoWhileLoop extends SimpleLoopThread {
  public void run() {
    int i = 0;
    do {
      i++;
    } while (keepGoing);
  }
}

class InfiniteForLoop extends SimpleLoopThread {
  public void run() {
    int i = 0;
    for (int j = 0; keepGoing; j++) {
      i += j;
    }
  }
}

class MakeGarbage extends SimpleLoopThread {
  public void run() {
    while (keepGoing) {
      byte[] garbage = new byte[100000];
    }
  }
}
