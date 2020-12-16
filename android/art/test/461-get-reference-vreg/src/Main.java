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

  int testThisWithInstanceCall() {
    return doNativeCallRef();
  }

  int testThisWithStaticCall() {
    return doStaticNativeCallRef();
  }

  static int testParameter(Object a) {
    return doStaticNativeCallRef();
  }

  static int testObjectInScope() {
    Object a = array[0];
    return doStaticNativeCallRef();
  }

  native int doNativeCallRef();
  static native int doStaticNativeCallRef();

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    Main rm = new Main();
    if (rm.testThisWithInstanceCall() != 1) {
      throw new Error("Expected 1");
    }

    if (rm.testThisWithStaticCall() != 2) {
      throw new Error("Expected 2");
    }

    if (testParameter(new Object()) != 3) {
      throw new Error("Expected 3");
    }

    if (testObjectInScope() != 4) {
      throw new Error("Expected 4");
    }
  }

  static Object[] array = new Object[] { new Object() };
}
