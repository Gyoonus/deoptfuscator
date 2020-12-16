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

  static Object exactCheck = new ExactCheck();
  static Object abstractCheckImpl = new AbstractCheckImpl();
  static Object interfaceCheckImpl = new InterfaceCheckImpl();
  static Object normalCheck = new NormalCheck();
  static Object regularObject = new Object();
  static Object objectArray = new Object[2];
  static Object intArray = new int[2];
  static Object doubleArray = new double[2];
  static Object exactArray = new ExactCheck[2];
  static Object normalArray = new NormalCheck[2];

  static Object field;

  public static void main(String[] args) {
    checkInstanceOfNonTryCatch();
    // We also check for a method with try/catch because the compiler then makes a slow
    // path unconditionally save its live registers.
    checkInstanceOfTryCatch();

    checkCheckCast();
  }

  public static void checkInstanceOfNonTryCatch() {
    check(true, exactCheck instanceof ExactCheck);
    check(false, regularObject instanceof ExactCheck);

    check(true, abstractCheckImpl instanceof AbstractCheck);
    check(false, regularObject instanceof AbstractCheck);

    check(true,  interfaceCheckImpl instanceof InterfaceCheck);
    check(false, regularObject instanceof InterfaceCheck);

    check(true, normalCheck instanceof NormalCheck);
    check(true, exactCheck instanceof NormalCheck);
    check(false, regularObject instanceof NormalCheck);

    check(false, regularObject instanceof int[]);
    check(false, objectArray instanceof int[]);
    check(true, intArray instanceof int[]);
    check(false, doubleArray instanceof int[]);

    check(false, regularObject instanceof ExactCheck[]);
    check(false, objectArray instanceof ExactCheck[]);
    check(false, doubleArray instanceof ExactCheck[]);
    check(true, exactArray instanceof ExactCheck[]);
    check(false, normalArray instanceof ExactCheck[]);

    check(false, regularObject instanceof NormalCheck[]);
    check(false, objectArray instanceof NormalCheck[]);
    check(false, doubleArray instanceof NormalCheck[]);
    check(true, exactArray instanceof NormalCheck[]);
    check(true, normalArray instanceof NormalCheck[]);

    check(false, regularObject instanceof Object[]);
    check(true, objectArray instanceof Object[]);
    check(false, doubleArray instanceof Object[]);
    check(true, exactArray instanceof Object[]);
    check(true, normalArray instanceof Object[]);
  }

  public static void checkInstanceOfTryCatch() {
    try {
      check(true, exactCheck instanceof ExactCheck);
      check(false, regularObject instanceof ExactCheck);

      check(true, abstractCheckImpl instanceof AbstractCheck);
      check(false, regularObject instanceof AbstractCheck);

      check(true,  interfaceCheckImpl instanceof InterfaceCheck);
      check(false, regularObject instanceof InterfaceCheck);

      check(true, normalCheck instanceof NormalCheck);
      check(true, exactCheck instanceof NormalCheck);
      check(false, regularObject instanceof NormalCheck);

      check(false, regularObject instanceof int[]);
      check(false, objectArray instanceof int[]);
      check(true, intArray instanceof int[]);
      check(false, doubleArray instanceof int[]);

      check(false, regularObject instanceof ExactCheck[]);
      check(false, objectArray instanceof ExactCheck[]);
      check(false, doubleArray instanceof ExactCheck[]);
      check(true, exactArray instanceof ExactCheck[]);
      check(false, normalArray instanceof ExactCheck[]);

      check(false, regularObject instanceof NormalCheck[]);
      check(false, objectArray instanceof NormalCheck[]);
      check(false, doubleArray instanceof NormalCheck[]);
      check(true, exactArray instanceof NormalCheck[]);
      check(true, normalArray instanceof NormalCheck[]);

      check(false, regularObject instanceof Object[]);
      check(true, objectArray instanceof Object[]);
      check(false, doubleArray instanceof Object[]);
      check(true, exactArray instanceof Object[]);
      check(true, normalArray instanceof Object[]);
    } catch (Throwable t) {
      throw new Error("Unreachable");
    }
  }

  public static void check(boolean expected, boolean actual) {
    if (actual != expected) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void checkCheckCast() {
    // Exact check.
    field = (ExactCheck)exactCheck;
    try {
      field = (ExactCheck)regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    // Abstract check.
    field = (AbstractCheck)abstractCheckImpl;
    try {
      field = (AbstractCheck)regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    // Interface check.
    field = (InterfaceCheck)interfaceCheckImpl;
    try {
      field = (InterfaceCheck)regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    // Normal check.
    field = (NormalCheck)normalCheck;
    field = (NormalCheck)exactCheck;
    try {
      field = (NormalCheck)regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    // Primitive array check.
    try {
      field = (int[])regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    try {
      field = (int[])objectArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    field = (int[])intArray;
    try {
      field = (int[])doubleArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    // Array with final component type check.
    try {
      field = (ExactCheck[])regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    try {
      field = (ExactCheck[])objectArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    try {
      field = (ExactCheck[])doubleArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    field = (ExactCheck[])exactArray;
    try {
      field = (ExactCheck[])normalArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    // Array with non final component type check.
    try {
      field = (NormalCheck[])regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    try {
      field = (NormalCheck[])objectArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    try {
      field = (NormalCheck[])doubleArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    field = (NormalCheck[])exactArray;
    field = (NormalCheck[])normalArray;

    // Object[] check.
    try{
      field = (Object[])regularObject;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    field = (Object[])objectArray;
    try {
      field = (Object[])doubleArray;
      throw new Error("Can't reach here");
    } catch (ClassCastException ignore) {}

    field = (Object[])exactArray;
    field = (Object[])normalArray;
  }
}

class NormalCheck {
}

final class ExactCheck extends NormalCheck {
}

abstract class AbstractCheck {
}

class AbstractCheckImpl extends AbstractCheck {
}

interface InterfaceCheck {
}

class InterfaceCheckImpl implements InterfaceCheck {
}
