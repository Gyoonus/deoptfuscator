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

import static art.Redefinition.CommonClassDefinition;

import java.util.Arrays;
import java.util.ArrayList;
import java.util.Base64;
import java.lang.reflect.*;
public class Test944 {

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
  private static CommonClassDefinition TRANSFORM_DEFINITION = new CommonClassDefinition(
      Transform.class,
      Base64.getDecoder().decode(
        "yv66vgAAADQAIAoABgAOCQAPABAIABEKABIAEwcAFQcAGAEABjxpbml0PgEAAygpVgEABENvZGUB" +
        "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAAxUZXN0OTQ0LmphdmEMAAcA" +
        "CAcAGQwAGgAbAQAHR29vZGJ5ZQcAHAwAHQAeBwAfAQAVYXJ0L1Rlc3Q5NDQkVHJhbnNmb3JtAQAJ" +
        "VHJhbnNmb3JtAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5nL09iamVjdAEAEGphdmEvbGFuZy9T" +
        "eXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsBABNqYXZhL2lvL1ByaW50U3RyZWFt" +
        "AQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEAC2FydC9UZXN0OTQ0ACAABQAGAAAA" +
        "AAACAAAABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAACgABAAsACAABAAkA" +
        "AAAlAAIAAQAAAAmyAAISA7YABLEAAAABAAoAAAAKAAIAAAAMAAgADQACAAwAAAACAA0AFwAAAAoA" +
        "AQAFABQAFgAI"),
      Base64.getDecoder().decode(
        "ZGV4CjAzNQCFgsuWAAAAAAAAAAAAAAAAAAAAAAAAAAC4AwAAcAAAAHhWNBIAAAAAAAAAAPQCAAAU" +
        "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAAB0AgAARAEAAEQB" +
        "AABMAQAAVQEAAG4BAAB9AQAAoQEAAMEBAADYAQAA7AEAAAACAAAUAgAAIgIAAC0CAAAwAgAANAIA" +
        "AEECAABHAgAATAIAAFUCAABcAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAMAAAA" +
        "DAAAAAgAAAAAAAAADQAAAAgAAABkAgAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
        "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAKAAAA5AIAALgCAAAAAAAABjxpbml0PgAHR29vZGJ5ZQAX" +
        "TGFydC9UZXN0OTQ0JFRyYW5zZm9ybTsADUxhcnQvVGVzdDk0NDsAIkxkYWx2aWsvYW5ub3RhdGlv" +
        "bi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwAVTGphdmEv" +
        "aW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAS" +
        "TGphdmEvbGFuZy9TeXN0ZW07AAxUZXN0OTQ0LmphdmEACVRyYW5zZm9ybQABVgACVkwAC2FjY2Vz" +
        "c0ZsYWdzAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQAAAQAAAAYAAAAKAAcOAAwA" +
        "Bw4BCA8AAAAAAQABAAEAAABsAgAABAAAAHAQAwAAAA4AAwABAAIAAABxAgAACQAAAGIAAAAbAQEA" +
        "AABuIAIAEAAOAAAAAAABAQCAgAT8BAEBlAUAAAICARMYAQIDAg4ECA8XCwACAAAAyAIAAM4CAADY" +
        "AgAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAUAAAAcAAAAAIAAAAJAAAAwAAAAAMA" +
        "AAACAAAA5AAAAAQAAAABAAAA/AAAAAUAAAAEAAAABAEAAAYAAAABAAAAJAEAAAIgAAAUAAAARAEA" +
        "AAEQAAABAAAAZAIAAAMgAAACAAAAbAIAAAEgAAACAAAAfAIAAAAgAAABAAAAuAIAAAQgAAACAAAA" +
        "yAIAAAMQAAABAAAA2AIAAAYgAAABAAAA5AIAAAAQAAABAAAA9AIAAA=="));

