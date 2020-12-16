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

public class MethodIdItem implements RawDexObject {
  public short classIdx;
  public short protoIdx;
  public int nameIdx;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    classIdx = file.readUShort();
    protoIdx = file.readUShort();
    nameIdx = file.readUInt();
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUShort(classIdx);
    file.writeUShort(protoIdx);
    file.writeUInt(nameIdx);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (kind == IndexUpdateKind.TYPE_ID && classIdx >= insertedIdx) {
      classIdx++;
    }
    if (kind == IndexUpdateKind.PROTO_ID && protoIdx >= insertedIdx) {
      protoIdx++;
    }
    if (kind == IndexUpdateKind.STRING_ID && nameIdx >= insertedIdx) {
      nameIdx++;
    }
  }
}
