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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

/** Class with helper methods for field and method lookups using java.lang.invoke. */
public class JLI {
  public static boolean canDiscoverWithLookupFindGetter(
      MethodHandles.Lookup lookup, Class<?> klass, String fieldName, Class<?> fieldType) {
    try {
      return lookup.findGetter(klass, fieldName, fieldType) != null;
    } catch (NoSuchFieldException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }

  public static boolean canDiscoverWithLookupFindSetter(
      MethodHandles.Lookup lookup, Class<?> klass, String fieldName, Class<?> fieldType) {
    try {
      return lookup.findSetter(klass, fieldName, fieldType) != null;
    } catch (NoSuchFieldException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }

  public static boolean canDiscoverWithLookupFindStaticGetter(
      MethodHandles.Lookup lookup, Class<?> klass, String fieldName, Class<?> fieldType) {
    try {
      return lookup.findStaticGetter(klass, fieldName, fieldType) != null;
    } catch (NoSuchFieldException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }

  public static boolean canDiscoverWithLookupFindStaticSetter(
      MethodHandles.Lookup lookup, Class<?> klass, String fieldName, Class<?> fieldType) {
    try {
      return lookup.findStaticSetter(klass, fieldName, fieldType) != null;
    } catch (NoSuchFieldException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }

  public static boolean canDiscoverWithLookupFindConstructor(
      MethodHandles.Lookup lookup, Class<?> klass, MethodType methodType) {
    try {
      return lookup.findConstructor(klass, methodType) != null;
    } catch (NoSuchMethodException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }

  public static boolean canDiscoverWithLookupFindVirtual(
      MethodHandles.Lookup lookup, Class<?> klass, String methodName, MethodType methodType) {
    try {
      return lookup.findVirtual(klass, methodName, methodType) != null;
    } catch (NoSuchMethodException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }

  public static boolean canDiscoverWithLookupFindStatic(
      MethodHandles.Lookup lookup, Class<?> klass, String methodName, MethodType methodType) {
    try {
      return lookup.findStatic(klass, methodName, methodType) != null;
    } catch (NoSuchMethodException e) {
      return false;
    } catch (IllegalAccessException e) {
      return true;
    }
  }
}
