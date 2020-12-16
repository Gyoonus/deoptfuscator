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

public class Main {
  public static void main(String[] args) {
    System.out.println($noinline$FloatConvert(false));
    System.out.println($noinline$DoubleConvert(false));
  }

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

  public static long $noinline$FloatConvert(boolean flag) {
    // Try defeating inlining.
    if (doThrow) {
      throw new Error();
    }
    long l = myLong;
    myFloat = (float)l;
    return l;
  }

  public static long $noinline$DoubleConvert(boolean flag) {
    // Try defeating inlining.
    if (doThrow) {
      throw new Error();
    }
    long l = myLong;
    myDouble = (double)l;
    return l;
  }

  public static long myLong = 42;
  public static float myFloat = 2.0f;
  public static double myDouble = 4.0d;
}
