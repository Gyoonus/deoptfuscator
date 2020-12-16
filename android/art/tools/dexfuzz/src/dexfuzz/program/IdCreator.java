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

package dexfuzz.program;

import dexfuzz.Log;
import dexfuzz.rawdex.FieldIdItem;
import dexfuzz.rawdex.MethodIdItem;
import dexfuzz.rawdex.Offset;
import dexfuzz.rawdex.Offsettable;
import dexfuzz.rawdex.ProtoIdItem;
import dexfuzz.rawdex.RawDexFile;
import dexfuzz.rawdex.RawDexObject.IndexUpdateKind;
import dexfuzz.rawdex.StringDataItem;
import dexfuzz.rawdex.StringIdItem;
import dexfuzz.rawdex.TypeIdItem;
import dexfuzz.rawdex.TypeItem;
import dexfuzz.rawdex.TypeList;

import java.util.ArrayList;
import java.util.List;

/**
 * Responsible for the finding and creation of TypeIds, MethodIds, FieldIds, and StringIds,
 * during mutation.
 */
public class IdCreator {
  private RawDexFile rawDexFile;

  public IdCreator(RawDexFile rawDexFile) {
    this.rawDexFile = rawDexFile;
  }

  private int findProtoIdInsertionPoint(String signature) {
    int returnTypeIdx = findTypeId(convertSignatureToReturnType(signature));
    String[] parameterListStrings = convertSignatureToParameterList(signature);
    TypeList parameterList = null;
    if (parameterListStrings.length > 0) {
      parameterList = findTypeList(parameterListStrings);
    }

    if (returnTypeIdx < 0) {
      Log.errorAndQuit("Did not create necessary return type before finding insertion "
          + "point for new proto!");
    }

    if (parameterListStrings.length > 0 && parameterList == null) {
      Log.errorAndQuit("Did not create necessary parameter list before finding insertion "
          + "point for new proto!");
    }

    int protoIdIdx = 0;
    for (ProtoIdItem protoId : rawDexFile.protoIds) {
      if (returnTypeIdx < protoId.returnTypeIdx) {
        break;
      }
      if (returnTypeIdx == protoId.returnTypeIdx
          && parameterListStrings.length == 0) {
        break;
      }
      if (returnTypeIdx == protoId.returnTypeIdx
          && parameterListStrings.length > 0
          && protoId.parametersOff.pointsToSomething()
          && parameterList.comesBefore(
              (TypeList) protoId.parametersOff.getPointedToItem())) {
        break;
      }
      protoIdIdx++;
    }
    return protoIdIdx;
  }

  private int findMethodIdInsertionPoint(String className, String methodName, String signature) {
    int classIdx = findTypeId(className);
    int nameIdx = findString(methodName);
    int protoIdx = findProtoId(signature);

    if (classIdx < 0 || nameIdx < 0 || protoIdx < 0) {
      Log.errorAndQuit("Did not create necessary class, name or proto strings before finding "
          + " insertion point for new method!");
    }

    int methodIdIdx = 0;
    for (MethodIdItem methodId : rawDexFile.methodIds) {
      if (classIdx < methodId.classIdx) {
        break;
      }
      if (classIdx == methodId.classIdx && nameIdx < methodId.nameIdx) {
        break;
      }
      if (classIdx == methodId.classIdx && nameIdx == methodId.nameIdx
          && protoIdx < methodId.protoIdx) {
        break;
      }
      methodIdIdx++;
    }
    return methodIdIdx;
  }

  private int findTypeIdInsertionPoint(String className) {
    int descriptorIdx = findString(className);

    if (descriptorIdx < 0) {
      Log.errorAndQuit("Did not create necessary descriptor string before finding "
          + " insertion point for new type!");
    }

    int typeIdIdx = 0;
    for (TypeIdItem typeId : rawDexFile.typeIds) {
      if (descriptorIdx < typeId.descriptorIdx) {
        break;
      }
      typeIdIdx++;
    }
    return typeIdIdx;
  }

  private int findStringDataInsertionPoint(String string) {
    int stringDataIdx = 0;
    for (StringDataItem stringData : rawDexFile.stringDatas) {
      if (stringData.getSize() > 0 && stringData.getString().compareTo(string) >= 0) {
        break;
      }
      stringDataIdx++;
    }
    return stringDataIdx;
  }

