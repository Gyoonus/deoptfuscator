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

public class HeaderItem implements RawDexObject {
  public byte[] magic;
  public int checksum;
  public byte[] signature; // Verification doesn't depend on this, so we don't update it.
  public int fileSize;
  public int headerSize;
  public int endianTag;
  public int linkSize;
  public Offset linkOff;
  public Offset mapOff;
  public int stringIdsSize;
  public Offset stringIdsOff;
  public int typeIdsSize;
  public Offset typeIdsOff;
  public int protoIdsSize;
  public Offset protoIdsOff;
  public int fieldIdsSize;
  public Offset fieldIdsOff;
  public int methodIdsSize;
  public Offset methodIdsOff;
  public int classDefsSize;
  public Offset classDefsOff;
  public int dataSize;
  public Offset dataOff;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    magic = new byte[8];
    for (int i = 0; i < 8; i++) {
      magic[i] = file.readByte();
    }
    checksum = file.readUInt();
    signature = new byte[20];
    for (int i = 0; i < 20; i++) {
      signature[i] = file.readByte();
    }
    fileSize = file.readUInt();
    headerSize = file.readUInt();
    endianTag = file.readUInt();
    linkSize = file.readUInt();
    linkOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    mapOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    stringIdsSize = file.readUInt();
    stringIdsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    typeIdsSize = file.readUInt();
    typeIdsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    protoIdsSize = file.readUInt();
    protoIdsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    fieldIdsSize = file.readUInt();
    fieldIdsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    methodIdsSize = file.readUInt();
    methodIdsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    classDefsSize = file.readUInt();
    classDefsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    dataSize = file.readUInt();
    dataOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    if (headerSize != 0x70) {
      Log.errorAndQuit("Invalid header size in header.");
    }
    if (file.getFilePointer() != headerSize) {
      Log.errorAndQuit("Read a different amount than expected in header: "
          + file.getFilePointer());
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    for (int i = 0; i < 8; i++) {
      file.writeByte(magic[i]);
    }
    // Will be recalculated later!
    file.writeUInt(checksum);
    for (int i = 0; i < 20; i++) {
      file.writeByte(signature[i]);
    }
    // Will be recalculated later!
    file.writeUInt(fileSize);
    file.writeUInt(headerSize);
    file.writeUInt(endianTag);
    file.writeUInt(linkSize);
    file.getOffsetTracker().tryToWriteOffset(linkOff, file, false /* ULEB128 */);
    file.getOffsetTracker().tryToWriteOffset(mapOff, file, false /* ULEB128 */);
    file.writeUInt(stringIdsSize);
    file.getOffsetTracker().tryToWriteOffset(stringIdsOff, file, false /* ULEB128 */);
    file.writeUInt(typeIdsSize);
    file.getOffsetTracker().tryToWriteOffset(typeIdsOff, file, false /* ULEB128 */);
    file.writeUInt(protoIdsSize);
    file.getOffsetTracker().tryToWriteOffset(protoIdsOff, file, false /* ULEB128 */);
    file.writeUInt(fieldIdsSize);
    file.getOffsetTracker().tryToWriteOffset(fieldIdsOff, file, false /* ULEB128 */);
    file.writeUInt(methodIdsSize);
    file.getOffsetTracker().tryToWriteOffset(methodIdsOff, file, false /* ULEB128 */);
    file.writeUInt(classDefsSize);
    file.getOffsetTracker().tryToWriteOffset(classDefsOff, file, false /* ULEB128 */);
    // will be recalculated later
    file.writeUInt(dataSize);
    file.getOffsetTracker().tryToWriteOffset(dataOff, file, false /* ULEB128 */);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing
  }
}
