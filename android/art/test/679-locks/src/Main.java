/*
 * Copyright (C) 2018 The Android Open Source Project
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

class NotLoaded {
  public void foo() {}
}

public class Main {
    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        TestSync.run();
    }

    public static void run() {
        testVisitLocks();
    }

    static Object myStatic;

    // Note: declared in 167-visit-locks.
    public static native void testVisitLocks();
}

// 167-visit-locks/visit-locks.cc looks at the locks held in TestSync.run().
class TestSync {
  public static void run() {
    Object o = Main.myStatic;
    if (o != null) {
      if (o instanceof NotLoaded) {
        ((NotLoaded)o).foo();
      }
    }
    synchronized ("MyString") {
      Main.testVisitLocks();
    }
  }
}
