/*
 * Copyright (C) 2014 The Android Open Source Project
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
  public final static int INTERFACE_DEFINED_BITS =
      0x0001 |  // public, may be set.
      0x0002 |  // private, may be flagged by inner class.
      0x0004 |  // protected, may be flagged by inner class.
      0x0008 |  // static, may be flagged by inner class.
      0x0010 |  // final, must not be set.
      0x0020 |  // super, must not be set.
      0x0200 |  // interface, must be set.
      0x0400 |  // abstract, must be set.
      0x1000 |  // synthetic, may be set.
      0x2000 |  // annotation, may be set (annotation implies interface)
      0x4000 ;  // enum, must not be set.

  public final static int CLASS_DEFINED_BITS =
      0x0001 |  // public, may be set.
      0x0002 |  // private, may be flagged by inner class.
      0x0004 |  // protected, may be flagged by inner class.
      0x0008 |  // static, may be flagged by inner class.
      0x0010 |  // final, may be set.
      0x0020 |  // super, may be set.
      0x0200 |  // interface, must not be set.
      0x0400 |  // abstract, may be set.
      0x1000 |  // synthetic, may be set.
      0x2000 |  // annotation, must not be set.
      0x4000 ;  // enum, may be set.

  public final static int FIELD_DEFINED_BITS =
       0x0001 |  // public
       0x0002 |  // private
       0x0004 |  // protected
       0x0008 |  // static
       0x0010 |  // final
       0x0040 |  // volatile
       0x0080 |  // transient
       0x1000 |  // synthetic
       0x4000 ;  // enum

  public final static int METHOD_DEFINED_BITS =
       0x0001 |  // public
       0x0002 |  // private
       0x0004 |  // protected
       0x0008 |  // static
       0x0010 |  // final
       0x0020 |  // synchronized
       0x0040 |  // bridge
       0x0080 |  // varargs
       0x0100 |  // native
       0x0400 |  // abstract
       0x0800 |  // strictfp
       0x1000 ;  // synthetic

  public static void main(String args[]) throws Exception {
    check("Inf");
    check("NonInf");
    check("A");
    check("A$B");
  }

  private static void check(String className) throws Exception {
    Class<?> clazz = Class.forName(className);
    if (className.equals("Inf")) {
      if (!clazz.isInterface()) {
        throw new RuntimeException("Expected an interface.");
      }
      int undefinedBits = 0xFFFF ^ INTERFACE_DEFINED_BITS;
      if ((clazz.getModifiers() & undefinedBits) != 0) {
        System.out.println("Clazz.getModifiers(): " + Integer.toBinaryString(clazz.getModifiers()));
        System.out.println("INTERFACE_DEF_BITS: " + Integer.toBinaryString(INTERFACE_DEFINED_BITS));
        throw new RuntimeException("Undefined bits for an interface: " + className);
      }
    } else {
      if (clazz.isInterface()) {
        throw new RuntimeException("Expected a class.");
      }
      int undefinedBits = 0xFFFF ^ CLASS_DEFINED_BITS;
      if ((clazz.getModifiers() & undefinedBits) != 0) {
        System.out.println("Clazz.getModifiers(): " + Integer.toBinaryString(clazz.getModifiers()));
        System.out.println("CLASS_DEF_BITS: " + Integer.toBinaryString(CLASS_DEFINED_BITS));
        throw new RuntimeException("Undefined bits for a class: " + className);
      }
    }

    // Check fields.
    for (java.lang.reflect.Field f : clazz.getDeclaredFields()) {
      String name = f.getName();
      int undefinedBits = 0xFFFF ^ FIELD_DEFINED_BITS;
      if ((f.getModifiers() & undefinedBits) != 0) {
        System.out.println("f.getModifiers(): " + Integer.toBinaryString(f.getModifiers()));
        System.out.println("FIELD_DEF_BITS: " + Integer.toBinaryString(FIELD_DEFINED_BITS));
        throw new RuntimeException("Unexpected field bits: " + name);
      }
      if (name.equals("I")) {
        // Interface field, just check generically.
      } else {
        // Check the name, see that the corresponding bit is set.
        int bitmask = getFieldMask(name);
        if ((bitmask & f.getModifiers()) == 0) {
          throw new RuntimeException("Expected field bit not set.");
        }
      }
    }

    // Check methods.
    for (java.lang.reflect.Method m : clazz.getDeclaredMethods()) {
      String name = m.getName();
      int undefinedBits = 0xFFFF ^ METHOD_DEFINED_BITS;
      if ((m.getModifiers() & undefinedBits) != 0) {
          System.out.println("m.getModifiers(): " + Integer.toBinaryString(m.getModifiers()));
          System.out.println("METHOD_DEF_BITS: " + Integer.toBinaryString(METHOD_DEFINED_BITS));
        throw new RuntimeException("Unexpected method bits: " + name);
      }
      // Check the name, see that the corresponding bit is set.
      int bitmask = getMethodMask(name);
      if ((bitmask & m.getModifiers()) == 0) {
        throw new RuntimeException("Expected method bit not set.");
      }
    }
  }

  private static int getFieldMask(String name) {
    int index = name.indexOf("Field");
    if (index > 0) {
      String shortS = name.substring(0, index);
      if (shortS.equals("public")) {
        return 0x0001;
      }
      if (shortS.equals("private")) {
        return 0x0002;
      }
      if (shortS.equals("protected")) {
        return 0x0004;
      }
      if (shortS.equals("static")) {
        return 0x0008;
      }
      if (shortS.equals("transient")) {
        return 0x0080;
      }
      if (shortS.equals("volatile")) {
        return 0x0040;
      }
      if (shortS.equals("final")) {
        return 0x0010;
      }
    }
    throw new RuntimeException("Unexpected field name " + name);
  }

  private static int getMethodMask(String name) {
    int index = name.indexOf("Method");
    if (index > 0) {
      String shortS = name.substring(0, index);
      if (shortS.equals("public")) {
        return 0x0001;
      }
      if (shortS.equals("private")) {
        return 0x0002;
      }
      if (shortS.equals("protected")) {
        return 0x0004;
      }
      if (shortS.equals("static")) {
        return 0x0008;
      }
      if (shortS.equals("synchronized")) {
        return 0x0020;
      }
      if (shortS.equals("varargs")) {
        return 0x0080;
      }
      if (shortS.equals("final")) {
        return 0x0010;
      }
      if (shortS.equals("native")) {
        return 0x0100;
      }
      if (shortS.equals("abstract")) {
        return 0x0400;
      }
      if (shortS.equals("strictfp")) {
        return 0x0800;
      }
    }
    throw new RuntimeException("Unexpected method name " + name);
  }
}
