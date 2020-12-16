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

public class ProtoIdItem implements RawDexObject {
  public int shortyIdx;
  public int returnTypeIdx;
  public Offset parametersOff;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    shortyIdx = file.readUInt();
    returnTypeIdx = file.readUInt();
    parametersOff = file.getOffsetTracker().getNewOffset(file.readUInt());
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUInt(shortyIdx);
    file.writeUInt(returnTypeIdx);
    file.getOffsetTracker().tryToWriteOffset(parametersOff, file, false /* ULEB128 */);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (kind == IndexUpdateKind.STRING_ID && shortyIdx >= insertedIdx) {
      shortyIdx++;
    }
    if (kind == IndexUpdateKind.TYPE_ID && returnTypeIdx >= insertedIdx) {
      returnTypeIdx++;
    }
  }
}
