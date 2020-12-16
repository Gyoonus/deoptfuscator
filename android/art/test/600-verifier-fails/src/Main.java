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

public class Main {

  public static final String staticFinalField = null;

  private static String staticPrivateField = null;

  private int privateField = 0;

  private void privateMethod() { }

  private static void test(String name) throws Exception {
    try {
      Class<?> a = Class.forName(name);
      a.newInstance();
    } catch (java.lang.LinkageError e) {
      System.out.println("passed " + name);
    }
  }

  public static void main(String[] args) throws Exception {
    test("A");
    test("B");
    test("C");
    test("D");
    test("E");
    test("F");
  }
}
