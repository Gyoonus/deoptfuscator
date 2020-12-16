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

package art;

import java.lang.reflect.Method;
import java.util.Base64;

public class Test984 {

  static class Transform {
    // This method must be 'static' so that when we try to invoke it through a j.l.r.Method we will
    // simply use the jmethodID directly and not do any lookup in any receiver object.
    public static void sayHi(Runnable r) {
      System.out.println("hello");
      r.run();
      System.out.println("goodbye");
    }
  }
  // static class Transform {
  //   public static void sayHi(Runnable r) {
  //     System.out.println("Hello - Transformed");
  //     r.run();
  //     System.out.println("Goodbye - Transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAKAoACAARCQASABMIABQKABUAFgsAFwAYCAAZBwAbBwAeAQAGPGluaXQ+AQADKClW" +
    "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7" +
    "KVYBAApTb3VyY2VGaWxlAQAMVGVzdDk4NC5qYXZhDAAJAAoHAB8MACAAIQEAE0hlbGxvIC0gVHJh" +
    "bnNmb3JtZWQHACIMACMAJAcAJQwAJgAKAQAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkBwAnAQAVYXJ0" +
    "L1Rlc3Q5ODQkVHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5n" +
    "L09iamVjdAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsB" +
    "ABNqYXZhL2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEA" +
    "EmphdmEvbGFuZy9SdW5uYWJsZQEAA3J1bgEAC2FydC9UZXN0OTg0ACAABwAIAAAAAAACAAAACQAK" +
    "AAEACwAAAB0AAQABAAAABSq3AAGxAAAAAQAMAAAABgABAAAABQAJAA0ADgABAAsAAAA7AAIAAQAA" +
    "ABeyAAISA7YABCq5AAUBALIAAhIGtgAEsQAAAAEADAAAABIABAAAAAcACAAIAA4ACQAWAAoAAgAP" +
    "AAAAAgAQAB0AAAAKAAEABwAaABwACA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQB/mxSMAAAAAAAAAAAAAAAAAAAAAAAAAAA8BAAAcAAAAHhWNBIAAAAAAAAAAHgDAAAX" +
    "AAAAcAAAAAoAAADMAAAAAwAAAPQAAAABAAAAGAEAAAUAAAAgAQAAAQAAAEgBAADUAgAAaAEAAGgB" +
    "AABwAQAAhwEAAJwBAAC1AQAAxAEAAOgBAAAIAgAAHwIAADMCAABJAgAAXQIAAHECAAB/AgAAigIA" +
    "AI0CAACRAgAAngIAAKQCAACpAgAAsgIAALcCAAC+AgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
    "CQAAAAoAAAALAAAADgAAAA4AAAAJAAAAAAAAAA8AAAAJAAAAyAIAAA8AAAAJAAAA0AIAAAgABAAS" +
    "AAAAAAAAAAAAAAAAAAEAFQAAAAQAAgATAAAABQAAAAAAAAAGAAAAFAAAAAAAAAAAAAAABQAAAAAA" +
    "AAAMAAAAaAMAADwDAAAAAAAABjxpbml0PgAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkABNIZWxsbyAt" +
    "IFRyYW5zZm9ybWVkABdMYXJ0L1Rlc3Q5ODQkVHJhbnNmb3JtOwANTGFydC9UZXN0OTg0OwAiTGRh" +
    "bHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVy" +
    "Q2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEv" +
    "bGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AAxU" +
    "ZXN0OTg0LmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAANvdXQAB3By" +
    "aW50bG4AA3J1bgAFc2F5SGkABXZhbHVlAAAAAAEAAAAGAAAAAQAAAAcAAAAFAAcOAAcBAAcOAQgP" +
    "AQMPAQgPAAEAAQABAAAA2AIAAAQAAABwEAMAAAAOAAMAAQACAAAA3QIAABQAAABiAAAAGwECAAAA" +
    "biACABAAchAEAAIAYgAAABsBAQAAAG4gAgAQAA4AAAACAACAgATsBQEJhAYAAAICARYYAQIDAhAE" +
    "CBEXDQACAAAATAMAAFIDAABcAwAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAXAAAA" +
    "cAAAAAIAAAAKAAAAzAAAAAMAAAADAAAA9AAAAAQAAAABAAAAGAEAAAUAAAAFAAAAIAEAAAYAAAAB" +
    "AAAASAEAAAIgAAAXAAAAaAEAAAEQAAACAAAAyAIAAAMgAAACAAAA2AIAAAEgAAACAAAA7AIAAAAg" +
    "AAABAAAAPAMAAAQgAAACAAAATAMAAAMQAAABAAAAXAMAAAYgAAABAAAAaAMAAAAQAAABAAAAeAMA" +
    "AA==");

  public static void run() {
    doTest();
  }

  // The Method that holds an obsolete method pointer. We will fill it in by getting a jmethodID
  // from a stack with an obsolete method in it. There should be no other ways to obtain an obsolete
  // jmethodID in ART without unsafe casts.
  public static Method obsolete_method = null;

  public static void doTest() {
    // Capture the obsolete method.
    //
    // NB The obsolete method must be direct so that we will not look in the receiver type to get
    // the actual method.
    Transform.sayHi(() -> {
      System.out.println("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
      System.out.println("Retrieving obsolete method from current stack");
      // This should get the obsolete sayHi method (as the only obsolete method on the current
      // threads stack).
      Test984.obsolete_method = getFirstObsoleteMethod984();
    });

    // Prove we did actually redefine something.
    System.out.println("Invoking redefined version of method.");
    Transform.sayHi(() -> { System.out.println("Not doing anything here"); });

    System.out.println("invoking obsolete method");
    try {
      obsolete_method.invoke(null, (Runnable)() -> {
        throw new Error("Unexpected code running from invoke of obsolete method!");
      });
      throw new Error("Running obsolete method did not throw exception");
    } catch (Throwable e) {
      if (e instanceof InternalError || e.getCause() instanceof InternalError) {
        System.out.println("Caught expected error from attempting to invoke an obsolete method.");
      } else {
        System.out.println("Unexpected error type for calling obsolete method! Expected either "
            + "an InternalError or something that is caused by an InternalError.");
        throw new Error("Unexpected error caught: ", e);
      }
    }
  }

  // Gets the first obsolete method on the current threads stack (NB only looks through the first 30
  // stack frames).
  private static native Method getFirstObsoleteMethod984();
}
