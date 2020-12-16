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

public class Main extends UnresolvedSuperClass {

  /// CHECK-START: void Main.callInvokeUnresolvedStatic() register (before)
  /// CHECK:        InvokeUnresolved invoke_type:static
  static public void callInvokeUnresolvedStatic() {
    UnresolvedClass.staticMethod();
  }

  /// CHECK-START: void Main.callInvokeUnresolvedVirtual(UnresolvedClass) register (before)
  /// CHECK:        InvokeUnresolved invoke_type:virtual
  static public void callInvokeUnresolvedVirtual(UnresolvedClass c) {
    c.virtualMethod();
  }

  /// CHECK-START: void Main.callInvokeUnresolvedInterface(UnresolvedInterface) register (before)
  /// CHECK:        InvokeUnresolved invoke_type:interface
  static public void callInvokeUnresolvedInterface(UnresolvedInterface c) {
    c.interfaceMethod();
  }

  static public void callInvokeUnresolvedSuper(Main c) {
    c.superMethod();
  }

  /// CHECK-START: void Main.superMethod() register (before)
  /// CHECK:        InvokeUnresolved invoke_type:super
  public void superMethod() {
    super.superMethod();
  }

  /// CHECK-START: void Main.callUnresolvedStaticFieldAccess() register (before)
  /// CHECK:        UnresolvedStaticFieldSet field_type:Int8
  /// CHECK:        UnresolvedStaticFieldSet field_type:Uint16
  /// CHECK:        UnresolvedStaticFieldSet field_type:Int32
  /// CHECK:        UnresolvedStaticFieldSet field_type:Int64
  /// CHECK:        UnresolvedStaticFieldSet field_type:Float32
  /// CHECK:        UnresolvedStaticFieldSet field_type:Float64
  /// CHECK:        UnresolvedStaticFieldSet field_type:Reference

  /// CHECK:        UnresolvedStaticFieldGet field_type:Int8
  /// CHECK:        UnresolvedStaticFieldGet field_type:Uint16
  /// CHECK:        UnresolvedStaticFieldGet field_type:Int32
  /// CHECK:        UnresolvedStaticFieldGet field_type:Int64
  /// CHECK:        UnresolvedStaticFieldGet field_type:Float32
  /// CHECK:        UnresolvedStaticFieldGet field_type:Float64
  /// CHECK:        UnresolvedStaticFieldGet field_type:Reference
  static public void callUnresolvedStaticFieldAccess() {
    Object o = new Object();
    UnresolvedClass.staticByte = (byte)1;
    UnresolvedClass.staticChar = '1';
    UnresolvedClass.staticInt = 123456789;
    UnresolvedClass.staticLong = 123456789123456789l;
    UnresolvedClass.staticFloat = 123456789123456789f;
    UnresolvedClass.staticDouble = 123456789123456789d;
    UnresolvedClass.staticObject = o;

    expectEquals((byte)1, UnresolvedClass.staticByte);
    expectEquals('1', UnresolvedClass.staticChar);
    expectEquals(123456789, UnresolvedClass.staticInt);
    expectEquals(123456789123456789l, UnresolvedClass.staticLong);
    expectEquals(123456789123456789f, UnresolvedClass.staticFloat);
    expectEquals(123456789123456789d, UnresolvedClass.staticDouble);
    expectEquals(o, UnresolvedClass.staticObject);

    // Check "large" values.

    UnresolvedClass.staticByte = (byte)-1;
    UnresolvedClass.staticChar = (char)32768;
    UnresolvedClass.staticInt = -1;

    expectEquals((byte)-1, UnresolvedClass.staticByte);
    expectEquals((char)32768, UnresolvedClass.staticChar);
    expectEquals(-1, UnresolvedClass.staticInt);
  }

  /// CHECK-START: void Main.callUnresolvedInstanceFieldAccess(UnresolvedClass) register (before)
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Int8
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Uint16
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Int32
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Int64
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Float32
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Float64
  /// CHECK:        UnresolvedInstanceFieldSet field_type:Reference

