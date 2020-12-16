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

import java.io.IOException;

public class EncodedValue implements RawDexObject {
  public byte valueArg;
  public byte valueType;
  public byte[] value;
  public EncodedArray encodedArray;
  public EncodedAnnotation encodedAnnotation;

  private static final byte VALUE_BYTE = 0x00;
  private static final byte VALUE_ARRAY = 0x1c;
  private static final byte VALUE_ANNOTATION = 0x1d;
  private static final byte VALUE_NULL = 0x1e;
  private static final byte VALUE_BOOLEAN = 0x1f;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    int valueArgAndType = file.readUnsignedByte();

    // Get lower 5 bits.
    valueType = (byte) (valueArgAndType & 0x1f);
    // Get upper 3 bits.
    valueArg = (byte) ((valueArgAndType & 0xe0) >> 5);

    int size = 0;

    switch (valueType) {
      case VALUE_BYTE:
        size = 1;
        break;
      case VALUE_ARRAY:
        (encodedArray = new EncodedArray()).read(file);
        size = 0; // So we don't read into value.
        break;
      case VALUE_ANNOTATION:
        (encodedAnnotation = new EncodedAnnotation()).read(file);
        size = 0; // So we don't read into value.
        break;
      case VALUE_NULL:
      case VALUE_BOOLEAN:
        // No value
        size = 0;
        break;
      default:
        // All others encode value_arg as (size - 1), so...
        size = valueArg + 1;
        break;
    }

    if (size != 0) {
      value = new byte[size];
      for (int i = 0; i < size; i++) {
        value[i] = file.readByte();
      }
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    int valueArgAndType = ((valueType) | (valueArg << 5));
    file.writeByte(valueArgAndType);

    if (encodedArray != null) {
      encodedArray.write(file);
    } else if (encodedAnnotation != null) {
      encodedAnnotation.write(file);
    } else if (value != null) {
      file.write(value);
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (encodedArray != null) {
      encodedArray.incrementIndex(kind, insertedIdx);
    } else if (encodedAnnotation != null) {
      encodedAnnotation.incrementIndex(kind, insertedIdx);
    }
  }
}
