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
    switch (staticField) {
      case -1:
        return;
      case -4:
        // We add this case to make it an odd number of case/default.
        // The code generation for it used to be bogus.
        throw new Error("Cannot happen");
      default:
        throw new Error("Cannot happen");
    }
  }
  static int staticField = -1;
}
