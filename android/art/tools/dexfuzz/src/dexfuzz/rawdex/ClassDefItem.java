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

public class ClassDefItem implements RawDexObject {
  public static int data_size = 0x20;

  public int classIdx;
  public int accessFlags;
  public int superclassIdx;
  public Offset interfacesOff;
  public int sourceFileIdx;
  public Offset annotationsOff;
  public Offset classDataOff;
  public Offset staticValuesOff;

  public static class MetaInfo {
    public ClassDataItem classDataItem;
  }

  public MetaInfo meta = new MetaInfo();

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    classIdx = file.readUInt();
    accessFlags = file.readUInt();
    superclassIdx = file.readUInt();
    interfacesOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    sourceFileIdx = file.readUInt();
    annotationsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    classDataOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    staticValuesOff = file.getOffsetTracker().getNewOffset(file.readUInt());
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUInt(classIdx);
    file.writeUInt(accessFlags);
    file.writeUInt(superclassIdx);
    file.getOffsetTracker().tryToWriteOffset(interfacesOff, file, false /* ULEB128 */);
    file.writeUInt(sourceFileIdx);
    file.getOffsetTracker().tryToWriteOffset(annotationsOff, file, false /* ULEB128 */);
    file.getOffsetTracker().tryToWriteOffset(classDataOff, file, false /* ULEB128 */);
    file.getOffsetTracker().tryToWriteOffset(staticValuesOff, file, false /* ULEB128 */);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (kind == IndexUpdateKind.TYPE_ID && classIdx >= insertedIdx) {
      classIdx++;
    }
    if (kind == IndexUpdateKind.TYPE_ID && superclassIdx >= insertedIdx) {
      superclassIdx++;
    }
    if (kind == IndexUpdateKind.STRING_ID && sourceFileIdx >= insertedIdx) {
      sourceFileIdx++;
    }
  }
}
