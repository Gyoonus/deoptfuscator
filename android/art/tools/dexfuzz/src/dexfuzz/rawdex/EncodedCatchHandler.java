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

public class EncodedCatchHandler implements RawDexObject {
  public int size;
  public EncodedTypeAddrPair[] handlers;
  public int catchAllAddr;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    size = file.readSleb128();
    int absoluteSize = Math.abs(size);
    if (absoluteSize > 0) {
      handlers = new EncodedTypeAddrPair[absoluteSize];
      for (int i = 0; i < Math.abs(size); i++) {
        (handlers[i] = new EncodedTypeAddrPair()).read(file);
      }
    }
    if (size <= 0) {
      catchAllAddr = file.readUleb128();
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.writeSleb128(size);
    if (handlers != null) {
      for (EncodedTypeAddrPair encodedTypeAddrPair : handlers) {
        encodedTypeAddrPair.write(file);
      }
    }
    if (size <= 0) {
      file.writeUleb128(catchAllAddr);
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (handlers != null) {
      for (EncodedTypeAddrPair handler : handlers) {
        handler.incrementIndex(kind, insertedIdx);
      }
    }
  }
}
