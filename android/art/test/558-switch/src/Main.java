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
   public static boolean testMethod(int statusCode) {
        switch (statusCode) {
        case 303:
        case 301:
        case 302:
        case 307:
            return true;
        default:
            return false;
        } //end of switch
    }

  public static void main(String[] args) {
    if (!testMethod(301)) {
      throw new Error("Unexpected result");
    }
  }
}
