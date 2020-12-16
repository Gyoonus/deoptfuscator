/*
 * Copyright (C) 2018 The Android Open Source Project
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

package art.constmethodhandle;

import java.io.*;
import java.util.*;
import java.lang.invoke.CallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.nio.file.*;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;

// This test will modify in place the compiled java files to fill in the transformed version and
// fill in the TestInvoker.runTest function with a load-constant of a method-handle. It will use d8
// (passed in as an argument) to create the dex we will transform TestInvoke into.
public class TestGenerator {

  public static void main(String[] args) throws IOException {
    if (args.length != 2) {
      throw new Error("Unable to convert class to dex without d8 binary!");
    }
    Path base = Paths.get(args[0]);
    String d8Bin = args[1];

    Path initTestInvoke = base.resolve(TestGenerator.class.getPackage().getName().replace('.', '/'))
                              .resolve(TestInvoke.class.getSimpleName() + ".class");
    byte[] initClass = new FileInputStream(initTestInvoke.toFile()).readAllBytes();

    // Make the initial version of TestInvoker
    generateInvoker(initClass, "sayHi", new FileOutputStream(initTestInvoke.toFile()));

    // Make the final 'class' version of testInvoker
    ByteArrayOutputStream finalClass = new ByteArrayOutputStream();
    generateInvoker(initClass, "sayBye", finalClass);

    Path initTest1948 = base.resolve("art").resolve(art.Test1948.class.getSimpleName() + ".class");
    byte[] finalClassBytes = finalClass.toByteArray();
    byte[] finalDexBytes = getFinalDexBytes(d8Bin, finalClassBytes);
    generateTestCode(
        new FileInputStream(initTest1948.toFile()).readAllBytes(),
        finalClassBytes,
        finalDexBytes,
        new FileOutputStream(initTest1948.toFile()));
  }

  // Modify the Test1948 class bytecode so it has the transformed version of TestInvoker as a string
  // constant.
  private static void generateTestCode(
      byte[] initClass, byte[] transClass, byte[] transDex, OutputStream out) throws IOException {
    ClassReader cr = new ClassReader(initClass);
    ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
    cr.accept(
        new ClassVisitor(Opcodes.ASM6, cw) {
          @Override
          public void visitEnd() {
            generateStringAccessorMethod(
                cw, "getDexBase64", Base64.getEncoder().encodeToString(transDex));
            generateStringAccessorMethod(
                cw, "getClassBase64", Base64.getEncoder().encodeToString(transClass));
            super.visitEnd();
          }
        }, 0);
    out.write(cw.toByteArray());
  }

  // Insert a string accessor method so we can get the transformed versions of TestInvoker.
  private static void generateStringAccessorMethod(ClassVisitor cv, String name, String ret) {
    MethodVisitor mv = cv.visitMethod(
        Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC | Opcodes.ACC_SYNTHETIC,
        name, "()Ljava/lang/String;", null, null);
    mv.visitLdcInsn(ret);
    mv.visitInsn(Opcodes.ARETURN);
    mv.visitMaxs(-1, -1);
  }

  // Use d8bin to convert the classBytes into a dex file bytes. We need to do this here because we
  // need the dex-file bytes to be used by the test class to redefine TestInvoker. We use d8 because
  // it doesn't require setting up a directory structures or matching file names like dx does.
  // TODO We should maybe just call d8 functions directly?
  private static byte[] getFinalDexBytes(String d8Bin, byte[] classBytes) throws IOException {
    Path tempDir = Files.createTempDirectory("FinalTestInvoker_Gen");
    File tempInput = Files.createTempFile(tempDir, "temp_input_class", ".class").toFile();

    OutputStream tempClassStream = new FileOutputStream(tempInput);
    tempClassStream.write(classBytes);
    tempClassStream.close();
    tempClassStream = null;

    Process d8Proc = new ProcessBuilder(d8Bin,
                                        // Put classes.dex in the temp-dir we made.
                                        "--output", tempDir.toAbsolutePath().toString(),
                                        "--min-api", "28",  // Allow the new invoke ops.
                                        "--no-desugaring",  // Don't try to be clever please.
                                        tempInput.toPath().toAbsolutePath().toString())
        .inheritIO()  // Just print to stdio.
        .start();
    int res;
    try {
      res = d8Proc.waitFor();
    } catch (Exception e) {
      System.out.println("Failed to dex: ".concat(e.toString()));
      e.printStackTrace();
      res = -123;
    }
    tempInput.delete();
    try {
      if (res == 0) {
        byte[] out = new FileInputStream(tempDir.resolve("classes.dex").toFile()).readAllBytes();
        tempDir.resolve("classes.dex").toFile().delete();
        return out;
      }
    } finally {
      tempDir.toFile().delete();
    }
    throw new Error("Failed to get dex file! " + res);
  }

  private static void generateInvoker(
      byte[] inputClass,
      String toCall,
      OutputStream output) throws IOException {
    ClassReader cr = new ClassReader(inputClass);
    ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
    cr.accept(
        new ClassVisitor(Opcodes.ASM6, cw) {
          @Override
          public void visitEnd() {
            generateRunTest(cw, toCall);
            super.visitEnd();
          }
        }, 0);
    output.write(cw.toByteArray());
  }

  // Creates the following method:
  //   public runTest(Runnable preCall) {
  //     preCall.run();
  //     MethodHandle mh = <CONSTANT MH>;
  //     mh.invokeExact();
  //   }
  private static void generateRunTest(ClassVisitor cv, String toCall) {
    MethodVisitor mv = cv.visitMethod(Opcodes.ACC_PUBLIC,
                                      "runTest", "(Ljava/lang/Runnable;)V", null, null);
    MethodType mt = MethodType.methodType(Void.TYPE);
    Handle mh = new Handle(
        Opcodes.H_INVOKESTATIC,
        Type.getInternalName(Responses.class),
        toCall,
        mt.toMethodDescriptorString(),
        false);
    String internalName = Type.getInternalName(Runnable.class);
    mv.visitVarInsn(Opcodes.ALOAD, 1);
    mv.visitMethodInsn(Opcodes.INVOKEINTERFACE, internalName, "run", "()V", true);
    mv.visitLdcInsn(mh);
    mv.visitMethodInsn(
        Opcodes.INVOKEVIRTUAL,
        Type.getInternalName(MethodHandle.class),
        "invokeExact",
        "()V",
        false);
    mv.visitInsn(Opcodes.RETURN);
    mv.visitMaxs(-1, -1);
  }
}
