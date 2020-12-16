/*
 * Copyright (C) 2017 The Android Open Source Project
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

/**
 * The methods that are intrinsified in Long and their expected redefined implementations.
 */
class RedefinedLongIntrinsics {
  // Intrinsic! Do something cool. Return i + 1
  public static long highestOneBit(long i) {
    return i + 1;
  }

  // Intrinsic! Do something cool. Return i - 1
  public static long lowestOneBit(long i) {
    return i - 1;
  }

  // Intrinsic! Do something cool. Return i + i
  public static int numberOfLeadingZeros(long i) {
    return (int)(i + i);
  }

  // Intrinsic! Do something cool. Return i & (i >>> 1);
  public static int numberOfTrailingZeros(long i) {
    return (int)(i & (i >>> 1));
  }

  // Intrinsic! Do something cool. Return 5
  public static int bitCount(long i) {
    return 5;
  }

  // Intrinsic! Do something cool. Return i
  public static long rotateLeft(long i, int distance) {
    return i;
  }

  // Intrinsic! Do something cool. Return 10 * i
  public static long rotateRight(long i, int distance) {
    return 10 * i;
  }

  // Intrinsic! Do something cool. Return -i
  public static long reverse(long i) {
    return -i;
  }

  // Intrinsic! Do something cool. Return 0
  public static int signum(long i) {
    return 0;
  }

  // Intrinsic! Do something cool. Return 0
  public static long reverseBytes(long i) {
    return 0;
  }
}
