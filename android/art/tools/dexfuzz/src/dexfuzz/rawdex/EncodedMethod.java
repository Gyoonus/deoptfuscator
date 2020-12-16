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

public class EncodedMethod implements RawDexObject {
  public int methodIdxDiff;
  public int accessFlags;
  public Offset codeOff;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    methodIdxDiff = file.readUleb128();
    accessFlags = file.readUleb128();
    codeOff = file.getOffsetTracker().getNewOffset(file.readUleb128());
    if (isNative()) {
      Log.errorAndQuit("Sorry, DEX files with native methods are not supported yet.");
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.writeUleb128(methodIdxDiff);
    file.writeUleb128(accessFlags);
    file.getOffsetTracker().tryToWriteOffset(codeOff, file, true /* ULEB128 */);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
    // NB: our idx_diff is handled in ClassDataItem...
  }

  public boolean isStatic() {
    return ((accessFlags & Flag.ACC_STATIC.getValue()) != 0);
  }

  public boolean isNative() {
    return ((accessFlags & Flag.ACC_NATIVE.getValue()) != 0);
  }

  /**
   * Set/unset the static flag for this EncodedMethod.
   */
  public void setStatic(boolean turnOn) {
    if (turnOn) {
      accessFlags |= Flag.ACC_STATIC.getValue();
    } else {
      accessFlags &= ~(Flag.ACC_STATIC.getValue());
    }
  }

  private static enum Flag {
    ACC_PUBLIC(0x1),
    ACC_PRIVATE(0x2),
    ACC_PROTECTED(0x4),
    ACC_STATIC(0x8),
    ACC_FINAL(0x10),
    ACC_SYNCHRONIZED(0x20),
    ACC_VARARGS(0x80),
    ACC_NATIVE(0x100),
    ACC_ABSTRACT(0x400),
    ACC_STRICT(0x800),
    ACC_SYNTHETIC(0x1000),
    ACC_ENUM(0x4000),
    ACC_CONSTRUCTOR(0x10000);

    private int value;

    private Flag(int value) {
      this.value = value;
    }

    public int getValue() {
      return value;
    }
  }
}
