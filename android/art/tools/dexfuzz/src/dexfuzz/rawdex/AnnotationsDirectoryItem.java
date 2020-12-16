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

public class AnnotationsDirectoryItem implements RawDexObject {
  public Offset classAnnotationsOff;
  public int fieldsSize;
  public int annotatedMethodsSize;
  public int annotatedParametersSize;
  public FieldAnnotation[] fieldAnnotations;
  public MethodAnnotation[] methodAnnotations;
  public ParameterAnnotation[] parameterAnnotations;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().getNewOffsettable(file, this);
    classAnnotationsOff = file.getOffsetTracker().getNewOffset(file.readUInt());
    fieldsSize = file.readUInt();
    annotatedMethodsSize = file.readUInt();
    annotatedParametersSize = file.readUInt();
    if (fieldsSize != 0) {
      fieldAnnotations = new FieldAnnotation[fieldsSize];
      for (int i = 0; i < fieldsSize; i++) {
        (fieldAnnotations[i] = new FieldAnnotation()).read(file);
      }
    }
    if (annotatedMethodsSize != 0) {
      methodAnnotations = new MethodAnnotation[annotatedMethodsSize];
      for (int i = 0; i < annotatedMethodsSize; i++) {
        (methodAnnotations[i] = new MethodAnnotation()).read(file);
      }
    }
    if (annotatedParametersSize != 0) {
      parameterAnnotations = new ParameterAnnotation[annotatedParametersSize];
      for (int i = 0; i < annotatedParametersSize; i++) {
        (parameterAnnotations[i] = new ParameterAnnotation()).read(file);
      }
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.getOffsetTracker().tryToWriteOffset(classAnnotationsOff, file, false /* ULEB128 */);
    file.writeUInt(fieldsSize);
    file.writeUInt(annotatedMethodsSize);
    file.writeUInt(annotatedParametersSize);
    if (fieldAnnotations != null) {
      for (FieldAnnotation fieldAnnotation : fieldAnnotations) {
        fieldAnnotation.write(file);
      }
    }
    if (methodAnnotations != null) {
      for (MethodAnnotation methodAnnotation : methodAnnotations) {
        methodAnnotation.write(file);
      }
    }
    if (parameterAnnotations != null) {
      for (ParameterAnnotation parameterAnnotation : parameterAnnotations) {
        parameterAnnotation.write(file);
      }
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (fieldAnnotations != null) {
      for (FieldAnnotation fieldAnnotation : fieldAnnotations) {
        fieldAnnotation.incrementIndex(kind, insertedIdx);
      }
    }
    if (methodAnnotations != null) {
      for (MethodAnnotation methodAnnotation : methodAnnotations) {
        methodAnnotation.incrementIndex(kind, insertedIdx);
      }
    }
    if (parameterAnnotations != null) {
      for (ParameterAnnotation parameterAnnotation : parameterAnnotations) {
        parameterAnnotation.incrementIndex(kind, insertedIdx);
      }
    }
  }
}
