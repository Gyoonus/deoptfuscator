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

// To make it easier to follow the tests:
//  - all interfaces defined in this file extend InterfaceSuper (except InterfaceOtherSuper)
//  - all classes defined in this file extend ClassSuper (except ClassOtherSuper)

interface InterfaceSuper {}
interface InterfaceOtherSuper {}

interface InterfaceA extends InterfaceSuper {}
interface InterfaceB extends InterfaceSuper {}
interface InterfaceExtendsA extends InterfaceA {}
interface InterfaceExtendsB extends InterfaceB {}

class ClassSuper {}
class ClassOtherSuper {}

class ClassA extends ClassSuper {}
class ClassB extends ClassSuper {}
class ClassExtendsA extends ClassA {}
class ClassExtendsB extends ClassB {}

class ClassImplementsInterfaceA extends ClassSuper implements InterfaceA {}

public class Main {

  /// CHECK-START: java.lang.Object Main.testMergeNullContant(boolean) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:Main
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeNullContant(boolean cond) {
    return cond ? null : new Main();
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassExtendsA, ClassExtendsB) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:ClassSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassExtendsA a, ClassExtendsB b) {
    // Different classes, have a common super type.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassExtendsA, ClassSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:ClassSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassExtendsA a, ClassSuper b) {
    // Different classes, one is the super type of the other.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassSuper, ClassSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:ClassSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassSuper a, ClassSuper b) {
    // Same classes.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassOtherSuper, ClassSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassOtherSuper a, ClassSuper b) {
    // Different classes, have Object as the common super type.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClassWithInterface(boolean, ClassImplementsInterfaceA, InterfaceSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClassWithInterface(boolean cond, ClassImplementsInterfaceA a, InterfaceSuper b) {
    // Class implements interface.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClassWithInterface(boolean, ClassSuper, InterfaceSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClassWithInterface(boolean cond, ClassSuper a, InterfaceSuper b) {
    // Class doesn't implement interface.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceExtendsA, InterfaceSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceExtendsA a, InterfaceSuper b) {
    // Different Interfaces, one implements the other.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceSuper, InterfaceSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceSuper a, InterfaceSuper b) {
    // Same interfaces.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceExtendsA, InterfaceExtendsB) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceExtendsA a, InterfaceExtendsB b) {
    // Different Interfaces, have a common super type.
    return cond ? a : b;
  }

    /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceSuper, InterfaceOtherSuper) builder (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceSuper a, InterfaceOtherSuper b) {
    // Different interfaces.
    return cond ? a : b;
  }

  public static void main(String[] args) {
  }
}
