/*
 * Copyright (C) 2015 The Android Open Source Project
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

import sun.misc.Unsafe;
import java.lang.reflect.Field;

public class Main {

  long instanceField;
  static long myLongField1;
  static long myLongField2;

  public static void main(String[] args) throws Exception {
    Unsafe unsafe = getUnsafe();
    Main f = new Main();
    long offset = unsafe.objectFieldOffset(Main.class.getDeclaredField("instanceField"));
    getUnsafe(); // spill offset
    long a = myLongField1;
    // We used the hinted register for the low part of b, which is EBX, as requested
    // by the intrinsic below. Allocating EBX for the low part, would put ESP as the high
    // part, and we did not check that ESP was blocked.
    long b = myLongField2;
    unsafe.compareAndSwapLong(f, offset, a, b);
  }


  private static Unsafe getUnsafe() throws Exception {
    Field f = Unsafe.class.getDeclaredField("theUnsafe");
    f.setAccessible(true);
    return (Unsafe) f.get(null);
  }
}
