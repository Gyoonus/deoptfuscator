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

public class EncodedAnnotation implements RawDexObject {
  public int typeIdx;
  public int size;
  public AnnotationElement[] elements;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    typeIdx = file.readUleb128();
    size = file.readUleb128();
    if (size != 0) {
      elements = new AnnotationElement[size];
      for (int i = 0; i < size; i++) {
        (elements[i] = new AnnotationElement()).read(file);
      }
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.writeUleb128(typeIdx);
    file.writeUleb128(size);
    if (size != 0) {
      for (AnnotationElement annotationElement : elements) {
        annotationElement.write(file);
      }
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (kind == IndexUpdateKind.TYPE_ID && typeIdx >= insertedIdx) {
      typeIdx++;
    }
    if (size != 0) {
      for (AnnotationElement element : elements) {
        element.incrementIndex(kind, insertedIdx);
      }
    }
  }

}
