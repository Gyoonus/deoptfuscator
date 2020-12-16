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

public class Main {

  public static void main(String[] args) {
    System.out.println("passed");
  }

  /// CHECK-START-ARM64: void Main.test() disassembly (after)
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK-NOT:    blr
  /// CHECK: ReturnVoid

  static void test() {
    Object[] src = new Object[1024];
    Object[] dst = new Object[2048];
    // The length of the copied data must not be too large (smaller
    // than kSystemArrayCopyThreshold = 128) for the call to
    // System.arraycopy to be intrinsified.
    System.arraycopy(src, 0, dst, 1024, 64);
  }

}
