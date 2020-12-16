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

public class ScopedPrimitiveArrayBenchmark {
  // Measure adds the first and last element of the array by using ScopedPrimitiveArray.
  static native long measureByteArray(int reps, byte[] arr);
  static native long measureShortArray(int reps, short[] arr);
  static native long measureIntArray(int reps, int[] arr);
  static native long measureLongArray(int reps, long[] arr);

  static final int smallLength = 16;
  static final int mediumLength = 256;
  static final int largeLength = 8096;
  static byte[] smallBytes = new byte[smallLength];
  static byte[] mediumBytes = new byte[mediumLength];
  static byte[] largeBytes = new byte[largeLength];
  static short[] smallShorts = new short[smallLength];
  static short[] mediumShorts = new short[mediumLength];
  static short[] largeShorts = new short[largeLength];
  static int[] smallInts = new int[smallLength];
  static int[] mediumInts = new int[mediumLength];
  static int[] largeInts = new int[largeLength];
  static long[] smallLongs = new long[smallLength];
  static long[] mediumLongs = new long[mediumLength];
  static long[] largeLongs = new long[largeLength];

  public void timeSmallBytes(int reps) {
    measureByteArray(reps, smallBytes);
  }

  public void timeMediumBytes(int reps) {
    measureByteArray(reps, mediumBytes);
  }

  public void timeLargeBytes(int reps) {
    measureByteArray(reps, largeBytes);
  }

  public void timeSmallShorts(int reps) {
    measureShortArray(reps, smallShorts);
  }

  public void timeMediumShorts(int reps) {
    measureShortArray(reps, mediumShorts);
  }

  public void timeLargeShorts(int reps) {
    measureShortArray(reps, largeShorts);
  }

  public void timeSmallInts(int reps) {
    measureIntArray(reps, smallInts);
  }

  public void timeMediumInts(int reps) {
    measureIntArray(reps, mediumInts);
  }

  public void timeLargeInts(int reps) {
    measureIntArray(reps, largeInts);
  }

  public void timeSmallLongs(int reps) {
    measureLongArray(reps, smallLongs);
  }

  public void timeMediumLongs(int reps) {
    measureLongArray(reps, mediumLongs);
  }

  public void timeLargeLongs(int reps) {
    measureLongArray(reps, largeLongs);
  }

  {
    System.loadLibrary("artbenchmark");
  }
}
