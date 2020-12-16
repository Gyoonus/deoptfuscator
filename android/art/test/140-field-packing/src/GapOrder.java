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

// Regression test for 22460222, the sub class.
// The field gaps order was wrong. If there were two gaps of different sizes,
// and the larger one was needed, it wouldn't be found.

import java.lang.reflect.Field;
import java.lang.reflect.Method;

class GapOrder extends GapOrderBase {
  // The base class is 9 bytes. The entire class should be packed as:
  //
  //    00: oooo oooo
  //    08: b-ss rrrr
  //    16: rrrr iiii
  //    24: dddd dddd
  //
  // The problem was, the packer wasn't finding the gap where iiii should go,
  // because the gap where ss goes was given priority. Instead it packed as:
  //    00: oooo oooo
  //    08: b--- rrrr
  //    16: rrrr ----
  //    24: dddd dddd
  //    32: iiii ss
  public Object r1;
  public Object r2;
  public double d;
  public int i;
  public short s;

  static private void CheckField(String fieldName, int expected) {
    Field field = null;
    try {
      field = GapOrder.class.getField(fieldName);
    } catch (ReflectiveOperationException e) {
      System.out.println(fieldName + " not found in GapOrder.");
      return;
    }

    int actual = -1;
    try {
      Method getOffset = Field.class.getMethod("getOffset");
      actual = (Integer)getOffset.invoke(field);
    } catch (ReflectiveOperationException e) {
      System.out.println("Unable to get field offset for " + fieldName + ":" + e);
      return;
    }

    if (actual != expected) {
      System.out.println(
          String.format("GapOrder.%s has offset %d, but expected %d",
            fieldName, actual, expected));
    }
  }

  static public void Check() {
    CheckField("r1", 12);
    CheckField("r2", 16);
    CheckField("d", 24);
    CheckField("i", 20);
    CheckField("s", 10);
  }
}

