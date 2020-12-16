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
import java.util.Arrays;

public class Test918 {
  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    testField(Math.class, "PI");
    testField(Integer.class, "value");
    testField(Foo.class, "this$0");
    testField(Bar.class, "VAL");
    testField(Generics.class, "generics");
  }

  private static void testField(Class<?> base, String fieldName)
      throws Exception {
    Field f = base.getDeclaredField(fieldName);
    String[] result = getFieldName(f);
    System.out.println(Arrays.toString(result));

    Class<?> declClass = getFieldDeclaringClass(f);
    if (base != declClass) {
      throw new RuntimeException("Declaring class not equal: " + base + " vs " + declClass);
    }
    System.out.println(declClass);

    int modifiers = getFieldModifiers(f);
    if (modifiers != f.getModifiers()) {
      throw new RuntimeException("Modifiers not equal: " + f.getModifiers() + " vs " + modifiers);
    }
    System.out.println(modifiers);

    boolean synth = isFieldSynthetic(f);
    if (synth != f.isSynthetic()) {
      throw new RuntimeException("Synthetic not equal: " + f.isSynthetic() + " vs " + synth);
    }
    System.out.println(synth);
  }

  private static native String[] getFieldName(Field f);
  private static native Class<?> getFieldDeclaringClass(Field f);
  private static native int getFieldModifiers(Field f);
  private static native boolean isFieldSynthetic(Field f);

  private class Foo {
  }

  private static interface Bar {
    public static int VAL = 1;
  }

  private static class Generics<T> {
    T generics;
  }
}
