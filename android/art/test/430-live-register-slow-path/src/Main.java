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
   $opt$TestSlowPath();
  }

  public static void $opt$TestSlowPath() {
    Object[] o = bar();
    assertEquals(0, o.length);
    // The slowpath of the instanceof requires the live register
    // holding `o` to be saved before going into runtime. The linear
    // scan register allocator used to miscompute the number of
    // live registers at a safepoint, so the place at which the register
    // was saved was wrong.
    doCall(o instanceof Interface[], o);
  }

  public static void assertEquals(int a, int b) {}
  public static boolean doCall(boolean val, Object o) { return val; }

  static Object[] bar() { return new Object[0]; }

  static interface Interface {}
}
