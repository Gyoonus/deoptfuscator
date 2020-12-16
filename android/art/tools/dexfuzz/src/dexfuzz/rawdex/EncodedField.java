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

public class EncodedField implements RawDexObject {
  public int fieldIdxDiff;
  public int accessFlags;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    fieldIdxDiff = file.readUleb128();
    accessFlags = file.readUleb128();
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.writeUleb128(fieldIdxDiff);
    file.writeUleb128(accessFlags);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
    // NB: our idx_diff is handled in ClassDataItem...
  }

  public boolean isVolatile() {
    return ((accessFlags & Flag.ACC_VOLATILE.getValue()) != 0);
  }

  /**
   * Set/unset the volatile flag for this EncodedField.
   */
  public void setVolatile(boolean turnOn) {
    if (turnOn) {
      accessFlags |= Flag.ACC_VOLATILE.getValue();
    } else {
      accessFlags &= ~(Flag.ACC_VOLATILE.getValue());
    }
  }

  private static enum Flag {
    ACC_PUBLIC(0x1),
    ACC_PRIVATE(0x2),
    ACC_PROTECTED(0x4),
    ACC_STATIC(0x8),
    ACC_FINAL(0x10),
    ACC_VOLATILE(0x40),
    ACC_TRANSIENT(0x80),
    ACC_SYNTHETIC(0x1000),
    ACC_ENUM(0x4000);

    private int value;

    private Flag(int value) {
      this.value = value;
    }

    public int getValue() {
      return value;
    }
  }
}