  /// CHECK:        UnresolvedInstanceFieldGet field_type:Int8
  /// CHECK:        UnresolvedInstanceFieldGet field_type:Uint16
  /// CHECK:        UnresolvedInstanceFieldGet field_type:Int32
  /// CHECK:        UnresolvedInstanceFieldGet field_type:Int64
  /// CHECK:        UnresolvedInstanceFieldGet field_type:Float32
  /// CHECK:        UnresolvedInstanceFieldGet field_type:Float64
  /// CHECK:        UnresolvedInstanceFieldGet field_type:Reference
  static public void callUnresolvedInstanceFieldAccess(UnresolvedClass c) {
    Object o = new Object();
    c.instanceByte = (byte)1;
    c.instanceChar = '1';
    c.instanceInt = 123456789;
    c.instanceLong = 123456789123456789l;
    c.instanceFloat = 123456789123456789f;
    c.instanceDouble = 123456789123456789d;
    c.instanceObject = o;

    expectEquals((byte)1, c.instanceByte);
    expectEquals('1', c.instanceChar);
    expectEquals(123456789, c.instanceInt);
    expectEquals(123456789123456789l, c.instanceLong);
    expectEquals(123456789123456789f, c.instanceFloat);
    expectEquals(123456789123456789d, c.instanceDouble);
    expectEquals(o, c.instanceObject);

    // Check "large" values.

    c.instanceByte = (byte)-1;
    c.instanceChar = (char)32768;
    c.instanceInt = -1;

    expectEquals((byte)-1, c.instanceByte);
    expectEquals((char)32768, c.instanceChar);
    expectEquals(-1, c.instanceInt);
  }

  /// CHECK-START: void Main.callUnresolvedNull(UnresolvedClass) register (before)
  /// CHECK-NOT: NullCheck
  static public void callUnresolvedNull(UnresolvedClass c) {
    int x = 0;
    try {
      x = c.instanceInt;
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
      x -= 1;
    }
    expectEquals(-1, x);
    try {
      c.instanceInt = -1;
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
      x -= 1;
    }
    expectEquals(-2, x);
    try {
      c.virtualMethod();
      throw new Error("Expected NPE");
    } catch (NullPointerException e) {
      x -= 1;
    }
    expectEquals(-3, x);
  }

  static public void testInstanceOf(Object o) {
    if (o instanceof UnresolvedSuperClass) {
      System.out.println("instanceof ok");
    }
  }

  static public UnresolvedSuperClass testCheckCast(Object o) {
    UnresolvedSuperClass c = (UnresolvedSuperClass) o;
    System.out.println("checkcast ok");
    return c;
  }
  /// CHECK-START: void Main.main(java.lang.String[]) register (before)
  /// CHECK:        InvokeUnresolved invoke_type:direct
  static public void main(String[] args) {
    UnresolvedClass c = new UnresolvedClass();
    Main m = new Main();
    callInvokeUnresolvedStatic();
    callInvokeUnresolvedVirtual(c);
    callInvokeUnresolvedInterface(c);
    callInvokeUnresolvedSuper(m);
    callUnresolvedStaticFieldAccess();
    callUnresolvedInstanceFieldAccess(c);
    callUnresolvedNull(null);
    testInstanceOf(m);
    testCheckCast(m);
    testLicm(2);
  }

  /// CHECK-START: void Main.testLicm(int) licm (before)
  /// CHECK:      <<Class:l\d+>>        LoadClass                                     loop:<<LoopLabel:B\d+>>
  /// CHECK-NEXT: <<Clinit:l\d+>>       ClinitCheck [<<Class>>]                       loop:<<LoopLabel>>
  /// CHECK-NEXT: <<New:l\d+>>          NewInstance [<<Clinit>>]                      loop:<<LoopLabel>>
  /// CHECK-NEXT:                       ConstructorFence [<<New>>]                    loop:<<LoopLabel>>
  /// CHECK-NEXT:                       InvokeUnresolved [<<New>>]                    loop:<<LoopLabel>>

  /// CHECK-START: void Main.testLicm(int) licm (after)
  /// CHECK:      <<Class:l\d+>>        LoadClass                                     loop:none
  /// CHECK-NEXT: <<Clinit:l\d+>>       ClinitCheck [<<Class>>]                       loop:none
  /// CHECK:      <<New:l\d+>>          NewInstance [<<Clinit>>]                      loop:<<LoopLabel:B\d+>>
  /// CHECK-NEXT:                       ConstructorFence [<<New>>]                    loop:<<LoopLabel>>
  /// CHECK-NEXT:                       InvokeUnresolved [<<New>>]                    loop:<<LoopLabel>>
  static public void testLicm(int count) {
    // Test to make sure we keep the initialization check after loading an unresolved class.
    UnresolvedClass c;
    int i = 0;
    do {
      c = new UnresolvedClass();
    } while (i++ != count);
  }

  public static void expectEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(float expected, float result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void expectEquals(Object expected, Object result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