  private int findFieldIdInsertionPoint(String className, String typeName, String fieldName) {
    int classIdx = findTypeId(className);
    int typeIdx = findTypeId(typeName);
    int nameIdx = findString(fieldName);

    if (classIdx < 0 || typeIdx < 0 || nameIdx < 0) {
      Log.errorAndQuit("Did not create necessary class, type or name strings before finding "
          + " insertion point for new field!");
    }

    int fieldIdIdx = 0;
    for (FieldIdItem fieldId : rawDexFile.fieldIds) {
      if (classIdx < fieldId.classIdx) {
        break;
      }
      if (classIdx == fieldId.classIdx && nameIdx < fieldId.nameIdx) {
        break;
      }
      if (classIdx == fieldId.classIdx && nameIdx == fieldId.nameIdx
          && typeIdx < fieldId.typeIdx) {
        break;
      }
      fieldIdIdx++;
    }
    return fieldIdIdx;
  }

  private int createMethodId(String className, String methodName, String signature) {
    if (rawDexFile.methodIds.size() >= 65536) {
      Log.errorAndQuit("Referenced too many methods for the DEX file.");
    }

    // Search for (or create) the prototype.
    int protoIdx = findOrCreateProtoId(signature);

    // Search for (or create) the owning class.
    // NB: findOrCreateProtoId could create new types, so this must come
    //     after it!
    int typeIdIdx = findOrCreateTypeId(className);

    // Search for (or create) the string representing the method name.
    // NB: findOrCreateProtoId/TypeId could create new strings, so this must come
    //     after them!
    int methodNameStringIdx = findOrCreateString(methodName);

    // Create MethodIdItem.
    MethodIdItem newMethodId = new MethodIdItem();
    newMethodId.classIdx = (short) typeIdIdx;
    newMethodId.protoIdx = (short) protoIdx;
    newMethodId.nameIdx = methodNameStringIdx;

    // MethodIds must be ordered.
    int newMethodIdIdx = findMethodIdInsertionPoint(className, methodName, signature);

    rawDexFile.methodIds.add(newMethodIdIdx, newMethodId);

    // Insert into OffsetTracker.
    if (newMethodIdIdx == 0) {
      rawDexFile.getOffsetTracker()
        .insertNewOffsettableAsFirstOfType(newMethodId, rawDexFile);
    } else {
      MethodIdItem prevMethodId = rawDexFile.methodIds.get(newMethodIdIdx - 1);
      rawDexFile.getOffsetTracker().insertNewOffsettableAfter(newMethodId, prevMethodId);
    }

    Log.info(String.format("Created new MethodIdItem for %s %s %s, index: 0x%04x",
        className, methodName, signature, newMethodIdIdx));

    // Now that we've potentially moved a lot of method IDs along, all references
    // to them need to be updated.
    rawDexFile.incrementIndex(IndexUpdateKind.METHOD_ID, newMethodIdIdx);

    // All done, return the index for the new method.
    return newMethodIdIdx;
  }

  private int findMethodId(String className, String methodName, String signature) {
    int classIdx = findTypeId(className);
    if (classIdx == -1) {
      return -1;
    }
    int nameIdx = findString(methodName);
    if (nameIdx == -1) {
      return -1;
    }
    int protoIdx = findProtoId(signature);
    if (nameIdx == -1) {
      return -1;
    }

    int methodIdIdx = 0;
    for (MethodIdItem methodId : rawDexFile.methodIds) {
      if (classIdx == methodId.classIdx
          && nameIdx == methodId.nameIdx
          && protoIdx == methodId.protoIdx) {
        return methodIdIdx;
      }
      methodIdIdx++;
    }
    return -1;
  }

  /**
   * Given a fully qualified class name (Ljava/lang/System;), method name (gc) and
   * and signature (()V), either find the MethodId in our DEX file's table, or create it.
   */
  public int findOrCreateMethodId(String className, String methodName, String shorty) {
    int methodIdIdx = findMethodId(className, methodName, shorty);
    if (methodIdIdx != -1) {
      return methodIdIdx;
    }
    return createMethodId(className, methodName, shorty);
  }

