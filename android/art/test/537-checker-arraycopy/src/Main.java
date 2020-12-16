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
    arraycopy();
    try {
      arraycopy(new Object());
      throw new Error("Should not be here");
    } catch (ArrayStoreException ase) {
      // Ignore.
    }
    try {
      arraycopy(null);
      throw new Error("Should not be here");
    } catch (NullPointerException npe) {
      // Ignore.
    }

    try {
      arraycopy(new Object[1]);
      throw new Error("Should not be here");
    } catch (ArrayIndexOutOfBoundsException aiooe) {
      // Ignore.
    }

    arraycopy(new Object[2]);
    arraycopy(new Object[2], 0);

    try {
      arraycopy(new Object[1], 1);
      throw new Error("Should not be here");
    } catch (ArrayIndexOutOfBoundsException aiooe) {
      // Ignore.
    }
  }

  /// CHECK-START-X86_64: void Main.arraycopy() disassembly (after)
  /// CHECK:          InvokeStaticOrDirect intrinsic:SystemArrayCopy
  /// CHECK-NOT:      test {{^[^\[].*}}, {{^[^\[].*}}
  /// CHECK-NOT:      call
  /// CHECK:          ReturnVoid
  // Checks that the call is intrinsified and that there is no register test instruction
  // when we know the source and destination are not null.
  public static void arraycopy() {
    Object[] obj = new Object[4];
    System.arraycopy(obj, 1, obj, 0, 1);
  }

  public static void arraycopy(Object obj) {
    System.arraycopy(obj, 1, obj, 0, 1);
  }

  // Test case for having enough registers on x86 for the arraycopy intrinsic.
  /// CHECK-START-X86: void Main.arraycopy(java.lang.Object[], int) disassembly (after)
  /// CHECK:          InvokeStaticOrDirect intrinsic:SystemArrayCopy
  /// CHECK-NOT:      mov {{[a-z]+}}, [esp + {{[0-9]+}}]
  /// CHECK:          ReturnVoid
  public static void arraycopy(Object[] obj, int pos) {
    System.arraycopy(obj, pos, obj, 0, obj.length);
  }

  // Test case for having enough registers on x86 for the arraycopy intrinsic
  // when an input is passed twice.
  /// CHECK-START-X86: int Main.arraycopy2(java.lang.Object[], int) disassembly (after)
  /// CHECK:          InvokeStaticOrDirect intrinsic:SystemArrayCopy
  /// CHECK-NOT:      mov {{[a-z]+}}, [esp + {{[0-9]+}}]
  /// CHECK:          Return
  public static int arraycopy2(Object[] obj, int pos) {
    System.arraycopy(obj, pos, obj, pos - 1, obj.length);
    return pos;
  }

  // Test case for not having enough registers on x86. The arraycopy intrinsic
  // will ask for length to be in stack and load it.
  /// CHECK-START-X86: int Main.arraycopy3(java.lang.Object[], java.lang.Object[], int, int, int) disassembly (after)
  /// CHECK:          InvokeStaticOrDirect intrinsic:SystemArrayCopy
  /// CHECK:          mov {{[a-z]+}}, [esp + {{[0-9]+}}]
  /// CHECK:          Return
  public static int arraycopy3(Object[] obj1, Object[] obj2, int input1, int input3, int input4) {
    System.arraycopy(obj1, input1, obj2, input3, input4);
    System.out.println(obj1);
    System.out.println(obj2);
    return input1 + input3 + input4;
  }
}
