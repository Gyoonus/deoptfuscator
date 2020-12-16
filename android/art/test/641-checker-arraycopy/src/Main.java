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

  // Note that this is testing we haven't intrinsified the byte[] arraycopy version.
  // Once we eventually start doing it, we will need to re-adjust this test.

  /// CHECK-START-X86: void Main.typedCopy(java.lang.Object, byte[]) disassembly (after)
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK-NOT:    call
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK:        call
  /// CHECK: ReturnVoid
  public static void typedCopy(Object o, byte[] foo) {
    System.arraycopy(o, 1, o, 0, 1);
    System.arraycopy((Object)foo, 1, (Object)foo, 0, 1);  // Don't use the @hide byte[] overload.
  }

  public static void untypedCopy(Object o, Object foo) {
    System.arraycopy(o, 1, o, 0, 1);
    System.arraycopy(foo, 1, foo, 0, 1);
  }

  // Test that we still do the optimization after inlining.

  /// CHECK-START-X86: void Main.untypedCopyCaller(java.lang.Object, byte[]) disassembly (after)
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK-NOT:    call
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK:        call
  /// CHECK: ReturnVoid
  public static void untypedCopyCaller(Object o, byte[] array) {
    untypedCopy(o, array);
  }

  public static void assertEquals(Object one, Object two) {
    if (one != two) {
      throw new Error("Expected " + one + ", got " + two);
    }
  }

  public static void main(String[] args) {
    // Simple sanity checks.
    byte[] a = new byte[2];
    Object[] o = new Object[2];

    o[0] = a;
    o[1] = o;
    a[0] = 1;
    a[1] = 2;

    untypedCopyCaller(o, a);
    assertEquals(o[0], o);
    assertEquals(o[1], o);
    assertEquals(a[0], (byte)2);
    assertEquals(a[1], (byte)2);

    o[0] = a;
    o[1] = o;
    a[0] = 1;
    a[1] = 2;

    typedCopy(o, a);
    assertEquals(o[0], o);
    assertEquals(o[1], o);
    assertEquals(a[0], (byte)2);
    assertEquals(a[1], (byte)2);
  }
}
