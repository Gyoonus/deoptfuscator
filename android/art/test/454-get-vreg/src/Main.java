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
  public Main() {
  }

  int testSimpleVReg(int a, float f, short s, boolean z, byte b, char c) {
    int e = doCall();
    int g = doNativeCall();
    return e + g;
  }

  long testPairVReg(long a, long b, long c, double e) {
    long f = doCall();
    long g = doNativeCall();
    return f + g;
  }

  native int doNativeCall();

  int doCall() {
    return 42;
  }

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    Main rm = new Main();
    if (rm.testSimpleVReg(1, 1.0f, (short)2, true, (byte)3, 'c') != 43) {
      throw new Error("Expected 43");
    }

    if (rm.testPairVReg(Long.MIN_VALUE, Long.MAX_VALUE, 0, 2.0) != 44) {
      throw new Error("Expected 44");
    }
  }
}
