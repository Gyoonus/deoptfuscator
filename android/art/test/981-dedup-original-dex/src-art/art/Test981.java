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

import java.lang.reflect.Field;
import java.util.Base64;
import java.nio.ByteBuffer;

import dalvik.system.ClassExt;
import dalvik.system.InMemoryDexClassLoader;

public class Test981 {

  static class Transform {
    public void sayHi() {
      System.out.println("hello");
    }
  }

  static class Transform2 {
    public void sayHi() {
      System.out.println("hello2");
    }
  }

  /**
   * base64 encoded class/dex file for
   * static class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES_1 = Base64.getDecoder().decode(
    "ZGV4CjAzNQB+giqQAAAAAAAAAAAAAAAAAAAAAAAAAAC4AwAAcAAAAHhWNBIAAAAAAAAAAPQCAAAU" +
    "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAAB0AgAARAEAAEQB" +
    "AABMAQAAVQEAAG4BAAB9AQAAoQEAAMEBAADYAQAA7AEAAAACAAAUAgAAIgIAAC0CAAAwAgAANAIA" +
    "AEECAABHAgAATAIAAFUCAABcAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAMAAAA" +
    "DAAAAAgAAAAAAAAADQAAAAgAAABkAgAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
    "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAKAAAA5AIAALgCAAAAAAAABjxpbml0PgAHR29vZGJ5ZQAX" +
    "TGFydC9UZXN0OTgxJFRyYW5zZm9ybTsADUxhcnQvVGVzdDk4MTsAIkxkYWx2aWsvYW5ub3RhdGlv" +
    "bi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEv" +
    "aW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAS" +
    "TGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0OTgxLmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vz" +
    "c0ZsYWdzAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQAAAQAAAAYAAAAFAAcOAAcA" +
    "Bw4BCA8AAAAAAQABAAEAAABsAgAABAAAAHAQAwAAAA4AAwABAAIAAABxAgAACQAAAGIAAAAbAQEA" +
    "AABuIAIAEAAOAAAAAAABAQCAgAT8BAEBlAUAAAICARMYAQIDAg4ECA8XCwACAAAAyAIAAM4CAADY" +
    "AgAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAUAAAAcAAAAAIAAAAJAAAAwAAAAAMA" +
    "AAACAAAA5AAAAAQAAAABAAAA/AAAAAUAAAAEAAAABAEAAAYAAAABAAAAJAEAAAIgAAAUAAAARAEA" +
    "AAEQAAABAAAAZAIAAAMgAAACAAAAbAIAAAEgAAACAAAAfAIAAAAgAAABAAAAuAIAAAQgAAACAAAA" +
    "yAIAAAMQAAABAAAA2AIAAAYgAAABAAAA5AIAAAAQAAABAAAA9AIAAA==");

  /**
   * base64 encoded class/dex file for
   * static class Transform2 {
   *   public void sayHi() {
   *    System.out.println("Goodbye2");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES_2 = Base64.getDecoder().decode(
    "ZGV4CjAzNQAhg+RVAAAAAAAAAAAAAAAAAAAAAAAAAAC8AwAAcAAAAHhWNBIAAAAAAAAAAPgCAAAU" +
    "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAAB4AgAARAEAAEQB" +
    "AABMAQAAVgEAAHABAAB/AQAAowEAAMMBAADaAQAA7gEAAAICAAAWAgAAJAIAADACAAAzAgAANwIA" +
    "AEQCAABKAgAATwIAAFgCAABfAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAMAAAA" +
    "DAAAAAgAAAAAAAAADQAAAAgAAABoAgAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
    "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAKAAAA6AIAALwCAAAAAAAABjxpbml0PgAIR29vZGJ5ZTIA" +
    "GExhcnQvVGVzdDk4MSRUcmFuc2Zvcm0yOwANTGFydC9UZXN0OTgxOwAiTGRhbHZpay9hbm5vdGF0" +
    "aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ABVMamF2" +
    "YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7" +
    "ABJMamF2YS9sYW5nL1N5c3RlbTsADFRlc3Q5ODEuamF2YQAKVHJhbnNmb3JtMgABVgACVkwAC2Fj" +
    "Y2Vzc0ZsYWdzAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQAAAAEAAAAGAAAACgAH" +
    "DgAMAAcOAQgPAAAAAAEAAQABAAAAcAIAAAQAAABwEAMAAAAOAAMAAQACAAAAdQIAAAkAAABiAAAA" +
    "GwEBAAAAbiACABAADgAAAAAAAQEAgIAEgAUBAZgFAAACAgETGAECAwIOBAgPFwsAAgAAAMwCAADS" +
    "AgAA3AIAAAAAAAAAAAAAAAAAABAAAAAAAAAAAQAAAAAAAAABAAAAFAAAAHAAAAACAAAACQAAAMAA" +
    "AAADAAAAAgAAAOQAAAAEAAAAAQAAAPwAAAAFAAAABAAAAAQBAAAGAAAAAQAAACQBAAACIAAAFAAA" +
    "AEQBAAABEAAAAQAAAGgCAAADIAAAAgAAAHACAAABIAAAAgAAAIACAAAAIAAAAQAAALwCAAAEIAAA" +
    "AgAAAMwCAAADEAAAAQAAANwCAAAGIAAAAQAAAOgCAAAAEAAAAQAAAPgCAAA=");

  /**
   * base64 encoded class/dex file for
   * class Transform3 {
   *   public void sayHi() {
   *    System.out.println("hello3");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES_3_INITIAL = Base64.getDecoder().decode(
    "ZGV4CjAzNQC2W2fBsAeLNAwWYlG8FVigzfsV7nBWITzQAgAAcAAAAHhWNBIAAAAAAAAAADACAAAO" +
    "AAAAcAAAAAYAAACoAAAAAgAAAMAAAAABAAAA2AAAAAQAAADgAAAAAQAAAAABAACwAQAAIAEAAGIB" +
    "AABqAQAAeAEAAI8BAACjAQAAtwEAAMsBAADcAQAA3wEAAOMBAAD3AQAA/wEAAAQCAAANAgAAAQAA" +
    "AAIAAAADAAAABAAAAAUAAAAHAAAABwAAAAUAAAAAAAAACAAAAAUAAABcAQAABAABAAsAAAAAAAAA" +
    "AAAAAAAAAAANAAAAAQABAAwAAAACAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAGAAAAAAAAAB8CAAAA" +
    "AAAAAQABAAEAAAAUAgAABAAAAHAQAwAAAA4AAwABAAIAAAAZAgAACQAAAGIAAAAbAQoAAABuIAIA" +
    "EAAOAAAAAQAAAAMABjxpbml0PgAMTFRyYW5zZm9ybTM7ABVMamF2YS9pby9QcmludFN0cmVhbTsA" +
    "EkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7ABJMamF2YS9sYW5nL1N5c3Rl" +
    "bTsAD1RyYW5zZm9ybTMuamF2YQABVgACVkwAEmVtaXR0ZXI6IGphY2stNC4zMAAGaGVsbG8zAANv" +
    "dXQAB3ByaW50bG4ABXNheUhpAAIABw4ABAAHDocAAAABAQCAgASgAgEBuAIAAAANAAAAAAAAAAEA" +
    "AAAAAAAAAQAAAA4AAABwAAAAAgAAAAYAAACoAAAAAwAAAAIAAADAAAAABAAAAAEAAADYAAAABQAA" +
    "AAQAAADgAAAABgAAAAEAAAAAAQAAASAAAAIAAAAgAQAAARAAAAEAAABcAQAAAiAAAA4AAABiAQAA" +
    "AyAAAAIAAAAUAgAAACAAAAEAAAAfAgAAABAAAAEAAAAwAgAA");

  /**
   * base64 encoded class/dex file for
   * class Transform3 {
   *   public void sayHi() {
   *    System.out.println("Goodbye3");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES_3_FINAL = Base64.getDecoder().decode(
    "ZGV4CjAzNQBAXE5GthgMydaFBuinf+ZBfXcBYIw2UlXQAgAAcAAAAHhWNBIAAAAAAAAAADACAAAO" +
    "AAAAcAAAAAYAAACoAAAAAgAAAMAAAAABAAAA2AAAAAQAAADgAAAAAQAAAAABAACwAQAAIAEAAGIB" +
    "AABqAQAAdAEAAIIBAACZAQAArQEAAMEBAADVAQAA5gEAAOkBAADtAQAAAQIAAAYCAAAPAgAAAgAA" +
    "AAMAAAAEAAAABQAAAAYAAAAIAAAACAAAAAUAAAAAAAAACQAAAAUAAABcAQAABAABAAsAAAAAAAAA" +
    "AAAAAAAAAAANAAAAAQABAAwAAAACAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAHAAAAAAAAACECAAAA" +
    "AAAAAQABAAEAAAAWAgAABAAAAHAQAwAAAA4AAwABAAIAAAAbAgAACQAAAGIAAAAbAQEAAABuIAIA" +
    "EAAOAAAAAQAAAAMABjxpbml0PgAIR29vZGJ5ZTMADExUcmFuc2Zvcm0zOwAVTGphdmEvaW8vUHJp" +
    "bnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwASTGphdmEv" +
    "bGFuZy9TeXN0ZW07AA9UcmFuc2Zvcm0zLmphdmEAAVYAAlZMABJlbWl0dGVyOiBqYWNrLTQuMzAA" +
    "A291dAAHcHJpbnRsbgAFc2F5SGkAAgAHDgAEAAcOhwAAAAEBAICABKACAQG4AgANAAAAAAAAAAEA" +
    "AAAAAAAAAQAAAA4AAABwAAAAAgAAAAYAAACoAAAAAwAAAAIAAADAAAAABAAAAAEAAADYAAAABQAA" +
    "AAQAAADgAAAABgAAAAEAAAAAAQAAASAAAAIAAAAgAQAAARAAAAEAAABcAQAAAiAAAA4AAABiAQAA" +
    "AyAAAAIAAAAWAgAAACAAAAEAAAAhAgAAABAAAAEAAAAwAgAA");

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_RETRANSFORM);
    doTest();
  }

  private static void assertSame(Object a, Object b) throws Exception {
    if (a != b) {
      throw new AssertionError("'" + (a != null ? a.toString() : "null") + "' is not the same as " +
                               "'" + (b != null ? b.toString() : "null") + "'");
    }
  }

  private static Object getObjectField(Object o, String name) throws Exception {
    return getObjectField(o, o.getClass(), name);
  }

  private static Object getObjectField(Object o, Class<?> type, String name) throws Exception {
    Field f = type.getDeclaredField(name);
    f.setAccessible(true);
    return f.get(o);
  }

  private static Object getOriginalDexFile(Class<?> k) throws Exception {
    ClassExt ext_data_object = (ClassExt) getObjectField(k, "extData");
    if (ext_data_object == null) {
      return null;
    }

    return getObjectField(ext_data_object, "originalDexFile");
  }

  public static void doTest() throws Exception {
    // Make sure both of these are loaded prior to transformations being added so they have the same
    // original dex files.
    Transform t1 = new Transform();
    Transform2 t2 = new Transform2();

    assertSame(null, getOriginalDexFile(t1.getClass()));
    assertSame(null, getOriginalDexFile(t2.getClass()));
    assertSame(null, getOriginalDexFile(Test981.class));

    Redefinition.addCommonTransformationResult("art/Test981$Transform", new byte[0], DEX_BYTES_1);
    Redefinition.addCommonTransformationResult("art/Test981$Transform2", new byte[0], DEX_BYTES_2);
    Redefinition.enableCommonRetransformation(true);
    Redefinition.doCommonClassRetransformation(Transform.class, Transform2.class);

    assertSame(getOriginalDexFile(t1.getClass()), getOriginalDexFile(t2.getClass()));
    assertSame(null, getOriginalDexFile(Test981.class));
    // Make sure that the original dex file is a DexCache object.
    assertSame(getOriginalDexFile(t1.getClass()).getClass(), Class.forName("java.lang.DexCache"));

    // Check that we end up with a byte[] if we do a direct RedefineClasses
    Redefinition.enableCommonRetransformation(false);
    Redefinition.doCommonClassRedefinition(Transform.class, new byte[0], DEX_BYTES_1);
    assertSame((new byte[0]).getClass(), getOriginalDexFile(t1.getClass()).getClass());

    // Check we don't have anything if we don't have any originalDexFile if the onload
    // transformation doesn't do anything.
    Redefinition.enableCommonRetransformation(true);
    Class<?> transform3Class = new InMemoryDexClassLoader(
        ByteBuffer.wrap(DEX_BYTES_3_INITIAL), Test981.class.getClassLoader()).loadClass("Transform3");
    assertSame(null, getOriginalDexFile(transform3Class));

    // Check that we end up with a java.lang.Long pointer if we do an 'on-load' redefinition.
    Redefinition.addCommonTransformationResult("Transform3", new byte[0], DEX_BYTES_3_FINAL);
    Redefinition.enableCommonRetransformation(true);
    Class<?> transform3ClassTransformed = new InMemoryDexClassLoader(
        ByteBuffer.wrap(DEX_BYTES_3_INITIAL), Test981.class.getClassLoader()).loadClass("Transform3");
    assertSame(Long.class, getOriginalDexFile(transform3ClassTransformed).getClass());
  }
}
