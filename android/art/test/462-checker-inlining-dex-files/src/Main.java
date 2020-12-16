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

// Add a class that will be the first entry in the dex cache, to
// avoid having the OtherDex and Main classes share the same cache index.
class AAA {
}

public class Main {

  /// CHECK-START: void Main.inlineEmptyMethod() inliner (before)
  /// CHECK-DAG:     <<Invoke:v\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      ReturnVoid

  /// CHECK-START: void Main.inlineEmptyMethod() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  public static void inlineEmptyMethod() {
    OtherDex.emptyMethod();
  }

  /// CHECK-START: int Main.inlineReturnIntMethod() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineReturnIntMethod() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineReturnIntMethod() inliner (after)
  /// CHECK-DAG:     <<Const38:i\d+>> IntConstant 38
  /// CHECK-DAG:                      Return [<<Const38>>]

  public static int inlineReturnIntMethod() {
    return OtherDex.returnIntMethod();
  }

  /// CHECK-START: int Main.dontInlineOtherDexStatic() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.dontInlineOtherDexStatic() inliner (after)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  public static int dontInlineOtherDexStatic() {
    return OtherDex.returnOtherDexStatic();
  }

  /// CHECK-START: int Main.inlineMainStatic() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.inlineMainStatic() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: int Main.inlineMainStatic() inliner (after)
  /// CHECK-DAG:     <<Static:i\d+>>  StaticFieldGet
  /// CHECK-DAG:                      Return [<<Static>>]

  public static int inlineMainStatic() {
    return OtherDex.returnMainStatic();
  }

  /// CHECK-START: int Main.dontInlineRecursiveCall() inliner (before)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: int Main.dontInlineRecursiveCall() inliner (after)
  /// CHECK-DAG:     <<Invoke:i\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  public static int dontInlineRecursiveCall() {
    return OtherDex.recursiveCall();
  }

  /// CHECK-START: java.lang.String Main.dontInlineReturnString() inliner (before)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: java.lang.String Main.dontInlineReturnString() inliner (after)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  public static String dontInlineReturnString() {
    return OtherDex.returnString();
  }

  /// CHECK-START: java.lang.Class Main.dontInlineOtherDexClass() inliner (before)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: java.lang.Class Main.dontInlineOtherDexClass() inliner (after)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  public static Class<?> dontInlineOtherDexClass() {
    return OtherDex.returnOtherDexClass();
  }

  /// CHECK-START: java.lang.Class Main.inlineMainClass() inliner (before)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: java.lang.Class Main.inlineMainClass() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: java.lang.Class Main.inlineMainClass() inliner (after)
  /// CHECK-DAG:                     Return [<<Class:l\d+>>]
  /// CHECK-DAG:     <<Class>>       LoadClass
  // Note: There are two LoadClass instructions. We obtain the correct
  //       instruction id by matching the Return's input list first.

  public static Class<?> inlineMainClass() {
    return OtherDex.returnMainClass();
  }

  /// CHECK-START: java.lang.Class Main.dontInlineOtherDexClassStaticCall() inliner (before)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: java.lang.Class Main.dontInlineOtherDexClassStaticCall() inliner (after)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  public static Class<?> dontInlineOtherDexClassStaticCall() {
    return OtherDex.returnOtherDexClassStaticCall();
  }

  /// CHECK-START: java.lang.Class Main.inlineOtherDexCallingMain() inliner (before)
  /// CHECK-DAG:     <<Invoke:l\d+>>  InvokeStaticOrDirect
  /// CHECK-DAG:                      Return [<<Invoke>>]

  /// CHECK-START: java.lang.Class Main.inlineOtherDexCallingMain() inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect

  /// CHECK-START: java.lang.Class Main.inlineOtherDexCallingMain() inliner (after)
  /// CHECK-DAG:                     Return [<<Class:l\d+>>]
  /// CHECK-DAG:     <<Class>>       LoadClass
  // Note: There are two LoadClass instructions. We obtain the correct
  //       instruction id by matching the Return's input list first.

  public static Class<?> inlineOtherDexCallingMain() {
    return OtherDex.returnOtherDexCallingMain();
  }

  public static Class<?> getOtherClass() {
    return Main.class;
  }

  public static void main(String[] args) {
    inlineEmptyMethod();
    if (inlineReturnIntMethod() != 38) {
      throw new Error("Expected 38");
    }

    if (dontInlineOtherDexStatic() != 1) {
      throw new Error("Expected 1");
    }

    if (inlineMainStatic() != 42) {
      throw new Error("Expected 42");
    }

    if (dontInlineReturnString() != "OtherDex") {
      throw new Error("Expected OtherDex");
    }

    if (dontInlineOtherDexClass() != OtherDex.class) {
      throw new Error("Expected " + OtherDex.class);
    }

    if (dontInlineOtherDexClassStaticCall() != OtherDex.class) {
      throw new Error("Expected " + OtherDex.class);
    }

    if (inlineMainClass() != Main.class) {
      throw new Error("Expected " + Main.class);
    }

    if (inlineOtherDexCallingMain() != Main.class) {
      throw new Error("Expected " + Main.class);
    }
  }

  // Reference the AAA class to ensure it is in the dex cache.
  public static Class<?> cls = AAA.class;

  // Add a field that will be the first entry in the dex cache, to
  // avoid having the OtherDex.myStatic and Main.myStatic fields
  // share the same cache index.
  public static int aaa = 32;
  public static int myStatic = 42;
}
