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

public class AnnotationSetItem implements RawDexObject {
  public int size;
  public AnnotationOffItem[] entries;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().getNewOffsettable(file, this);
    size = file.readUInt();
    entries = new AnnotationOffItem[size];
    for (int i = 0; i < size; i++) {
      (entries[i] = new AnnotationOffItem()).read(file);
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUInt(size);
    for (AnnotationOffItem annotationOffItem : entries) {
      annotationOffItem.write(file);
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
  }
}
