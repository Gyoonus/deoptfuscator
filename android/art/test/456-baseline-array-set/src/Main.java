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

public class Main {
  public static void main(String[] args) {
    doArrayAccess(new Integer(1), 0);
  }

  public static void doArrayAccess(Integer value, int index) {
    try {
      Integer[] array = new Integer[2];
      // If we were to do optimization on the baseline register
      // allocator, generating code for the array set would fail on x86.
      array[index] = array[index + 1];
      array[index] = value;
    } catch (ArrayStoreException e) {
      throw e;
    }
  }
}
