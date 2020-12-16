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

import static art.Redefinition.CommonClassDefinition;
import java.util.ArrayList;
import java.util.Base64;

public class Test926 {

  static class Transform {
    public void sayHi(Runnable r) {
      System.out.println("hello");
      r.run();
      System.out.println("goodbye");
    }
  }

  static class Transform2 {
    public void sayHi(Runnable r) {
      System.out.println("hello - 2");
      r.run();
      System.out.println("goodbye - 2");
    }
  }
  // static class Transform {
  //   public void sayHi(Runnable r) {
  //     System.out.println("Hello - Transformed");
  //     r.run();
  //     System.out.println("Goodbye - Transformed");
  //   }
  // }
  private static CommonClassDefinition VALID_DEFINITION_T1 = new CommonClassDefinition(
      Transform.class,
      Base64.getDecoder().decode(
        "yv66vgAAADQAKAoACAARCQASABMIABQKABUAFgsAFwAYCAAZBwAbBwAeAQAGPGluaXQ+AQADKClW" +
        "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7" +
        "KVYBAApTb3VyY2VGaWxlAQAMVGVzdDkyNi5qYXZhDAAJAAoHAB8MACAAIQEAE0hlbGxvIC0gVHJh" +
        "bnNmb3JtZWQHACIMACMAJAcAJQwAJgAKAQAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkBwAnAQAVYXJ0" +
        "L1Rlc3Q5MjYkVHJhbnNmb3JtAQAJVHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5n" +
        "L09iamVjdAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsB" +
        "ABNqYXZhL2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEA" +
        "EmphdmEvbGFuZy9SdW5uYWJsZQEAA3J1bgEAC2FydC9UZXN0OTI2ACAABwAIAAAAAAACAAAACQAK" +
        "AAEACwAAAB0AAQABAAAABSq3AAGxAAAAAQAMAAAABgABAAAADAABAA0ADgABAAsAAAA7AAIAAgAA" +
        "ABeyAAISA7YABCu5AAUBALIAAhIGtgAEsQAAAAEADAAAABIABAAAAA4ACAAPAA4AEAAWABEAAgAP" +
        "AAAAAgAQAB0AAAAKAAEABwAaABwACA=="),
      Base64.getDecoder().decode(
        "ZGV4CjAzNQB8m+R/AAAAAAAAAAAAAAAAAAAAAAAAAAA8BAAAcAAAAHhWNBIAAAAAAAAAAHgDAAAX" +
        "AAAAcAAAAAoAAADMAAAAAwAAAPQAAAABAAAAGAEAAAUAAAAgAQAAAQAAAEgBAADUAgAAaAEAAGgB" +
        "AABwAQAAhwEAAJwBAAC1AQAAxAEAAOgBAAAIAgAAHwIAADMCAABJAgAAXQIAAHECAAB/AgAAigIA" +
        "AI0CAACRAgAAngIAAKQCAACpAgAAsgIAALcCAAC+AgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
        "CQAAAAoAAAALAAAADgAAAA4AAAAJAAAAAAAAAA8AAAAJAAAAyAIAAA8AAAAJAAAA0AIAAAgABAAS" +
        "AAAAAAAAAAAAAAAAAAEAFQAAAAQAAgATAAAABQAAAAAAAAAGAAAAFAAAAAAAAAAAAAAABQAAAAAA" +
        "AAAMAAAAaAMAADwDAAAAAAAABjxpbml0PgAVR29vZGJ5ZSAtIFRyYW5zZm9ybWVkABNIZWxsbyAt" +
        "IFRyYW5zZm9ybWVkABdMYXJ0L1Rlc3Q5MjYkVHJhbnNmb3JtOwANTGFydC9UZXN0OTI2OwAiTGRh" +
        "bHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVy" +
        "Q2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEv" +
        "bGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07AAxU" +
        "ZXN0OTI2LmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAANvdXQAB3By" +
        "aW50bG4AA3J1bgAFc2F5SGkABXZhbHVlAAAAAAEAAAAGAAAAAQAAAAcAAAAMAAcOAA4BAAcOAQgP" +
        "AQMPAQgPAAEAAQABAAAA2AIAAAQAAABwEAMAAAAOAAQAAgACAAAA3QIAABQAAABiAAAAGwECAAAA" +
        "biACABAAchAEAAMAYgAAABsBAQAAAG4gAgAQAA4AAAABAQCAgATsBQEBhAYAAAICARYYAQIDAhAE" +
        "CBEXDQACAAAATAMAAFIDAABcAwAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAXAAAA" +
        "cAAAAAIAAAAKAAAAzAAAAAMAAAADAAAA9AAAAAQAAAABAAAAGAEAAAUAAAAFAAAAIAEAAAYAAAAB" +
        "AAAASAEAAAIgAAAXAAAAaAEAAAEQAAACAAAAyAIAAAMgAAACAAAA2AIAAAEgAAACAAAA7AIAAAAg" +
        "AAABAAAAPAMAAAQgAAACAAAATAMAAAMQAAABAAAAXAMAAAYgAAABAAAAaAMAAAAQAAABAAAAeAMA" +
        "AA=="));
  // static class Transform2 {
  //   public void sayHi(Runnable r) {
  //     System.out.println("Hello 2 - Transformed");
  //     r.run();
  //     System.out.println("Goodbye 2 - Transformed");
  //   }
  // }
  private static CommonClassDefinition VALID_DEFINITION_T2 = new CommonClassDefinition(
      Transform2.class,
      Base64.getDecoder().decode(
        "yv66vgAAADQAKAoACAARCQASABMIABQKABUAFgsAFwAYCAAZBwAbBwAeAQAGPGluaXQ+AQADKClW" +
        "AQAEQ29kZQEAD0xpbmVOdW1iZXJUYWJsZQEABXNheUhpAQAXKExqYXZhL2xhbmcvUnVubmFibGU7" +
        "KVYBAApTb3VyY2VGaWxlAQAMVGVzdDkyNi5qYXZhDAAJAAoHAB8MACAAIQEAFUhlbGxvIDIgLSBU" +
        "cmFuc2Zvcm1lZAcAIgwAIwAkBwAlDAAmAAoBABdHb29kYnllIDIgLSBUcmFuc2Zvcm1lZAcAJwEA" +
        "FmFydC9UZXN0OTI2JFRyYW5zZm9ybTIBAApUcmFuc2Zvcm0yAQAMSW5uZXJDbGFzc2VzAQAQamF2" +
        "YS9sYW5nL09iamVjdAEAEGphdmEvbGFuZy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0" +
        "cmVhbTsBABNqYXZhL2lvL1ByaW50U3RyZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmlu" +
        "ZzspVgEAEmphdmEvbGFuZy9SdW5uYWJsZQEAA3J1bgEAC2FydC9UZXN0OTI2ACAABwAIAAAAAAAC" +
        "AAAACQAKAAEACwAAAB0AAQABAAAABSq3AAGxAAAAAQAMAAAABgABAAAABQABAA0ADgABAAsAAAA7" +
        "AAIAAgAAABeyAAISA7YABCu5AAUBALIAAhIGtgAEsQAAAAEADAAAABIABAAAAAcACAAIAA4ACQAW" +
        "AAoAAgAPAAAAAgAQAB0AAAAKAAEABwAaABwACA=="),
      Base64.getDecoder().decode(
        "ZGV4CjAzNQBCnaUuAAAAAAAAAAAAAAAAAAAAAAAAAABABAAAcAAAAHhWNBIAAAAAAAAAAHwDAAAX" +
        "AAAAcAAAAAoAAADMAAAAAwAAAPQAAAABAAAAGAEAAAUAAAAgAQAAAQAAAEgBAADYAgAAaAEAAGgB" +
        "AABwAQAAiQEAAKABAAC6AQAAyQEAAO0BAAANAgAAJAIAADgCAABOAgAAYgIAAHYCAACEAgAAkAIA" +
        "AJMCAACXAgAApAIAAKoCAACvAgAAuAIAAL0CAADEAgAAAwAAAAQAAAAFAAAABgAAAAcAAAAIAAAA" +
        "CQAAAAoAAAALAAAADgAAAA4AAAAJAAAAAAAAAA8AAAAJAAAAzAIAAA8AAAAJAAAA1AIAAAgABAAS" +
        "AAAAAAAAAAAAAAAAAAEAFQAAAAQAAgATAAAABQAAAAAAAAAGAAAAFAAAAAAAAAAAAAAABQAAAAAA" +
        "AAAMAAAAbAMAAEADAAAAAAAABjxpbml0PgAXR29vZGJ5ZSAyIC0gVHJhbnNmb3JtZWQAFUhlbGxv" +
        "IDIgLSBUcmFuc2Zvcm1lZAAYTGFydC9UZXN0OTI2JFRyYW5zZm9ybTI7AA1MYXJ0L1Rlc3Q5MjY7" +
        "ACJMZGFsdmlrL2Fubm90YXRpb24vRW5jbG9zaW5nQ2xhc3M7AB5MZGFsdmlrL2Fubm90YXRpb24v" +
        "SW5uZXJDbGFzczsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwASTGphdmEvbGFuZy9PYmplY3Q7ABRM" +
        "amF2YS9sYW5nL1J1bm5hYmxlOwASTGphdmEvbGFuZy9TdHJpbmc7ABJMamF2YS9sYW5nL1N5c3Rl" +
        "bTsADFRlc3Q5MjYuamF2YQAKVHJhbnNmb3JtMgABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAANv" +
        "dXQAB3ByaW50bG4AA3J1bgAFc2F5SGkABXZhbHVlAAABAAAABgAAAAEAAAAHAAAABQAHDgAHAQAH" +
        "DgEIDwEDDwEIDwABAAEAAQAAANwCAAAEAAAAcBADAAAADgAEAAIAAgAAAOECAAAUAAAAYgAAABsB" +
        "AgAAAG4gAgAQAHIQBAADAGIAAAAbAQEAAABuIAIAEAAOAAAAAQEAgIAE8AUBAYgGAAACAgEWGAEC" +
        "AwIQBAgRFw0AAgAAAFADAABWAwAAYAMAAAAAAAAAAAAAAAAAABAAAAAAAAAAAQAAAAAAAAABAAAA" +
        "FwAAAHAAAAACAAAACgAAAMwAAAADAAAAAwAAAPQAAAAEAAAAAQAAABgBAAAFAAAABQAAACABAAAG" +
        "AAAAAQAAAEgBAAACIAAAFwAAAGgBAAABEAAAAgAAAMwCAAADIAAAAgAAANwCAAABIAAAAgAAAPAC" +
        "AAAAIAAAAQAAAEADAAAEIAAAAgAAAFADAAADEAAAAQAAAGADAAAGIAAAAQAAAGwDAAAAEAAAAQAA" +
        "AHwDAAA="));

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform(), new Transform2());
  }

  public static void doTest(final Transform t1, final Transform2 t2) throws Exception {
    t1.sayHi(() -> { t2.sayHi(() -> { System.out.println("Not doing anything here"); }); });
    t1.sayHi(() -> {
      t2.sayHi(() -> {
        System.out.println("transforming calling functions");
        Redefinition.doMultiClassRedefinition(VALID_DEFINITION_T1, VALID_DEFINITION_T2);
      });
    });
    t1.sayHi(() -> { t2.sayHi(() -> { System.out.println("Not doing anything here"); }); });
  }
}