  private int createTypeId(String className) {
    if (rawDexFile.typeIds.size() >= 65536) {
      Log.errorAndQuit("Referenced too many classes for the DEX file.");
    }

    // Search for (or create) the string representing the class descriptor.
    int descriptorStringIdx = findOrCreateString(className);

    // Create TypeIdItem.
    TypeIdItem newTypeId = new TypeIdItem();
    newTypeId.descriptorIdx = descriptorStringIdx;

    // TypeIds must be ordered.
    int newTypeIdIdx = findTypeIdInsertionPoint(className);

    rawDexFile.typeIds.add(newTypeIdIdx, newTypeId);

    // Insert into OffsetTracker.
    if (newTypeIdIdx == 0) {
      rawDexFile.getOffsetTracker().insertNewOffsettableAsFirstOfType(newTypeId, rawDexFile);
    } else {
      TypeIdItem prevTypeId = rawDexFile.typeIds.get(newTypeIdIdx - 1);
      rawDexFile.getOffsetTracker().insertNewOffsettableAfter(newTypeId, prevTypeId);
    }

    Log.info(String.format("Created new ClassIdItem for %s, index: 0x%04x",
        className, newTypeIdIdx));

    // Now that we've potentially moved a lot of type IDs along, all references
    // to them need to be updated.
    rawDexFile.incrementIndex(IndexUpdateKind.TYPE_ID, newTypeIdIdx);

    // All done, return the index for the new class.
    return newTypeIdIdx;
  }

  private int findTypeId(String className) {
    int descriptorIdx = findString(className);
    if (descriptorIdx == -1) {
      return -1;
    }

    // Search for class.
    int typeIdIdx = 0;
    for (TypeIdItem typeId : rawDexFile.typeIds) {
      if (descriptorIdx == typeId.descriptorIdx) {
        return typeIdIdx;
      }
      typeIdIdx++;
    }
    return -1;
  }

  /**
   * Given a fully qualified class name (Ljava/lang/System;)
   * either find the TypeId in our DEX file's table, or create it.
   */
  public int findOrCreateTypeId(String className) {
    int typeIdIdx = findTypeId(className);
    if (typeIdIdx != -1) {
      return typeIdIdx;
    }
    return createTypeId(className);
  }

  private int createString(String string) {
    // Didn't find it, create one...
    int stringsCount = rawDexFile.stringIds.size();
    if (stringsCount != rawDexFile.stringDatas.size()) {
      Log.errorAndQuit("Corrupted DEX file, len(StringIDs) != len(StringDatas)");
    }

    // StringData must be ordered.
    int newStringIdx = findStringDataInsertionPoint(string);

    // Create StringDataItem.
    StringDataItem newStringData = new StringDataItem();
    newStringData.setSize(string.length());
    newStringData.setString(string);

    rawDexFile.stringDatas.add(newStringIdx, newStringData);

    // Insert into OffsetTracker.
    // (Need to save the Offsettable, because the StringIdItem will point to it.)
    Offsettable offsettableStringData = null;
    if (newStringIdx == 0) {
      offsettableStringData =
          rawDexFile.getOffsetTracker()
          .insertNewOffsettableAsFirstOfType(newStringData, rawDexFile);
    } else {
      StringDataItem prevStringData = rawDexFile.stringDatas.get(newStringIdx - 1);
      offsettableStringData = rawDexFile.getOffsetTracker()
          .insertNewOffsettableAfter(newStringData, prevStringData);
    }

    // Create StringIdItem.
    StringIdItem newStringId = new StringIdItem();
    newStringId.stringDataOff = new Offset(false);
    newStringId.stringDataOff.pointToNew(offsettableStringData);

    rawDexFile.stringIds.add(newStringIdx, newStringId);

    // Insert into OffsetTracker.
    if (newStringIdx == 0) {
      rawDexFile.getOffsetTracker()
        .insertNewOffsettableAsFirstOfType(newStringId, rawDexFile);
    } else {
      StringIdItem prevStringId = rawDexFile.stringIds.get(newStringIdx - 1);
      rawDexFile.getOffsetTracker().insertNewOffsettableAfter(newStringId, prevStringId);
    }


    Log.info(String.format("Created new StringIdItem and StringDataItem for %s, index: 0x%04x",
        string, newStringIdx));

    // Now that we've potentially moved a lot of string IDs along, all references
    // to them need to be updated.
    rawDexFile.incrementIndex(IndexUpdateKind.STRING_ID, newStringIdx);

    // All done, return the index for the new string.
    return newStringIdx;
  }

  private int findString(String string) {
    // Search for string.
    int stringIdx = 0;
    for (StringDataItem stringDataItem : rawDexFile.stringDatas) {
      if (stringDataItem.getSize() == 0 && string.isEmpty()) {
        return stringIdx;
      } else if (stringDataItem.getSize() > 0 && stringDataItem.getString().equals(string)) {
        return stringIdx;
      }
      stringIdx++;
    }
    return -1;
  }

