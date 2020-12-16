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
    new Main().bar();
  }

  public void bar() {
    // Use up all available D registers on ARM.
    baz(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);
  }

  public static void baz(float a, float b, float c, float d, float e, float f, float g,
                         float h, float i, float j, float k, float l, float m, float n, float o) {
    System.out.println(a - b - c - d - e - f - g - h - i - j - k - l - m - n - o);
  }

  float a = 1.0f;
  float b = 2.0f;
  float c = 3.0f;
  float d = 4.0f;
  float e = 5.0f;
  float f = 6.0f;
  float g = 7.0f;
  float h = 8.0f;
  float i = 9.0f;
  float j = 10.0f;
  float k = 11.0f;
  float l = 12.0f;
  float m = 13.0f;
  float n = 14.0f;
  float o = 15.0f;
}