  /**
   * base64 encoded class/dex file for
   * static class Transform2 {
   *   public void sayHi() {
   *    System.out.println("Goodbye2");
   *   }
   * }
   */
  private static CommonClassDefinition TRANSFORM2_DEFINITION = new CommonClassDefinition(
      Transform2.class,
      Base64.getDecoder().decode(
        "yv66vgAAADQAIAoABgAOCQAPABAIABEKABIAEwcAFQcAGAEABjxpbml0PgEAAygpVgEABENvZGUB" +
        "AA9MaW5lTnVtYmVyVGFibGUBAAVzYXlIaQEAClNvdXJjZUZpbGUBAAxUZXN0OTQ0LmphdmEMAAcA" +
        "CAcAGQwAGgAbAQAIR29vZGJ5ZTIHABwMAB0AHgcAHwEAFmFydC9UZXN0OTQ0JFRyYW5zZm9ybTIB" +
        "AApUcmFuc2Zvcm0yAQAMSW5uZXJDbGFzc2VzAQAQamF2YS9sYW5nL09iamVjdAEAEGphdmEvbGFu" +
        "Zy9TeXN0ZW0BAANvdXQBABVMamF2YS9pby9QcmludFN0cmVhbTsBABNqYXZhL2lvL1ByaW50U3Ry" +
        "ZWFtAQAHcHJpbnRsbgEAFShMamF2YS9sYW5nL1N0cmluZzspVgEAC2FydC9UZXN0OTQ0ACAABQAG" +
        "AAAAAAACAAAABwAIAAEACQAAAB0AAQABAAAABSq3AAGxAAAAAQAKAAAABgABAAAABQABAAsACAAB" +
        "AAkAAAAlAAIAAQAAAAmyAAISA7YABLEAAAABAAoAAAAKAAIAAAAHAAgACAACAAwAAAACAA0AFwAA" +
        "AAoAAQAFABQAFgAI"),
      Base64.getDecoder().decode(
        "ZGV4CjAzNQAUg8BCAAAAAAAAAAAAAAAAAAAAAAAAAAC8AwAAcAAAAHhWNBIAAAAAAAAAAPgCAAAU" +
        "AAAAcAAAAAkAAADAAAAAAgAAAOQAAAABAAAA/AAAAAQAAAAEAQAAAQAAACQBAAB4AgAARAEAAEQB" +
        "AABMAQAAVgEAAHABAAB/AQAAowEAAMMBAADaAQAA7gEAAAICAAAWAgAAJAIAADACAAAzAgAANwIA" +
        "AEQCAABKAgAATwIAAFgCAABfAgAAAgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAMAAAA" +
        "DAAAAAgAAAAAAAAADQAAAAgAAABoAgAABwAEABAAAAAAAAAAAAAAAAAAAAASAAAABAABABEAAAAF" +
        "AAAAAAAAAAAAAAAAAAAABQAAAAAAAAAKAAAA6AIAALwCAAAAAAAABjxpbml0PgAIR29vZGJ5ZTIA" +
        "GExhcnQvVGVzdDk0NCRUcmFuc2Zvcm0yOwANTGFydC9UZXN0OTQ0OwAiTGRhbHZpay9hbm5vdGF0" +
        "aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ABVMamF2" +
        "YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7" +
        "ABJMamF2YS9sYW5nL1N5c3RlbTsADFRlc3Q5NDQuamF2YQAKVHJhbnNmb3JtMgABVgACVkwAC2Fj" +
        "Y2Vzc0ZsYWdzAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhpAAV2YWx1ZQAAAAEAAAAGAAAABQAH" +
        "DgAHAAcOAQgPAAAAAAEAAQABAAAAcAIAAAQAAABwEAMAAAAOAAMAAQACAAAAdQIAAAkAAABiAAAA" +
        "GwEBAAAAbiACABAADgAAAAAAAQEAgIAEgAUBAZgFAAACAgETGAECAwIOBAgPFwsAAgAAAMwCAADS" +
        "AgAA3AIAAAAAAAAAAAAAAAAAABAAAAAAAAAAAQAAAAAAAAABAAAAFAAAAHAAAAACAAAACQAAAMAA" +
        "AAADAAAAAgAAAOQAAAAEAAAAAQAAAPwAAAAFAAAABAAAAAQBAAAGAAAAAQAAACQBAAACIAAAFAAA" +
        "AEQBAAABEAAAAQAAAGgCAAADIAAAAgAAAHACAAABIAAAAgAAAIACAAAAIAAAAQAAALwCAAAEIAAA" +
        "AgAAAMwCAAADEAAAAQAAANwCAAAGIAAAAQAAAOgCAAAAEAAAAQAAAPgCAAA="));

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
    System.out.println("Passed");
  }

  private static void checkIsInstance(Class<?> klass, Object o) throws Exception {
    if (!klass.isInstance(o)) {
      throw new Exception(klass + " is not the class of " + o);
    }
  }

  private static boolean arrayContains(long[] arr, long value) {
    if (arr == null) {
      return false;
    }
    for (int i = 0; i < arr.length; i++) {
      if (arr[i] == value) {
        return true;
      }
    }
    return false;
  }

  /**
   * Checks that we can find the dex-file for the given class in its classloader.
   *
   * Throws if it fails.
   */
  private static void checkDexFileInClassLoader(Class<?> klass) throws Exception {
    // If all the android BCP classes were availible when compiling this test and access checks
    // weren't a thing this function would be written as follows:
    //
    // long dexFilePtr = getDexFilePointer(klass);
    // dalvik.system.BaseDexClassLoader loader =
    //     (dalvik.system.BaseDexClassLoader)klass.getClassLoader();
    // dalvik.system.DexPathList pathListValue = loader.pathList;
    // dalvik.system.DexPathList.Element[] elementArrayValue = pathListValue.dexElements;
    // int array_length = elementArrayValue.length;
    // for (int i = 0; i < array_length; i++) {
    //   dalvik.system.DexPathList.Element curElement = elementArrayValue[i];
    //   dalvik.system.DexFile curDexFile = curElement.dexFile;
    //   if (curDexFile == null) {
    //     continue;
    //   }
    //   long[] curCookie = (long[])curDexFile.mCookie;
    //   long[] curInternalCookie = (long[])curDexFile.mInternalCookie;
    //   if (arrayContains(curCookie, dexFilePtr) || arrayContains(curInternalCookie, dexFilePtr)) {
    //     return;
    //   }
    // }
    // throw new Exception(
    //     "Unable to find dex file pointer " + dexFilePtr + " in class loader for " + klass);

    // Get all the fields and classes we need by reflection.
    Class<?> baseDexClassLoaderClass = Class.forName("dalvik.system.BaseDexClassLoader");
    Field pathListField = baseDexClassLoaderClass.getDeclaredField("pathList");

    Class<?> dexPathListClass = Class.forName("dalvik.system.DexPathList");
    Field elementArrayField = dexPathListClass.getDeclaredField("dexElements");

    Class<?> dexPathListElementClass = Class.forName("dalvik.system.DexPathList$Element");
    Field dexFileField = dexPathListElementClass.getDeclaredField("dexFile");

    Class<?> dexFileClass = Class.forName("dalvik.system.DexFile");
    Field dexFileCookieField = dexFileClass.getDeclaredField("mCookie");
    Field dexFileInternalCookieField = dexFileClass.getDeclaredField("mInternalCookie");

    // Make all the fields accessible
    AccessibleObject.setAccessible(new AccessibleObject[] { pathListField,
                                                            elementArrayField,
                                                            dexFileField,
                                                            dexFileCookieField,
                                                            dexFileInternalCookieField }, true);

    long dexFilePtr = getDexFilePointer(klass);

    ClassLoader loader = klass.getClassLoader();
    checkIsInstance(baseDexClassLoaderClass, loader);
    // DexPathList pathListValue = ((BaseDexClassLoader) loader).pathList;
    Object pathListValue = pathListField.get(loader);

    checkIsInstance(dexPathListClass, pathListValue);

    // DexPathList.Element[] elementArrayValue = pathListValue.dexElements;
    Object elementArrayValue = elementArrayField.get(pathListValue);
    if (!elementArrayValue.getClass().isArray() ||
        elementArrayValue.getClass().getComponentType() != dexPathListElementClass) {
      throw new Exception("elementArrayValue is not an " + dexPathListElementClass + " array!");
    }
    // int array_length = elementArrayValue.length;
    int array_length = Array.getLength(elementArrayValue);
    for (int i = 0; i < array_length; i++) {
      // DexPathList.Element curElement = elementArrayValue[i];
      Object curElement = Array.get(elementArrayValue, i);
      checkIsInstance(dexPathListElementClass, curElement);

      // DexFile curDexFile = curElement.dexFile;
      Object curDexFile = dexFileField.get(curElement);
      if (curDexFile == null) {
        continue;
      }
      checkIsInstance(dexFileClass, curDexFile);

      // long[] curCookie = (long[])curDexFile.mCookie;
      long[] curCookie = (long[])dexFileCookieField.get(curDexFile);
      // long[] curInternalCookie = (long[])curDexFile.mInternalCookie;
      long[] curInternalCookie = (long[])dexFileInternalCookieField.get(curDexFile);

      if (arrayContains(curCookie, dexFilePtr) || arrayContains(curInternalCookie, dexFilePtr)) {
        return;
      }
    }
    throw new Exception(
        "Unable to find dex file pointer " + dexFilePtr + " in class loader for " + klass);
  }

  private static void doTest() throws Exception {
    Transform t = new Transform();
    Transform2 t2 = new Transform2();

    long initial_t1_dex = getDexFilePointer(Transform.class);
    long initial_t2_dex = getDexFilePointer(Transform2.class);
    if (initial_t2_dex != initial_t1_dex) {
      throw new Exception("The classes " + Transform.class + " and " + Transform2.class + " " +
                          "have different initial dex files!");
    }
    checkDexFileInClassLoader(Transform.class);
    checkDexFileInClassLoader(Transform2.class);

    // Make sure they are loaded
    t.sayHi();
    t2.sayHi();
    // Redefine both of the classes.
    Redefinition.doMultiClassRedefinition(TRANSFORM_DEFINITION, TRANSFORM2_DEFINITION);
    // Make sure we actually transformed them!
    t.sayHi();
    t2.sayHi();

    long final_t1_dex = getDexFilePointer(Transform.class);
    long final_t2_dex = getDexFilePointer(Transform2.class);
    if (final_t2_dex == final_t1_dex) {
      throw new Exception("The classes " + Transform.class + " and " + Transform2.class + " " +
                          "have the same initial dex files!");
    } else if (final_t1_dex == initial_t1_dex) {
      throw new Exception("The class " + Transform.class + " did not get a new dex file!");
    } else if (final_t2_dex == initial_t2_dex) {
      throw new Exception("The class " + Transform2.class + " did not get a new dex file!");
    }
    // Check to make sure the new dex files are in the class loader.
    checkDexFileInClassLoader(Transform.class);
    checkDexFileInClassLoader(Transform2.class);
  }

  // Gets the 'long' (really a native pointer) that is stored in the ClassLoader representing the
  // DexFile a class is loaded from. This is plucked out of the internal DexCache object associated
  // with the class.
  private static long getDexFilePointer(Class<?> target) throws Exception {
    // If all the android BCP classes were available when compiling this test and access checks
    // weren't a thing this function would be written as follows:
    //
    // java.lang.DexCache dexCacheObject = target.dexCache;
    // if (dexCacheObject == null) {
    //   return 0;
    // }
    // return dexCacheObject.dexFile;
    Field dexCacheField = Class.class.getDeclaredField("dexCache");

    Class<?> dexCacheClass = Class.forName("java.lang.DexCache");
    Field dexFileField = dexCacheClass.getDeclaredField("dexFile");

    AccessibleObject.setAccessible(new AccessibleObject[] { dexCacheField, dexFileField }, true);

    Object dexCacheObject = dexCacheField.get(target);
    if (dexCacheObject == null) {
      return 0;
    }
    checkIsInstance(dexCacheClass, dexCacheObject);
    return dexFileField.getLong(dexCacheObject);
  }
}