  /**
   * Given a string, either find the StringId in our DEX file's table, or create it.
   */
  public int findOrCreateString(String string) {
    int stringIdx = findString(string);
    if (stringIdx != -1) {
      return stringIdx;
    }
    return createString(string);
  }

  private int createFieldId(String className, String typeName, String fieldName) {
    if (rawDexFile.fieldIds.size() >= 65536) {
      Log.errorAndQuit("Referenced too many fields for the DEX file.");
    }

    // Search for (or create) the owning class.
    int classIdx = findOrCreateTypeId(className);

    // Search for (or create) the field's type.
    int typeIdx = findOrCreateTypeId(typeName);

    // The creation of the typeIdx may have changed the classIdx, search again!
    classIdx = findOrCreateTypeId(className);

    // Search for (or create) the string representing the field name.
    int fieldNameStringIdx = findOrCreateString(fieldName);

    // Create FieldIdItem.
    FieldIdItem newFieldId = new FieldIdItem();
    newFieldId.classIdx = (short) classIdx;
    newFieldId.typeIdx = (short) typeIdx;
    newFieldId.nameIdx = fieldNameStringIdx;

    // FieldIds must be ordered.
    int newFieldIdIdx = findFieldIdInsertionPoint(className, typeName, fieldName);

    rawDexFile.fieldIds.add(newFieldIdIdx, newFieldId);

    // Insert into OffsetTracker.
    if (newFieldIdIdx == 0 && rawDexFile.fieldIds.size() == 1) {
      // Special case: we didn't have any fields before!
      rawDexFile.getOffsetTracker()
        .insertNewOffsettableAsFirstEverField(newFieldId, rawDexFile);
    } else if (newFieldIdIdx == 0) {
      rawDexFile.getOffsetTracker().insertNewOffsettableAsFirstOfType(newFieldId, rawDexFile);
    } else {
      FieldIdItem prevFieldId = rawDexFile.fieldIds.get(newFieldIdIdx - 1);
      rawDexFile.getOffsetTracker().insertNewOffsettableAfter(newFieldId, prevFieldId);
    }

    Log.info(String.format("Created new FieldIdItem for %s %s %s, index: 0x%04x",
        className, typeName, fieldName, newFieldIdIdx));

    // Now that we've potentially moved a lot of field IDs along, all references
    // to them need to be updated.
    rawDexFile.incrementIndex(IndexUpdateKind.FIELD_ID, newFieldIdIdx);

    // All done, return the index for the new field.
    return newFieldIdIdx;
  }

  private int findFieldId(String className, String typeName, String fieldName) {
    int classIdx = findTypeId(className);
    if (classIdx == -1) {
      return -1;
    }
    int typeIdx = findTypeId(typeName);
    if (typeIdx == -1) {
      return -1;
    }
    int nameIdx = findString(fieldName);
    if (nameIdx == -1) {
      return -1;
    }

    int fieldIdIdx = 0;
    for (FieldIdItem fieldId : rawDexFile.fieldIds) {
      if (classIdx == fieldId.classIdx
          && typeIdx == fieldId.typeIdx
          && nameIdx == fieldId.nameIdx) {
        return fieldIdIdx;
      }
      fieldIdIdx++;
    }
    return -1;
  }

  /**
   * Given a field's fully qualified class name, type name, and name,
   * either find the FieldId in our DEX file's table, or create it.
   */
  public int findOrCreateFieldId(String className, String typeName, String fieldName) {
    int fieldIdx = findFieldId(className, typeName, fieldName);
    if (fieldIdx != -1) {
      return fieldIdx;
    }
    return createFieldId(className, typeName, fieldName);
  }

  /**
   * Returns a 1 or 2 element String[]. If 1 element, the only element is the return type
   * part of the signature. If 2 elements, the first is the parameters, the second is
   * the return type.
   */
  private String[] convertSignatureToParametersAndReturnType(String signature) {
    if (signature.charAt(0) != '(' || !signature.contains(")")) {
      Log.errorAndQuit("Invalid signature: " + signature);
    }
    String[] elems = signature.substring(1).split("\\)");
    return elems;
  }

