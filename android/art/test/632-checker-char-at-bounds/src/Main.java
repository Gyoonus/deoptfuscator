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

  /// CHECK-START: void Main.main(java.lang.String[]) licm (after)
  /// CHECK-DAG: <<NullCheck:l\d+>>   NullCheck
  /// CHECK-DAG: <<BoundsCheck:i\d+>> BoundsCheck
  /// CHECK-DAG:                      ArrayGet [<<NullCheck>>,<<BoundsCheck>>]
  public static void main(String[] args) {
    try {
      String foo = myString;
      foo.getClass(); // Make sure the null check is not in the loop.
      char c = 0;
      for (int i = 0; i < 10; i++) {
        // The charAt may be licm'ed, but it has to be licm'ed with its
        // bounds check.
        c = foo.charAt(10000000);
      }
      System.out.println(c);
    } catch (StringIndexOutOfBoundsException e) {
      // Expected
    }
  }

  static String myString = "foo";
}
