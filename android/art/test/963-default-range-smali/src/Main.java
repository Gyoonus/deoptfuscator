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
    A a = new A();
    System.out.println(a.SayHi("a string 0",
                               "a string 1",
                               "a string 2",
                               "a string 3",
                               "a string 4",
                               "a string 5",
                               "a string 6",
                               "a string 7",
                               "a string 8",
                               "a string 9"));
    Iface b = a;
    System.out.println(b.SayHi("a string 0",
                               "a string 1",
                               "a string 2",
                               "a string 3",
                               "a string 4",
                               "a string 5",
                               "a string 6",
                               "a string 7",
                               "a string 8",
                               "a string 9"));
  }
}
