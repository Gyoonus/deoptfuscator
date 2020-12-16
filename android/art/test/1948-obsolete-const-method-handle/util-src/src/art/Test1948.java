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

package art;
import art.constmethodhandle.TestInvoke;
import java.util.*;
import java.lang.reflect.*;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public class Test1948 {
  // These are initialized by a method added by test_generator.
  // They will contain the dex bytes of TestInvoker but with the method handle changed to pointing
  // to sayBye.
  public static final byte[] CLASS_BYTES;
  public static final byte[] DEX_BYTES;
  static {
    try {
      // TestGenerator will add functions that get the base64 string of these functions. When we
      // compile this the functions haven't been generated yet though so just do things this way.
      MethodHandle getClassBase64 = MethodHandles.lookup().findStatic(
          Test1948.class, "getClassBase64", MethodType.methodType(String.class));
      MethodHandle getDexBase64 = MethodHandles.lookup().findStatic(
          Test1948.class, "getDexBase64", MethodType.methodType(String.class));
      CLASS_BYTES = Base64.getDecoder().decode((String)getClassBase64.invokeExact());
      DEX_BYTES = Base64.getDecoder().decode((String)getDexBase64.invokeExact());
    } catch (Throwable e) {
      throw new Error("Failed to initialize statics: ", e);
    }
  }

  public static void run() throws Throwable {
    // NB Because we aren't using desugar we cannot use capturing-lambda or string concat anywhere
    // in this test! Version 9+ javac turns these into invokedynamics using bootstrap methods not
    // currently present in android.
    new TestInvoke().runTest(
        new Runnable() { public void run() { System.out.println("Do nothing"); } });
    new TestInvoke().runTest(
        new Runnable() {
          public void run() {
            System.out.println("transforming calling function");
            Redefinition.doCommonClassRedefinition(TestInvoke.class, CLASS_BYTES, DEX_BYTES);
          }
        });
    new TestInvoke().runTest(
        new Runnable() { public void run() { System.out.println("Do nothing"); } });
  }
}
