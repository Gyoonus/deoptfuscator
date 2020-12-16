/*
 * Copyright (C) 2016 The Android Open Source Project
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

  /// CHECK-START: void Main.testNewStringFromBytes() builder (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromBytes intrinsic:None

  /// CHECK-START: void Main.testNewStringFromBytes() intrinsics_recognition (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromBytes intrinsic:StringNewStringFromBytes

  public static void testNewStringFromBytes() {
    byte[] bytes = { 'f', 'o', 'o' };
    String s = StringFactory.newStringFromBytes(bytes, 0, 0, 3);
    System.out.println(s);
  }

  // The (native) method
  //
  //   java.lang.StringFactory.newStringFromChars(int offset, int charCount, char[] data)
  //
  // is recognized as intrinsic StringNewStringFromChars.  However,
  // because this method is not public, we cannot call it and check
  // that the compiler actually intrinsifies it (as it does for the
  // StringNewStringFromBytes and StringNewStringFromString
  // intrinsics) with Checker.
  //
  // We can call a public method such as
  //
  //   java.lang.StringFactory.newStringFromChars(char[] data)
  //
  // which contains a call to the former (non-public) native method.
  // However, this call will not be inlined (because it is a method in
  // another Dex file and which contains a call, which needs an
  // environment), so we cannot use Checker here to ensure the native
  // call was intrinsified either.

  /// CHECK-START: void Main.testNewStringFromChars() builder (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromChars intrinsic:None

  /// CHECK-START: void Main.testNewStringFromChars() intrinsics_recognition (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromChars intrinsic:None

  /// CHECK-START: void Main.testNewStringFromChars() inliner (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromChars intrinsic:None

  public static void testNewStringFromChars() {
    char[] chars = { 'b', 'a', 'r' };
    String s = StringFactory.newStringFromChars(chars);
    System.out.println(s);
  }

  /// CHECK-START: void Main.testNewStringFromString() builder (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromString intrinsic:None

  /// CHECK-START: void Main.testNewStringFromString() intrinsics_recognition (after)
  /// CHECK-DAG:     InvokeStaticOrDirect method_name:java.lang.StringFactory.newStringFromString intrinsic:StringNewStringFromString

  public static void testNewStringFromString() {
    String s1 = "baz";
    String s2 = StringFactory.newStringFromString(s1);
    System.out.println(s2);
  }

  public static void main(String[] args) throws Exception {
    testNewStringFromBytes();
    testNewStringFromChars();
    testNewStringFromString();
  }
}
