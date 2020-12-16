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

import art.Redefinition;
import java.util.Base64;
public class Main {
  static abstract class TransformAbstract {
    public abstract void doSayHi();

    public void sayHi() {
      System.out.println("hello");
    }
  }

  static final class TransformConcrete extends TransformAbstract {
    public final void doSayHi() {
      System.out.print("Running sayHi() - ");
      sayHi();
    }
  }

  public static native void ensureJitCompiled(Class k, String m);

  /**
   * base64 encoded class/dex file for
   * static abstract class TransformAbstract {
   *   public abstract void doSayHi();
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
    "yv66vgAAADQAIQoABgAPCQAQABEIABIKABMAFAcAFgcAGQEABjxpbml0PgEAAygpVgEABENvZGUB" +
    "AA9MaW5lTnVtYmVyVGFibGUBAAdkb1NheUhpAQAFc2F5SGkBAApTb3VyY2VGaWxlAQAJTWFpbi5q" +
    "YXZhDAAHAAgHABoMABsAHAEAB0dvb2RieWUHAB0MAB4AHwcAIAEAFk1haW4kVHJhbnNmb3JtQWJz" +
    "dHJhY3QBABFUcmFuc2Zvcm1BYnN0cmFjdAEADElubmVyQ2xhc3NlcwEAEGphdmEvbGFuZy9PYmpl" +
    "Y3QBABBqYXZhL2xhbmcvU3lzdGVtAQADb3V0AQAVTGphdmEvaW8vUHJpbnRTdHJlYW07AQATamF2" +
    "YS9pby9QcmludFN0cmVhbQEAB3ByaW50bG4BABUoTGphdmEvbGFuZy9TdHJpbmc7KVYBAARNYWlu" +
    "BCAABQAGAAAAAAADAAAABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAAHAQB" +
    "AAsACAAAAAEADAAIAAEACQAAACUAAgABAAAACbIAAhIDtgAEsQAAAAEACgAAAAoAAgAAAB8ACAAg" +
    "AAIADQAAAAIADgAYAAAACgABAAUAFQAXBAg=");
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQCQkoTiKzIz0l96rtsnUxdY4Kwx+YINWFHEAwAAcAAAAHhWNBIAAAAAAAAAAAADAAAV" +
    "AAAAcAAAAAkAAADEAAAAAgAAAOgAAAABAAAAAAEAAAUAAAAIAQAAAQAAADABAAB0AgAAUAEAAKoB" +
    "AACyAQAAuwEAANUBAADdAQAAAQIAACECAAA4AgAATAIAAGACAAB0AgAAfwIAAJICAACVAgAAmQIA" +
    "AKYCAACvAgAAtQIAALoCAADDAgAAygIAAAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAgAAAAJAAAA" +
    "DAAAAAwAAAAIAAAAAAAAAA0AAAAIAAAApAEAAAcABAARAAAAAAAAAAAAAAAAAAAADwAAAAAAAAAT" +
    "AAAABAABABIAAAAFAAAAAAAAAAAAAAAABAAABQAAAAAAAAAKAAAAlAEAAOwCAAAAAAAAAgAAANwC" +
    "AADiAgAAAQABAAEAAADRAgAABAAAAHAQBAAAAA4AAwABAAIAAADWAgAACAAAAGIAAAAaAQEAbiAD" +
    "ABAADgBQAQAAAAAAAAAAAAAAAAAAAQAAAAYABjxpbml0PgAHR29vZGJ5ZQAYTE1haW4kVHJhbnNm" +
    "b3JtQWJzdHJhY3Q7AAZMTWFpbjsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsA" +
    "HkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJM" +
    "amF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07" +
    "AAlNYWluLmphdmEAEVRyYW5zZm9ybUFic3RyYWN0AAFWAAJWTAALYWNjZXNzRmxhZ3MAB2RvU2F5" +
    "SGkABG5hbWUAA291dAAHcHJpbnRsbgAFc2F5SGkABXZhbHVlABwABw4AHwAHDngAAgIBFBgBAgMC" +
    "DiQIBBAXCwAAAQIAgIAE3AIBgQgAAQH0AgAAEAAAAAAAAAABAAAAAAAAAAEAAAAVAAAAcAAAAAIA" +
    "AAAJAAAAxAAAAAMAAAACAAAA6AAAAAQAAAABAAAAAAEAAAUAAAAFAAAACAEAAAYAAAABAAAAMAEA" +
    "AAMQAAABAAAAUAEAAAEgAAACAAAAXAEAAAYgAAABAAAAlAEAAAEQAAABAAAApAEAAAIgAAAVAAAA" +
    "qgEAAAMgAAACAAAA0QIAAAQgAAACAAAA3AIAAAAgAAABAAAA7AIAAAAQAAABAAAAAAMAAA==");

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);

    ensureJitCompiled(TransformAbstract.class, "sayHi");
    ensureJitCompiled(TransformConcrete.class, "doSayHi");

    TransformAbstract t1 = new TransformConcrete();
    t1.doSayHi();

    assertSingleImplementation(TransformAbstract.class, "doSayHi", true);

    System.out.println("redefining TransformAbstract");
    Redefinition.doCommonClassRedefinition(TransformAbstract.class, CLASS_BYTES, DEX_BYTES);

    t1.doSayHi();
  }

  private static native boolean hasSingleImplementation(Class<?> clazz, String method_name);
  private static void assertSingleImplementation(Class<?> clazz, String method_name, boolean b) {
    if (hasSingleImplementation(clazz, method_name) != b) {
      System.out.println(clazz + "." + method_name +
          " doesn't have single implementation value of " + b);
    }
  }
}
