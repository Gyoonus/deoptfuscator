/*
 * Copyright (C) 2015 The Android Open Source Project
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

class Main {
  public static void main(String[] args) {
    ssaBuilderDouble(new double[] { 1.2, 4.3, 5.2 });
    ssaBuilderDouble(new double[] { 1.2, 4.3, 5.2, 6.8 });
  }

  public static void ssaBuilderDouble(double[] array) {
    double x;
    if (array.length > 3) {
      x = array[0];
    } else {
      x = array[1];
    }
    array[2] = x;
    System.out.println(x);
  }
}
