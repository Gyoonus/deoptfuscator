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

package dexfuzz.rawdex;

import dexfuzz.Log;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;

/**
 * An extension to RandomAccessFile that allows reading/writing
 * DEX files in little-endian form, the variable-length LEB format
 * and also provides word-alignment functions.
 */
public class DexRandomAccessFile extends RandomAccessFile {
  private OffsetTracker offsetTracker;

  public OffsetTracker getOffsetTracker() {
    return offsetTracker;
  }

  public void setOffsetTracker(OffsetTracker offsetTracker) {
    this.offsetTracker = offsetTracker;
  }

  /**
   * Constructor, passes straight on to RandomAccessFile currently.
   * @param filename The file to open.
   * @param mode Strings "r" or "rw" work best.
   */
  public DexRandomAccessFile(String filename, String mode)
      throws FileNotFoundException {
    super(filename, mode);
  }

  /**
   * @return A 16-bit number, read from the file as little-endian.
   */
  public short readUShort() throws IOException {
    int b1 = readUnsignedByte();
    int b2 = readUnsignedByte();
    return (short) ((b2 << 8) | b1);
  }

  /**
   * @param value A 16-bit number to be written to the file in little-endian.
   */
  public void writeUShort(short value) throws IOException {
    int b1 = value & 0xff;
    int b2 = (value & 0xff00) >> 8;
    writeByte(b1);
    writeByte(b2);
  }

  /**
   * @return A 32-bit number, read from the file as little-endian.
   */
  public int readUInt() throws IOException {
    int b1 = readUnsignedByte();
    int b2 = readUnsignedByte();
    int b3 = readUnsignedByte();
    int b4 = readUnsignedByte();
    return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
  }

  /**
   * @param value A 32-bit number to be written to the file in little-endian.
   */
  public void writeUInt(int value) throws IOException {
    int b1 = value & 0xff;
    writeByte(b1);
    int b2 = (value & 0xff00) >> 8;
    writeByte(b2);
    int b3 = (value & 0xff0000) >> 16;
    writeByte(b3);
    int b4 = (value & 0xff000000) >> 24;
    writeByte(b4);
  }

  /**
   * @return An up to 32-bit number, read from the file in ULEB128 form.
   */
  public int readUleb128() throws IOException {
    int shift = 0;
    int value = 0;
    int rawByte = readUnsignedByte();
    boolean done = false;
    while (!done) {
      // Get the lower seven bits.
      // 0x7f = 0111 1111
      value |= ((rawByte & 0x7f) << shift);
      shift += 7;
      // Check the 8th bit - if it's 0, we're done.
      // 0x80 = 1000 0000
      if ((rawByte & 0x80) == 0) {
        done = true;
      } else {
        rawByte = readUnsignedByte();
      }
    }
    return value;
  }

  /**
   * @param value A 32-bit number to be written to the file in ULEB128 form.
   */
  public void writeUleb128(int value) throws IOException {
    if (value == 0) {
      writeByte(0);
      return;
    }

    while (value != 0) {
      int marker = 1;
      // If we're down to the last 7 bits, the marker will be 0.
      if ((value & 0xffffff80) == 0) {
        marker = 0;
      }
      // Get the lowest 7 bits, add on the marker in the high bit.
      int nextByte = value & 0x7f | (marker << 7);
      writeByte(nextByte);
      value >>>= 7;
    }
  }

  /**
   * Write out ULEB128 value always using 5 bytes.
   * A version of ULEB128 that will always write out 5 bytes, because this
   * value will be patched later, and if we used a smaller encoding, the new value
   * may overflow the previously selected encoding size.
   * The largest encoding for 0 in ULEB128 would be:
   *   0x80 0x80 0x80 0x80 0x00
   * and for 1 would be:
   *   0x81 0x80 0x80 0x80 0x00
   */
  public void writeLargestUleb128(int value) throws IOException {
    Log.debug("Writing " + value + " using the largest possible ULEB128 encoding.");
    if (value == 0) {
      writeByte(0x80);
      writeByte(0x80);
      writeByte(0x80);
      writeByte(0x80);
      writeByte(0x0);
      return;
    }

    for (int i = 0; i < 5; i++) {
      int marker = 1;
      // If we're writing the 5th byte, the marker is 0.
      if (i == 4) {
        marker = 0;
      }
      // Get the lowest 7 bits, add on the marker in the high bit.
      int nextByte = value & 0x7f | (marker << 7);
      writeByte(nextByte);
      value >>>= 7;
    }
  }

