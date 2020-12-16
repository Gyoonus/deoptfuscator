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
import java.util.ArrayList;
import java.util.List;

public class RawDexFile implements RawDexObject {
  private OffsetTracker offsetTracker;

  public HeaderItem header;

  public MapList mapList;

  // Can be allocated after reading the header.
  public List<StringIdItem> stringIds;
  public List<TypeIdItem> typeIds;
  public List<ProtoIdItem> protoIds;
  public List<FieldIdItem> fieldIds;
  public List<MethodIdItem> methodIds;
  public List<ClassDefItem> classDefs;

  // Need to be allocated later (will be allocated in MapList.java)
  public List<StringDataItem> stringDatas;
  public List<ClassDataItem> classDatas;
  public List<TypeList> typeLists;
  public List<CodeItem> codeItems;
  public DebugInfoItem debugInfoItem;
  public List<AnnotationsDirectoryItem> annotationsDirectoryItems;
  public List<AnnotationSetRefList> annotationSetRefLists;
  public List<AnnotationSetItem> annotationSetItems;
  public List<AnnotationItem> annotationItems;
  public List<EncodedArrayItem> encodedArrayItems;

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    // Get a reference to the OffsetTracker, so that IdCreator can use it.
    offsetTracker = file.getOffsetTracker();

    file.seek(0);

    // Read header.
    (header = new HeaderItem()).read(file);

    // We can allocate all of these now.
    stringIds = new ArrayList<StringIdItem>(header.stringIdsSize);
    typeIds = new ArrayList<TypeIdItem>(header.typeIdsSize);
    protoIds = new ArrayList<ProtoIdItem>(header.protoIdsSize);
    fieldIds = new ArrayList<FieldIdItem>(header.fieldIdsSize);
    methodIds = new ArrayList<MethodIdItem>(header.methodIdsSize);
    classDefs = new ArrayList<ClassDefItem>(header.classDefsSize);

    mapList = new MapList(this);
    mapList.read(file);

    file.getOffsetTracker().associateOffsets();
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.seek(0);

    // We read the header first, and then the map list, and then everything
    // else. Therefore, when we get to the end of the header, tell OffsetTracker
    // to skip past the map list offsets, and then when we get to the map list,
    // tell OffsetTracker to skip back there, and then return to where it was previously.

    // Update the map items' sizes first
    // - but only update the items that we expect to have changed size.
    // ALSO update the header's table sizes!
    for (MapItem mapItem : mapList.mapItems) {
      switch (mapItem.type) {
        case MapItem.TYPE_STRING_ID_ITEM:
          if (mapItem.size != stringIds.size()) {
            Log.debug("Updating StringIDs List size: " + stringIds.size());
            mapItem.size = stringIds.size();
            header.stringIdsSize = stringIds.size();
          }
          break;
        case MapItem.TYPE_STRING_DATA_ITEM:
          if (mapItem.size != stringDatas.size()) {
            Log.debug("Updating StringDatas List size: " + stringDatas.size());
            mapItem.size = stringDatas.size();
          }
          break;
        case MapItem.TYPE_METHOD_ID_ITEM:
          if (mapItem.size != methodIds.size()) {
            Log.debug("Updating MethodIDs List size: " + methodIds.size());
            mapItem.size = methodIds.size();
            header.methodIdsSize = methodIds.size();
          }
          break;
        case MapItem.TYPE_FIELD_ID_ITEM:
          if (mapItem.size != fieldIds.size()) {
            Log.debug("Updating FieldIDs List size: " + fieldIds.size());
            mapItem.size = fieldIds.size();
            header.fieldIdsSize = fieldIds.size();
          }
          break;
        case MapItem.TYPE_PROTO_ID_ITEM:
          if (mapItem.size != protoIds.size()) {
            Log.debug("Updating ProtoIDs List size: " + protoIds.size());
            mapItem.size = protoIds.size();
            header.protoIdsSize = protoIds.size();
          }
          break;
        case MapItem.TYPE_TYPE_ID_ITEM:
          if (mapItem.size != typeIds.size()) {
            Log.debug("Updating TypeIDs List size: " + typeIds.size());
            mapItem.size = typeIds.size();
            header.typeIdsSize = typeIds.size();
          }
          break;
        case MapItem.TYPE_TYPE_LIST:
          if (mapItem.size != typeLists.size()) {
            Log.debug("Updating TypeLists List size: " + typeLists.size());
            mapItem.size = typeLists.size();
          }
          break;
        default:
      }
    }