  private String[] convertSignatureToParameterList(String signature) {
    String[] elems = convertSignatureToParametersAndReturnType(signature);
    String parameters = "";
    if (elems.length == 2) {
      parameters = elems[0];
    }

    List<String> parameterList = new ArrayList<String>();

    int typePointer = 0;
    while (typePointer != parameters.length()) {
      if (elems[0].charAt(typePointer) == 'L') {
        int start = typePointer;
        // Read up to the next ;
        while (elems[0].charAt(typePointer) != ';') {
          typePointer++;
        }
        parameterList.add(parameters.substring(start, typePointer + 1));
      } else {
        parameterList.add(Character.toString(parameters.charAt(typePointer)));
      }
      typePointer++;
    }

    return parameterList.toArray(new String[]{});
  }

  private String convertSignatureToReturnType(String signature) {
    String[] elems = convertSignatureToParametersAndReturnType(signature);
    String returnType = "";
    if (elems.length == 1) {
      returnType = elems[0];
    } else {
      returnType = elems[1];
    }

    return returnType;
  }

  private String convertSignatureToShorty(String signature) {
    String[] elems = convertSignatureToParametersAndReturnType(signature);

    StringBuilder shortyBuilder = new StringBuilder();

    String parameters = "";
    String returnType = "";

    if (elems.length == 1) {
      shortyBuilder.append("V");
    } else {
      parameters = elems[0];
      returnType = elems[1];
      char returnChar = returnType.charAt(0);
      // Arrays are references in shorties.
      if (returnChar == '[') {
        returnChar = 'L';
      }
      shortyBuilder.append(returnChar);
    }

    int typePointer = 0;
    while (typePointer != parameters.length()) {
      if (parameters.charAt(typePointer) == 'L') {
        shortyBuilder.append('L');
        // Read up to the next ;
        while (parameters.charAt(typePointer) != ';') {
          typePointer++;
          if (typePointer == parameters.length()) {
            Log.errorAndQuit("Illegal type specified in signature - L with no ;!");
          }
        }
      } else if (parameters.charAt(typePointer) == '[') {
        // Arrays are references in shorties.
        shortyBuilder.append('L');
        // Read past all the [s
        while (parameters.charAt(typePointer) == '[') {
          typePointer++;
        }
        if (parameters.charAt(typePointer) == 'L') {
          // Read up to the next ;
          while (parameters.charAt(typePointer) != ';') {
            typePointer++;
            if (typePointer == parameters.length()) {
              Log.errorAndQuit("Illegal type specified in signature - L with no ;!");
            }
          }
        }
      } else {
        shortyBuilder.append(parameters.charAt(typePointer));
      }

      typePointer++;
    }

    return shortyBuilder.toString();
  }

  private Integer[] convertParameterListToTypeIdList(String[] parameterList) {
    List<Integer> typeIdList = new ArrayList<Integer>();
    for (String parameter : parameterList) {
      int typeIdx = findTypeId(parameter);
      if (typeIdx == -1) {
        return null;
      }
      typeIdList.add(typeIdx);
    }
    return typeIdList.toArray(new Integer[]{});
  }

  private TypeList createTypeList(String[] parameterList) {
    TypeList typeList = new TypeList();
    List<TypeItem> typeItemList = new ArrayList<TypeItem>();

    // This must be done as two passes, one to create all the types,
    // and then one to put them in the type list.
    for (String parameter : parameterList) {
      findOrCreateTypeId(parameter);
    }

    // Now actually put them in the list.
    for (String parameter : parameterList) {
      TypeItem typeItem = new TypeItem();
      typeItem.typeIdx = (short) findOrCreateTypeId(parameter);
      typeItemList.add(typeItem);
    }
    typeList.list = typeItemList.toArray(new TypeItem[]{});
    typeList.size = typeItemList.size();

    // Insert into OffsetTracker.
    if (rawDexFile.typeLists == null) {
      // Special case: we didn't have any fields before!
      Log.info("Need to create first type list.");
      rawDexFile.typeLists = new ArrayList<TypeList>();
      rawDexFile.getOffsetTracker()
        .insertNewOffsettableAsFirstEverTypeList(typeList, rawDexFile);
    } else {
      TypeList prevTypeList =
          rawDexFile.typeLists.get(rawDexFile.typeLists.size() - 1);
      rawDexFile.getOffsetTracker().insertNewOffsettableAfter(typeList, prevTypeList);
    }

    // Finally, add this new TypeList to the list of them.
    rawDexFile.typeLists.add(typeList);

    return typeList;
  }

