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
  static volatile long long_volatile;
  static volatile double double_volatile;

  public static void main(String[] args) {
    checkVolatileUpdate(0L);
    checkVolatileUpdate(Long.MAX_VALUE);
    checkVolatileUpdate(Long.MIN_VALUE);

    checkVolatileUpdate(0.0);
    checkVolatileUpdate(Double.MAX_VALUE);
    checkVolatileUpdate(-Double.MAX_VALUE);
  }

  public static long $opt$update(long a) {
     long_volatile = a;
     return long_volatile;
  }

  public static double $opt$update(double a) {
     double_volatile = a;
     return double_volatile;
  }

  public static void checkVolatileUpdate(long value) {
    if (value != $opt$update(value)) {
      throw new RuntimeException("Volatile update failed for long:" + value);
    }
  }

  public static void checkVolatileUpdate(double value) {
    if (value != $opt$update(value)) {
      throw new RuntimeException("Volatile update failed for double:" + value);
    }
  }

}
