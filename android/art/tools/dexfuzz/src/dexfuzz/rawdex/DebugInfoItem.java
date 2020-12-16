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

// Right now we are not parsing debug_info_item, just take the raw size
public class DebugInfoItem implements RawDexObject {
  private int size;
  private byte[] data;

  public DebugInfoItem(int size) {
    this.size = size;
  }

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    data = new byte[size];
    file.read(data);

    // Since we are not parsing the section, ensure that the last byte is DBG_END_SEQUENCE.
    if (data[size - 1] != 0) {
      Log.errorAndQuit("Error reading debug_info_item. The last byte is not DBG_END_SEQUENCE.");
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.write(data);
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
  }
}
