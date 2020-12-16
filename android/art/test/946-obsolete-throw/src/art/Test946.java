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

package art;

import java.util.Base64;

public class Test946 {

  static class Transform {
    public void sayHi(Runnable r) {
      System.out.println("hello");
      r.run();
      System.out.println("goodbye");
    }
  }

  // static class Transform {
  //   public void sayHi(Runnable r) {
  //     System.out.println("Hello - Transformed");
  //     r.run();
  //     System.out.println("Goodbye - Transformed");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAKAoACAARCQASABMIABQKABUAFgsAFwAYCAAZBwAbBwAeAQAGPGluaXQ+AQADKClW" +
    "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7" +
    "KVYBAApTb3VyY2VGaWxlAQAMVGVzdDk0Ni5qYXZhDAAJAAoHAB8MACAAIQEAE0hlbGxvIC0gVHJh" +
    "bnNmb3JtZWQHACIMACMAJAcAJQwAJgAKAQAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkBwAnAQAVYXJ0" +
    "L1Rlc3Q5NDYkVHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5n" +
    "L09iamVjdAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsB" +
    "ABNqYXZhL2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEA" +
    "EmphdmEvbGFuZy9SdW5uYWJsZQEAA3J1bgEAC2FydC9UZXN0OTQ2ACAABwAIAAAAAAACAAAACQAK" +
    "AAEACwAAAB0AAQABAAAABSq3AAGxAAAAAQAMAAAABgABAAAABQABAA0ADgABAAsAAAA7AAIAAgAA" +
    "ABeyAAISA7YABCu5AAUBALIAAhIGtgAEsQAAAAEADAAAABIABAAAAAcACAAIAA4ACQAWAAoAAgAP" +
    "AAAAAgAQAB0AAAAKAAEABwAaABwACA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQB0mzt6AAAAAAAAAAAAAAAAAAAAAAAAAAA8BAAAcAAAAHhWNBIAAAAAAAAAAHgDAAAX" +
    "AAAAcAAAAAoAAADMAAAAAwAAAPQAAAABAAAAGAEAAAUAAAAgAQAAAQAAAEgBAADUAgAAaAEAAGgB" +
    "AABwAQAAhwEAAJwBAAC1AQAAxAEAAOgBAAAIAgAAHwIAADMCAABJAgAAXQIAAHECAAB/AgAAigIA" +
    "AI0CAACRAgAAngIAAKQCAACpAgAAsgIAALcCAAC+AgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
    "CQAAAAoAAAALAAAADgAAAA4AAAAJAAAAAAAAAA8AAAAJAAAAyAIAAA8AAAAJAAAA0AIAAAgABAAS" +
    "AAAAAAAAAAAAAAAAAAEAFQAAAAQAAgATAAAABQAAAAAAAAAGAAAAFAAAAAAAAAAAAAAABQAAAAAA" +
    "AAAMAAAAaAMAADwDAAAAAAAABjxpbml0PgAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkABNIZWxsbyAt" +
    "IFRyYW5zZm9ybWVkABdMYXJ0L1Rlc3Q5NDYkVHJhbnNmb3JtOwANTGFydC9UZXN0OTQ2OwAiTGRh" +
    "bHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVy" +
    "Q2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEv" +
    "bGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AAxU" +
    "ZXN0OTQ2LmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAANvdXQAB3By" +
    "aW50bG4AA3J1bgAFc2F5SGkABXZhbHVlAAAAAAEAAAAGAAAAAQAAAAcAAAAFAAcOAAcBAAcOAQgP" +
    "AQMPAQgPAAEAAQABAAAA2AIAAAQAAABwEAMAAAAOAAQAAgACAAAA3QIAABQAAABiAAAAGwECAAAA" +
    "biACABAAchAEAAMAYgAAABsBAQAAAG4gAgAQAA4AAAABAQCAgATsBQEBhAYAAAICARYYAQIDAhAE" +
    "CBEXDQACAAAATAMAAFIDAABcAwAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAXAAAA" +
    "cAAAAAIAAAAKAAAAzAAAAAMAAAADAAAA9AAAAAQAAAABAAAAGAEAAAUAAAAFAAAAIAEAAAYAAAAB" +
    "AAAASAEAAAIgAAAXAAAAaAEAAAEQAAACAAAAyAIAAAMgAAACAAAA2AIAAAEgAAACAAAA7AIAAAAg" +
    "AAABAAAAPAMAAAQgAAACAAAATAMAAAMQAAABAAAAXAMAAAYgAAABAAAAaAMAAAAQAAABAAAAeAMA" +
    "AA==");

  public static void run() {
    doTest(new Transform());
  }

  static class DoRedefinitionClass implements Runnable {
    @Override
    public void run() {
      System.out.println("transforming calling function");
      Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
      throw new Error("Throwing exception into an obsolete method!");
    }
  }

  public static void doTest(Transform t) {
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
    try {
      t.sayHi(new DoRedefinitionClass());
    } catch (Throwable e) {
      System.out.println("Received error : " + e);
      e.printStackTrace(System.out);
    }
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
  }
}
