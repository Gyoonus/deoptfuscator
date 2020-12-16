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

import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class StringDataItem implements RawDexObject {
  private int size;
  private String data;
  private byte[] dataAsBytes;
  private boolean writeRawBytes;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    size = file.readUleb128();
    if (size != 0) {
      dataAsBytes = file.readDexUtf(size);
      data = new String(dataAsBytes, StandardCharsets.US_ASCII);
      if (size != data.length()) {
        Log.warn("Don't have full support for decoding MUTF-8 yet, DEX file "
            + "may be incorrectly mutated. Avoid using this test case for now.");
        writeRawBytes = true;
      }
    } else {
      // Read past the null byte.
      file.readByte();
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUleb128(size);
    if (size > 0) {
      if (writeRawBytes) {
        file.writeDexUtf(dataAsBytes);
      } else {
        file.writeDexUtf(data.getBytes(StandardCharsets.US_ASCII));
      }
    } else {
      // Write out the null byte.
      file.writeByte(0);
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
  }

  public void setSize(int size) {
    this.size = size;
  }

  public int getSize() {
    return size;
  }

  public void setString(String data) {
    this.data = data;
  }

  public String getString() {
    if (writeRawBytes) {
      Log.warn("Reading a string that hasn't been properly decoded! Returning empty string.");
      return "";
    }
    return data;
  }
}
