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

public class ClassDataItem implements RawDexObject {
  public int staticFieldsSize;
  public int instanceFieldsSize;
  public int directMethodsSize;
  public int virtualMethodsSize;

  public EncodedField[] staticFields;
  public EncodedField[] instanceFields;
  public EncodedMethod[] directMethods;
  public EncodedMethod[] virtualMethods;

  public static class MetaInfo {
    public ClassDefItem classDefItem;
  }

  public MetaInfo meta = new MetaInfo();

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().getNewOffsettable(file, this);
    staticFieldsSize = file.readUleb128();
    instanceFieldsSize = file.readUleb128();
    directMethodsSize = file.readUleb128();
    virtualMethodsSize = file.readUleb128();

    staticFields = new EncodedField[staticFieldsSize];
    for (int i = 0; i < staticFieldsSize; i++) {
      (staticFields[i] = new EncodedField()).read(file);
    }
    instanceFields = new EncodedField[instanceFieldsSize];
    for (int i = 0; i < instanceFieldsSize; i++) {
      (instanceFields[i] = new EncodedField()).read(file);
    }
    directMethods = new EncodedMethod[directMethodsSize];
    for (int i = 0; i < directMethodsSize; i++) {
      (directMethods[i] = new EncodedMethod()).read(file);
    }
    virtualMethods = new EncodedMethod[virtualMethodsSize];
    for (int i = 0; i < virtualMethodsSize; i++) {
      (virtualMethods[i] = new EncodedMethod()).read(file);
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUleb128(staticFieldsSize);
    file.writeUleb128(instanceFieldsSize);
    file.writeUleb128(directMethodsSize);
    file.writeUleb128(virtualMethodsSize);
    for (int i = 0; i < staticFieldsSize; i++) {
      staticFields[i].write(file);
    }
    for (int i = 0; i < instanceFieldsSize; i++) {
      instanceFields[i].write(file);
    }
    for (int i = 0; i < directMethodsSize; i++) {
      directMethods[i].write(file);
    }
    for (int i = 0; i < virtualMethodsSize; i++) {
      virtualMethods[i].write(file);
    }
  }

  private void incrementEncodedFields(int insertedIdx, EncodedField[] fields) {
    int fieldIdx = 0;
    for (EncodedField field : fields) {
      fieldIdx = field.fieldIdxDiff;
      if (fieldIdx >= insertedIdx) {
        field.fieldIdxDiff++;
        // Only need to increment one, as all the others are diffed from the previous.
        break;
      }
    }
  }

  private void incrementEncodedMethods(int insertedIdx, EncodedMethod[] methods) {
    int methodIdx = 0;
    for (EncodedMethod method : methods) {
      methodIdx = method.methodIdxDiff;
      if (methodIdx >= insertedIdx) {
        method.methodIdxDiff++;
        // Only need to increment one, as all the others are diffed from the previous.
        break;
      }
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    if (kind == IndexUpdateKind.FIELD_ID) {
      incrementEncodedFields(insertedIdx, staticFields);
      incrementEncodedFields(insertedIdx, instanceFields);
    }
    if (kind == IndexUpdateKind.METHOD_ID) {
      incrementEncodedMethods(insertedIdx, directMethods);
      incrementEncodedMethods(insertedIdx, virtualMethods);
    }
  }

  /**
   * For a given field index, search this ClassDataItem for a definition of this field.
   * @return null if the field isn't in this ClassDataItem.
   */
  public EncodedField getEncodedFieldWithIndex(int fieldIdx) {
    int searchFieldIdx = 0;
    for (EncodedField field : instanceFields) {
      searchFieldIdx += field.fieldIdxDiff;
      if (searchFieldIdx == fieldIdx) {
        return field;
      }
    }
    searchFieldIdx = 0;
    for (EncodedField field : staticFields) {
      searchFieldIdx += field.fieldIdxDiff;
      if (searchFieldIdx == fieldIdx) {
        return field;
      }
    }
    return null;
  }
}
