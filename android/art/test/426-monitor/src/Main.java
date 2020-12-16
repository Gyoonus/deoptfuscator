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

public class Main {
  public static void main(String[] args) {
    $opt$StaticSynchronizedMethod();
    new Main().$opt$InstanceSynchronizedMethod();
    $opt$SynchronizedBlock();
    new Main().$opt$DoubleInstanceSynchronized();
    $opt$DoubleStaticSynchronized();
  }

  public static synchronized void $opt$StaticSynchronizedMethod() {
    System.out.println("In static method");
  }

  public synchronized void $opt$InstanceSynchronizedMethod() {
    System.out.println("In instance method");
  }

  public static void $opt$SynchronizedBlock() {
    Object o = new Object();
    synchronized(o) {
      System.out.println("In synchronized block");
    }
  }

  public synchronized void $opt$DoubleInstanceSynchronized() {
    synchronized (this) {
      System.out.println("In second instance method");
    }
  }

  public synchronized static void $opt$DoubleStaticSynchronized() {
    synchronized (Main.class) {
      System.out.println("In second static method");
    }
  }
}
