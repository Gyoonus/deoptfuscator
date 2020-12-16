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

package dexfuzz.rawdex.formats;

/**
 * Class consisting of static methods used for common read/write operations
 * perfomed in the Format classes.
 */
public class RawInsnHelper {
  /**
   * Read a signed byte from the idx into the raw array.
   */
  public static long getSignedByteFromByte(byte[] raw, int idx) {
    return (long) raw[idx];
  }

  /**
   * Read an unsigned byte from the idx into the raw array.
   */
  public static long getUnsignedByteFromByte(byte[] raw, int idx) {
    return ((long) raw[idx]) & 0xff;
  }

  /**
   * Read an unsigned lower 4 bits from the idx into the raw array.
   */
  public static long getUnsignedLowNibbleFromByte(byte[] raw, int idx) {
    return ((long) raw[idx]) & 0xf;
  }

  /**
   * Read an unsigned higher 4 bits from the idx into the raw array.
   */
  public static long getUnsignedHighNibbleFromByte(byte[] raw, int idx) {
    return (((long) raw[idx]) >> 4) & 0xf;
  }

  /**
   * Read an unsigned 2 bytes as a short from the idx into the raw array.
   */
  public static long getUnsignedShortFromTwoBytes(byte[] raw, int idx) {
    return (long) ( (((long) raw[idx]) & 0xff)
        | ((((long) raw[idx + 1]) & 0xff) << 8));
  }

  /**
   * Read a signed 2 bytes as a short from the idx into the raw array.
   */
  public static long getSignedShortFromTwoBytes(byte[] raw, int idx) {
    return (long) ( (((long) raw[idx]) & 0xff)
        | (((long) raw[idx + 1]) << 8));
  }

  /**
   * Read an unsigned 4 bytes as an int from the idx into the raw array.
   */
  public static long getUnsignedIntFromFourBytes(byte[] raw, int idx) {
    return (long) ( (((long) raw[idx]) & 0xff)
        | ((((long) raw[idx + 1]) & 0xff) << 8)
        | ((((long) raw[idx + 2]) & 0xff) << 16)
        | ((((long) raw[idx + 3]) & 0xff) << 24) );
  }

  /**
   * Read a signed 4 bytes as an int from the idx into the raw array.
   */
  public static long getSignedIntFromFourBytes(byte[] raw, int idx) {
    return (long) ( (((long) raw[idx]) & 0xff)
        | ((((long) raw[idx + 1]) & 0xff) << 8)
        | ((((long) raw[idx + 2]) & 0xff) << 16)
        | (((long) raw[idx + 3]) << 24) );
  }

  /**
   * Read a signed 8 bytes as a long from the idx into the raw array.
   */
  public static long getSignedLongFromEightBytes(byte[] raw, int idx) {
    return (long) ( (((long) raw[idx]) & 0xff)
        | ((((long) raw[idx + 1]) & 0xff) << 8)
        | ((((long) raw[idx + 2]) & 0xff) << 16)
        | ((((long) raw[idx + 3]) & 0xff) << 24)
        | ((((long) raw[idx + 4]) & 0xff) << 32)
        | ((((long) raw[idx + 5]) & 0xff) << 40)
        | ((((long) raw[idx + 6]) & 0xff) << 48)
        | (((long) raw[idx + 7]) << 56) );
  }

  /**
   * Given an idx into a raw array, and an int, write that int into the array at that position.
   */
  public static void writeUnsignedIntToFourBytes(byte[] raw, int idx, int value) {
    raw[idx] = (byte) (value & 0xFF);
    raw[idx + 1] = (byte) ((value & 0xFF00) >>> 8);
    raw[idx + 2] = (byte) ((value & 0xFF0000) >>> 16);
    raw[idx + 3] = (byte) ((value & 0xFF000000) >>> 24);
  }

  /**
   * Given an idx into a raw array, and a short, write that int into the array at that position.
   */
  public static void writeUnsignedShortToTwoBytes(byte[] raw, int idx, int value) {
    raw[idx] = (byte) (value & 0xFF);
    raw[idx + 1] = (byte) ((value & 0xFF00) >>> 8);
  }
}
