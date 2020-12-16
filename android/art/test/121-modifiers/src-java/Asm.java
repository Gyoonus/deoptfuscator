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

import org.objectweb.asm.*;
import org.objectweb.asm.tree.*;
import java.io.*;
import java.util.*;

public class Asm {
  /*

  Overall class access flags:

      0x0001 |  // public
      0x0010 |  // final
      0x0020 |  // super
      0x0200 |  // interface
      0x0400 |  // abstract
      0x1000 |  // synthetic
      0x2000 |  // annotation
      0x4000 ;  // enum

  */

  public final static int INTERFACE_DEFINED_BITS =
      0x0001 |  // public, may be set.
      0x0010 |  // final, must not be set.
      0x0020 |  // super, must not be set.
      0x0200 |  // interface, must be set.
      0x0400 |  // abstract, must be set.
      0x1000 |  // synthetic, may be set.
      0x2000 |  // annotation, may be set (annotation implies interface)
      0x4000 ;  // enum, must not be set.

  public final static int CLASS_DEFINED_BITS =
      0x0001 |  // public, may be set.
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
    modify("Inf");
    modify("NonInf");
  }

  private static void modify(String clazz) throws Exception {
    ClassNode classNode = new ClassNode();
    ClassReader cr = new ClassReader(clazz);
    cr.accept(classNode, 0);

    modify(classNode);

    ClassWriter cw = new ClassWriter(0);
    classNode.accept(cw);
    byte[] b = cw.toByteArray();
    OutputStream out = new FileOutputStream(clazz + ".out");
    out.write(b, 0, b.length);
    out.close();
  }

  private static void modify(ClassNode classNode) throws Exception {
    int classFlagsOr = 0xFFFF;
    // Check whether classNode is an interface or class.
    if ((classNode.access & Opcodes.ACC_INTERFACE) == 0) {
      classFlagsOr ^= CLASS_DEFINED_BITS;
    } else {
      classFlagsOr ^= INTERFACE_DEFINED_BITS;
    }
    classNode.access |= classFlagsOr;

    // Fields.
    int fieldFlagsOr = 0xFFFF ^ FIELD_DEFINED_BITS;
    for (FieldNode fieldNode : (List<FieldNode>)classNode.fields) {
      fieldNode.access |= fieldFlagsOr;
    }

    // Methods.
    int methodFlagsOr = 0xFFFF ^ METHOD_DEFINED_BITS;
    for (MethodNode methodNode :(List<MethodNode>) classNode.methods) {
      methodNode.access |= methodFlagsOr;
    }
  }
}