    // Use the map list to write the file.
    for (MapItem mapItem : mapList.mapItems) {
      switch (mapItem.type) {
        case MapItem.TYPE_HEADER_ITEM:
          header.write(file);
          file.getOffsetTracker().skipToAfterMapList();
          break;
        case MapItem.TYPE_STRING_ID_ITEM:
          if (mapItem.size != stringIds.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches StringIDs table size " + stringIds.size());
          }
          for (StringIdItem stringId : stringIds) {
            stringId.write(file);
          }
          break;
        case MapItem.TYPE_TYPE_ID_ITEM:
          if (mapItem.size != typeIds.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches TypeIDs table size " + typeIds.size());
          }
          for (TypeIdItem typeId : typeIds) {
            typeId.write(file);
          }
          break;
        case MapItem.TYPE_PROTO_ID_ITEM:
          if (mapItem.size != protoIds.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches ProtoIDs table size " + protoIds.size());
          }
          for (ProtoIdItem protoId : protoIds) {
            protoId.write(file);
          }
          break;
        case MapItem.TYPE_FIELD_ID_ITEM:
          if (mapItem.size != fieldIds.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches FieldIDs table size " + fieldIds.size());
          }
          for (FieldIdItem fieldId : fieldIds) {
            fieldId.write(file);
          }
          break;
        case MapItem.TYPE_METHOD_ID_ITEM:
          if (mapItem.size != methodIds.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches MethodIDs table size " + methodIds.size());
          }
          for (MethodIdItem methodId : methodIds) {
            methodId.write(file);
          }
          break;
        case MapItem.TYPE_CLASS_DEF_ITEM:
          if (mapItem.size != classDefs.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches ClassDefs table size " + classDefs.size());
          }
          for (ClassDefItem classDef : classDefs) {
            classDef.write(file);
          }
          break;
        case MapItem.TYPE_MAP_LIST:
          file.getOffsetTracker().goBackToMapList();
          mapList.write(file);
          file.getOffsetTracker().goBackToPreviousPoint();
          break;
        case MapItem.TYPE_TYPE_LIST:
          if (mapItem.size != typeLists.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches TypeLists table size " + typeLists.size());
          }
          for (TypeList typeList : typeLists) {
            typeList.write(file);
          }
          break;
        case MapItem.TYPE_ANNOTATION_SET_REF_LIST:
          if (mapItem.size != annotationSetRefLists.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches AnnotationSetRefLists table size "
                + annotationSetRefLists.size());
          }
          for (AnnotationSetRefList annotationSetRefList : annotationSetRefLists) {
            annotationSetRefList.write(file);
          }
          break;
        case MapItem.TYPE_ANNOTATION_SET_ITEM:
          if (mapItem.size != annotationSetItems.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches AnnotationSetItems table size "
                + annotationSetItems.size());
          }
          for (AnnotationSetItem annotationSetItem : annotationSetItems) {
            annotationSetItem.write(file);
          }
          break;
        case MapItem.TYPE_CLASS_DATA_ITEM:
          if (mapItem.size != classDatas.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches ClassDataItems table size " + classDatas.size());
          }
          for (ClassDataItem classData : classDatas) {
            classData.write(file);
          }
          break;
        case MapItem.TYPE_CODE_ITEM:
          if (mapItem.size != codeItems.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches CodeItems table size " + codeItems.size());
          }
          for (CodeItem codeItem : codeItems) {
            codeItem.write(file);
          }
          break;
        case MapItem.TYPE_STRING_DATA_ITEM:
          if (mapItem.size != stringDatas.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches StringDataItems table size "
                + stringDatas.size());
          }
          for (StringDataItem stringDataItem : stringDatas) {
            stringDataItem.write(file);
          }
          break;
        case MapItem.TYPE_DEBUG_INFO_ITEM:
          debugInfoItem.write(file);
          break;
        case MapItem.TYPE_ANNOTATION_ITEM:
          if (mapItem.size != annotationItems.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches AnnotationItems table size "
                + annotationItems.size());
          }
          for (AnnotationItem annotationItem : annotationItems) {
            annotationItem.write(file);
          }
          break;
        case MapItem.TYPE_ENCODED_ARRAY_ITEM:
          if (mapItem.size != encodedArrayItems.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches EncodedArrayItems table size "
                + encodedArrayItems.size());
          }
          for (EncodedArrayItem encodedArrayItem : encodedArrayItems) {
            encodedArrayItem.write(file);
          }
          break;
        case MapItem.TYPE_ANNOTATIONS_DIRECTORY_ITEM:
          if (mapItem.size != annotationsDirectoryItems.size()) {
            Log.errorAndQuit("MapItem's size " + mapItem.size
                + " no longer matches AnnotationDirectoryItems table size "
                + annotationsDirectoryItems.size());
          }
          for (AnnotationsDirectoryItem annotationsDirectory : annotationsDirectoryItems) {
            annotationsDirectory.write(file);
          }
          break;
        default:
          Log.errorAndQuit("Encountered unknown map item in map item list.");
      }
    }

    file.getOffsetTracker().updateOffsets(file);
  }

  /**
   * Given a DexRandomAccessFile, calculate the correct adler32 checksum for it.
   */
  private int calculateAdler32Checksum(DexRandomAccessFile file) throws IOException {
    // Skip magic + checksum.
    file.seek(12);
    int a = 1;
    int b = 0;
    while (file.getFilePointer() < file.length()) {
      a = (a + file.readUnsignedByte()) % 65521;
      b = (b + a) % 65521;
    }
    return (b << 16) | a;
  }

  /**
   * Given a DexRandomAccessFile, update the file size, data size, and checksum.
   */
  public void updateHeader(DexRandomAccessFile file) throws IOException {
    // File size must be updated before checksum.
    int newFileSize = (int) file.length();
    file.seek(32);
    file.writeUInt(newFileSize);

    // Data size must be updated before checksum.
    int newDataSize = newFileSize - header.dataOff.getNewPositionOfItem();
    file.seek(104);
    file.writeUInt(newDataSize);

    // Now update the checksum.
    int newChecksum = calculateAdler32Checksum(file);
    file.seek(8);
    file.writeUInt(newChecksum);

    header.fileSize = newFileSize;
    header.dataSize = newDataSize;
    header.checksum = newChecksum;
  }

  /**
   * This should only be called from NewItemCreator.
   */
  public OffsetTracker getOffsetTracker() {
    return offsetTracker;
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    for (TypeIdItem typeId : typeIds) {
      typeId.incrementIndex(kind, insertedIdx);
    }
    for (ProtoIdItem protoId : protoIds) {
      protoId.incrementIndex(kind, insertedIdx);
    }
    for (FieldIdItem fieldId : fieldIds) {
      fieldId.incrementIndex(kind, insertedIdx);
    }
    for (MethodIdItem methodId : methodIds) {
      methodId.incrementIndex(kind, insertedIdx);
    }
    for (ClassDefItem classDef : classDefs) {
      classDef.incrementIndex(kind, insertedIdx);
    }
    for (ClassDataItem classData : classDatas) {
      classData.incrementIndex(kind, insertedIdx);
    }
    if (typeLists != null) {
      for (TypeList typeList : typeLists) {
        typeList.incrementIndex(kind, insertedIdx);
      }
    }
    for (CodeItem codeItem : codeItems) {
      codeItem.incrementIndex(kind, insertedIdx);
    }
    if (annotationsDirectoryItems != null) {
      for (AnnotationsDirectoryItem annotationsDirectoryItem : annotationsDirectoryItems) {
        annotationsDirectoryItem.incrementIndex(kind, insertedIdx);
      }
    }
    if (annotationItems != null) {
      for (AnnotationItem annotationItem : annotationItems) {
        annotationItem.incrementIndex(kind, insertedIdx);
      }
    }
  }
}
