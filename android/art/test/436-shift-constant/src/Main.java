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
    assertEquals(0x80000000, doShiftInt(1));
    assertEquals(0x8000000000000000L, doShiftLong(1L));
  }

  public static int doShiftInt(int value) {
    return value << 0xFFFF;
  }

  public static long doShiftLong(long value) {
    return value << 0xFFFF;
  }

  public static void assertEquals(int a, int b) {
    if (a != b) {
      throw new Error("Expected " + a + ", got " + b);
    }
  }

  public static void assertEquals(long a, long b) {
    if (a != b) {
      throw new Error("Expected " + a + ", got " + b);
    }
  }
}