  /**
   * @return An up to 32-bit number, read from the file in SLEB128 form.
   */
  public int readSleb128() throws IOException {
    int shift = 0;
    int value = 0;
    int rawByte = readUnsignedByte();
    boolean done = false;
    boolean mustSignExtend = false;
    while (!done) {
      // Get the lower seven bits.
      // 0x7f = 0111 1111
      value |= ((rawByte & 0x7f) << shift);
      shift += 7;
      // Check the 8th bit - if it's 0, we're done.
      // 0x80 = 1000 0000
      if ((rawByte & 0x80) == 0) {
        // Check the 7th bit - if it's a 1, we need to sign extend.
        if ((rawByte & 0x60) != 0) {
          mustSignExtend = true;
        }
        done = true;
      } else {
        rawByte = readUnsignedByte();
      }
    }
    if (mustSignExtend) {
      // Example:
      // say we shifted 7 bits, we need
      // to make all the upper 25 bits 1s.
      // load a 1...
      // 00000000 00000000 00000000 00000001
      // << 7
      // 00000000 00000000 00000000 10000000
      // - 1
      // 00000000 00000000 00000000 01111111
      // ~
      // 11111111 11111111 11111111 10000000
      int upperOnes = ~((1 << shift) - 1);
      value |= (upperOnes);
    }
    return value;
  }

  /**
   * @param value A 32-bit number to be written to the file in SLEB128 form.
   */
  public void writeSleb128(int value) throws IOException {
    if (value == 0) {
      writeByte(0);
      return;
    }
    if (value > 0) {
      writeUleb128(value);
      return;
    }
    if (value == -1) {
      writeByte(0x7f);
    }

    // When it's all 1s (0xffffffff), we're done!
    while (value != 0xffffffff) {
      int marker = 1;
      // If we're down to the last 7 bits (i.e., shifting a further 7 is all 1s),
      // the marker will be 0.
      if ((value >> 7) == 0xffffffff) {
        marker = 0;
      }
      // Get the lowest 7 bits, add on the marker in the high bit.
      int nextByte = value & 0x7f | (marker << 7);
      writeByte(nextByte);
      value >>= 7;
    }
  }

  /**
   * In DEX format, strings are in MUTF-8 format, the first ULEB128 value is the decoded size
   * (i.e., string.length), and then follows a null-terminated series of characters.
   * @param decodedSize The ULEB128 value that should have been read just before this.
   * @return The raw bytes of the string, not including the null character.
   */
  public byte[] readDexUtf(int decodedSize) throws IOException {
    // In the dex MUTF-8, the encoded size can never be larger than 3 times
    // the actual string's length (which is the ULEB128 value just before this
    // string, the "decoded size")

    // Therefore, allocate as much space as we might need.
    byte[] str = new byte[decodedSize * 3];

    // Get our first byte.
    int encodedSize = 0;
    byte rawByte = readByte();

    // Keep reading until we find the end marker.
    while (rawByte != 0) {
      str[encodedSize++] = rawByte;
      rawByte = readByte();
    }

    // Copy everything we read into str into the correctly-sized actual string.
    byte[] actualString = new byte[encodedSize];
    for (int i = 0; i < encodedSize; i++) {
      actualString[i] = str[i];
    }

    return actualString;
  }

  /**
   * Writes out raw bytes that would have been read by readDexUTF().
   * Will automatically write out the null-byte at the end.
   * @param data Bytes to be written out.
   */
  public void writeDexUtf(byte[] data) throws IOException {
    write(data);
    // Remember to add the end marker.
    writeByte(0);
  }

  /**
   * Align the file handle's seek pointer to the next N bytes.
   * @param alignment N to align to.
   */
  public void alignForwards(int alignment) throws IOException {
    long offset = getFilePointer();
    long mask = alignment - 1;
    if ((offset & mask) != 0) {
      int extra = alignment - (int) (offset & mask);
      seek(offset + extra);
    }
  }

  /**
   * Align the file handle's seek pointer backwards to the previous N bytes.
   * @param alignment N to align to.
   */
  public void alignBackwards(int alignment) throws IOException {
    long offset = getFilePointer();
    long mask = alignment - 1;
    if ((offset & mask) != 0) {
      offset &= (~mask);
      seek(offset);
    }
  }
}
