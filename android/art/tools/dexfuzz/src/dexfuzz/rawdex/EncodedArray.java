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

public class EncodedArray implements RawDexObject {
  public int size;
  public EncodedValue[] values;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    size = file.readUleb128();
    if (size != 0) {
      values = new EncodedValue[size];
      for (int i = 0; i < size; i++) {
        (values[i] = new EncodedValue()).read(file);
      }
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.writeUleb128(size);
    if (size != 0) {
      for (EncodedValue encodedValue : values) {
        encodedValue.write(file);
      }
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (size != 0) {
      for (EncodedValue value : values) {
        value.incrementIndex(kind, insertedIdx);
      }
    }
  }
}
