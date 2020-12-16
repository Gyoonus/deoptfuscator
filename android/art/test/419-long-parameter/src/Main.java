/*
 * Copyright (C) 2014 The Android Open Source Project
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
    if ($opt$TestCallee(1.0, 2.0, 1L, 2L) != 1L) {
      throw new Error("Unexpected result");
    }
    if ($opt$TestCaller() != 1L) {
      throw new Error("Unexpected result");
    }
  }

  public static long $opt$TestCallee(double a, double b, long c, long d) {
    return d - c;
  }

  public static long $opt$TestCaller() {
    return $opt$TestCallee(1.0, 2.0, 1L, 2L);
  }
}
