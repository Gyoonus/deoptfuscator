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

import java.util.Base64;
import java.util.concurrent.Semaphore;

public class Test951 {

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
    "KVYBAApTb3VyY2VGaWxlAQAMVGVzdDk1MS5qYXZhDAAJAAoHAB8MACAAIQEAE0hlbGxvIC0gVHJh" +
    "bnNmb3JtZWQHACIMACMAJAcAJQwAJgAKAQAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkBwAnAQAVYXJ0" +
    "L1Rlc3Q5NTEkVHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5n" +
    "L09iamVjdAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsB" +
    "ABNqYXZhL2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEA" +
    "EmphdmEvbGFuZy9SdW5uYWJsZQEAA3J1bgEAC2FydC9UZXN0OTUxACAABwAIAAAAAAACAAAACQAK" +
    "AAEACwAAAB0AAQABAAAABSq3AAGxAAAAAQAMAAAABgABAAAABQABAA0ADgABAAsAAAA7AAIAAgAA" +
    "ABeyAAISA7YABCu5AAUBALIAAhIGtgAEsQAAAAEADAAAABIABAAAAAcACAAIAA4ACQAWAAoAAgAP" +
    "AAAAAgAQAB0AAAAKAAEABwAaABwACA==");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBom/JeAAAAAAAAAAAAAAAAAAAAAAAAAAA8BAAAcAAAAHhWNBIAAAAAAAAAAHgDAAAX" +
    "AAAAcAAAAAoAAADMAAAAAwAAAPQAAAABAAAAGAEAAAUAAAAgAQAAAQAAAEgBAADUAgAAaAEAAGgB" +
    "AABwAQAAhwEAAJwBAAC1AQAAxAEAAOgBAAAIAgAAHwIAADMCAABJAgAAXQIAAHECAAB/AgAAigIA" +
    "AI0CAACRAgAAngIAAKQCAACpAgAAsgIAALcCAAC+AgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
    "CQAAAAoAAAALAAAADgAAAA4AAAAJAAAAAAAAAA8AAAAJAAAAyAIAAA8AAAAJAAAA0AIAAAgABAAS" +
    "AAAAAAAAAAAAAAAAAAEAFQAAAAQAAgATAAAABQAAAAAAAAAGAAAAFAAAAAAAAAAAAAAABQAAAAAA" +
    "AAAMAAAAaAMAADwDAAAAAAAABjxpbml0PgAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkABNIZWxsbyAt" +
    "IFRyYW5zZm9ybWVkABdMYXJ0L1Rlc3Q5NTEkVHJhbnNmb3JtOwANTGFydC9UZXN0OTUxOwAiTGRh" +
    "bHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVy" +
    "Q2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEv" +
    "bGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AAxU" +
    "ZXN0OTUxLmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAANvdXQAB3By" +
    "aW50bG4AA3J1bgAFc2F5SGkABXZhbHVlAAAAAAEAAAAGAAAAAQAAAAcAAAAFAAcOAAcBAAcOAQgP" +
    "AQMPAQgPAAEAAQABAAAA2AIAAAQAAABwEAMAAAAOAAQAAgACAAAA3QIAABQAAABiAAAAGwECAAAA" +
    "biACABAAchAEAAMAYgAAABsBAQAAAG4gAgAQAA4AAAABAQCAgATsBQEBhAYAAAICARYYAQIDAhAE" +
    "CBEXDQACAAAATAMAAFIDAABcAwAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAXAAAA" +
    "cAAAAAIAAAAKAAAAzAAAAAMAAAADAAAA9AAAAAQAAAABAAAAGAEAAAUAAAAFAAAAIAEAAAYAAAAB" +
    "AAAASAEAAAIgAAAXAAAAaAEAAAEQAAACAAAAyAIAAAMgAAACAAAA2AIAAAEgAAACAAAA7AIAAAAg" +
    "AAABAAAAPAMAAAQgAAACAAAATAMAAAMQAAABAAAAXAMAAAYgAAABAAAAaAMAAAAQAAABAAAAeAMA" +
    "AA==");

  public static void run() {
    // Semaphores to let each thread know where the other is. We could use barriers but semaphores
    // mean we don't need to have the worker thread be waiting around.
    final Semaphore sem_redefine_start = new Semaphore(0);
    final Semaphore sem_redefine_end = new Semaphore(0);
    // Create a thread to do the actual redefinition. We will just communicate through an
    // atomic-integer.
    new Thread(() -> {
      try {
        // Wait for the other thread to ask for redefinition.
        sem_redefine_start.acquire();
        // Do the redefinition.
        Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
        // Allow the other thread to wake up if it is waiting.
        sem_redefine_end.release();
      } catch (InterruptedException e) {
        throw new Error("unable to do redefinition", e);
      }
    }).start();

    Transform t = new Transform();
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
    t.sayHi(() -> {
      try {
        System.out.println("transforming calling function");
        // Wake up the waiting thread.
        sem_redefine_start.release();
        // Wait for the other thread to finish with redefinition.
        sem_redefine_end.acquire();
      } catch (InterruptedException e) {
        throw new Error("unable to do redefinition", e);
      }
    });
    t.sayHi(() -> { System.out.println("Not doing anything here"); });
  }
}