  private TypeList findTypeList(String[] parameterList) {
    Integer[] typeIdList = convertParameterListToTypeIdList(parameterList);
    if (typeIdList == null) {
      return null;
    }

    if (rawDexFile.typeLists == null) {
      // There's no type lists yet!
      return null;
    }

    for (TypeList typeList : rawDexFile.typeLists) {
      if (typeList.size != typeIdList.length) {
        continue;
      }

      boolean found = true;
      int idx = 0;
      for (TypeItem typeItem : typeList.list) {
        if (typeItem.typeIdx != typeIdList[idx]) {
          found = false;
          break;
        }
        idx++;
      }
      if (found && idx == parameterList.length) {
        return typeList;
      }
    }

    return null;
  }

  private TypeList findOrCreateTypeList(String[] parameterList) {
    TypeList typeList = findTypeList(parameterList);
    if (typeList != null) {
      return typeList;
    }
    return createTypeList(parameterList);
  }

  private int createProtoId(String signature) {
    String shorty = convertSignatureToShorty(signature);
    String returnType = convertSignatureToReturnType(signature);
    String[] parameterList = convertSignatureToParameterList(signature);

    if (rawDexFile.protoIds.size() >= 65536) {
      Log.errorAndQuit("Referenced too many protos for the DEX file.");
    }

    TypeList typeList = null;
    Offsettable typeListOffsettable = null;

    if (parameterList.length > 0) {
      // Search for (or create) the parameter list.
      typeList = findOrCreateTypeList(parameterList);

      typeListOffsettable =
          rawDexFile.getOffsetTracker().getOffsettableForItem(typeList);
    }

    // Search for (or create) the return type.
    int returnTypeIdx = findOrCreateTypeId(returnType);

    // Search for (or create) the shorty string.
    int shortyIdx = findOrCreateString(shorty);

    // Create ProtoIdItem.
    ProtoIdItem newProtoId = new ProtoIdItem();
    newProtoId.shortyIdx = shortyIdx;
    newProtoId.returnTypeIdx = returnTypeIdx;
    newProtoId.parametersOff = new Offset(false);
    if (parameterList.length > 0) {
      newProtoId.parametersOff.pointToNew(typeListOffsettable);
    }

    // ProtoIds must be ordered.
    int newProtoIdIdx = findProtoIdInsertionPoint(signature);

    rawDexFile.protoIds.add(newProtoIdIdx, newProtoId);

    // Insert into OffsetTracker.
    if (newProtoIdIdx == 0) {
      rawDexFile.getOffsetTracker().insertNewOffsettableAsFirstOfType(newProtoId, rawDexFile);
    } else {
      ProtoIdItem prevProtoId = rawDexFile.protoIds.get(newProtoIdIdx - 1);
      rawDexFile.getOffsetTracker().insertNewOffsettableAfter(newProtoId, prevProtoId);
    }

    Log.info(String.format("Created new ProtoIdItem for %s, index: 0x%04x",
        signature, newProtoIdIdx));

    // Now that we've potentially moved a lot of proto IDs along, all references
    // to them need to be updated.
    rawDexFile.incrementIndex(IndexUpdateKind.PROTO_ID, newProtoIdIdx);

    // All done, return the index for the new proto.
    return newProtoIdIdx;
  }

  private int findProtoId(String signature) {
    String shorty = convertSignatureToShorty(signature);
    String returnType = convertSignatureToReturnType(signature);
    String[] parameterList = convertSignatureToParameterList(signature);

    int shortyIdx = findString(shorty);
    if (shortyIdx == -1) {
      return -1;
    }
    int returnTypeIdx = findTypeId(returnType);
    if (returnTypeIdx == -1) {
      return -1;
    }

    // Only look for a TypeList if there's a parameter list.
    TypeList typeList = null;
    if (parameterList.length > 0) {
      typeList = findTypeList(parameterList);
      if (typeList == null) {
        return -1;
      }
    }

    int protoIdIdx = 0;
    for (ProtoIdItem protoId : rawDexFile.protoIds) {
      if (parameterList.length > 0) {
        // With parameters.
        if (shortyIdx == protoId.shortyIdx
            && returnTypeIdx == protoId.returnTypeIdx
            && typeList.equals(protoId.parametersOff.getPointedToItem())) {
          return protoIdIdx;
        }
      } else {
        // Without parameters.
        if (shortyIdx == protoId.shortyIdx
            && returnTypeIdx == protoId.returnTypeIdx) {
          return protoIdIdx;
        }
      }
      protoIdIdx++;
    }
    return -1;
  }

  private int findOrCreateProtoId(String signature) {
    int protoIdx = findProtoId(signature);
    if (protoIdx != -1) {
      return protoIdx;
    }
    return createProtoId(signature);
  }
}
