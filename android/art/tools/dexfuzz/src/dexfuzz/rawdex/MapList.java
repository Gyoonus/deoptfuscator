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

public class MapList implements RawDexObject {

  private RawDexFile rawDexFile;

  public int size;
  public List<MapItem> mapItems;

  public MapList(RawDexFile rawDexFile) {
    this.rawDexFile = rawDexFile;
  }

  @Override
  public void read(DexRandomAccessFile file) throws IOException {
    // Find the map list.
    file.seek(rawDexFile.header.mapOff.getOriginalOffset());

    file.getOffsetTracker().getNewOffsettable(file, this);

    // Get the number of entries.
    size = file.readUInt();

    // Allocate and populate the array.
    mapItems = new ArrayList<MapItem>(size);
    for (int i = 0; i < size; i++) {
      MapItem mapItem = new MapItem();
      mapItems.add(mapItem);
      mapItem.read(file);
    }

    file.getOffsetTracker().rememberPointAfterMapList();

    // NB: We track the current index into the MapList, so when we encounter the DebugInfoItem
    // MapItem, we know how to find the next MapItem, so we know how large the DebugInfo
    // area is, so we can copy it as a blob.
    int mapItemIdx = 0;

    // Iterate through the list, and create all the other data structures.
    for (MapItem mapItem : mapItems) {
      file.seek(mapItem.offset.getOriginalOffset());
      switch (mapItem.type) {
        case MapItem.TYPE_HEADER_ITEM:
          // Already read it; skip.
          break;
        case MapItem.TYPE_STRING_ID_ITEM:
          for (int i = 0; i < mapItem.size; i++) {
            StringIdItem newStringId = new StringIdItem();
            rawDexFile.stringIds.add(newStringId);
            newStringId.read(file);
          }
          break;
        case MapItem.TYPE_TYPE_ID_ITEM:
          for (int i = 0; i < mapItem.size; i++) {
            TypeIdItem newTypeId = new TypeIdItem();
            rawDexFile.typeIds.add(newTypeId);
            newTypeId.read(file);
          }
          break;
        case MapItem.TYPE_PROTO_ID_ITEM:
          for (int i = 0; i < mapItem.size; i++) {
            ProtoIdItem newProtoId = new ProtoIdItem();
            rawDexFile.protoIds.add(newProtoId);
            newProtoId.read(file);
          }
          break;
        case MapItem.TYPE_FIELD_ID_ITEM:
          for (int i = 0; i < mapItem.size; i++) {
            FieldIdItem newFieldId = new FieldIdItem();
            rawDexFile.fieldIds.add(newFieldId);
            newFieldId.read(file);
          }
          break;
        case MapItem.TYPE_METHOD_ID_ITEM:
          for (int i = 0; i < mapItem.size; i++) {
            MethodIdItem newMethodId = new MethodIdItem();
            rawDexFile.methodIds.add(newMethodId);
            newMethodId.read(file);
          }
          break;
        case MapItem.TYPE_CLASS_DEF_ITEM:
          for (int i = 0; i < mapItem.size; i++) {
            ClassDefItem newClassDef = new ClassDefItem();
            rawDexFile.classDefs.add(newClassDef);
            newClassDef.read(file);
          }
          break;
        case MapItem.TYPE_MAP_LIST:
          // Already read it; skip.
          break;
        case MapItem.TYPE_TYPE_LIST:
          rawDexFile.typeLists = new ArrayList<TypeList>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            TypeList newTypeList = new TypeList();
            rawDexFile.typeLists.add(newTypeList);
            newTypeList.read(file);
          }
          break;
        case MapItem.TYPE_ANNOTATION_SET_REF_LIST:
          rawDexFile.annotationSetRefLists =
            new ArrayList<AnnotationSetRefList>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            AnnotationSetRefList newAnnotationSetRefList = new AnnotationSetRefList();
            rawDexFile.annotationSetRefLists.add(newAnnotationSetRefList);
            newAnnotationSetRefList.read(file);
          }
          break;
        case MapItem.TYPE_ANNOTATION_SET_ITEM:
          rawDexFile.annotationSetItems = new ArrayList<AnnotationSetItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            AnnotationSetItem newAnnotationSetItem = new AnnotationSetItem();
            rawDexFile.annotationSetItems.add(newAnnotationSetItem);
            newAnnotationSetItem.read(file);
          }
          break;
        case MapItem.TYPE_CLASS_DATA_ITEM:
          rawDexFile.classDatas = new ArrayList<ClassDataItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            ClassDataItem newClassData = new ClassDataItem();
            rawDexFile.classDatas.add(newClassData);
            newClassData.read(file);
          }
          break;
        case MapItem.TYPE_CODE_ITEM:
          rawDexFile.codeItems = new ArrayList<CodeItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            CodeItem newCodeItem = new CodeItem();
            rawDexFile.codeItems.add(newCodeItem);
            newCodeItem.read(file);
          }
          break;
        case MapItem.TYPE_STRING_DATA_ITEM:
          rawDexFile.stringDatas = new ArrayList<StringDataItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            StringDataItem newStringData = new StringDataItem();
            rawDexFile.stringDatas.add(newStringData);
            newStringData.read(file);
          }
          break;
        case MapItem.TYPE_DEBUG_INFO_ITEM:
        {
          // We aren't interested in updating the debug data, so just read it as a blob.
          long start = mapItem.offset.getOriginalOffset();
          long end = 0;
          if (mapItemIdx + 1 == mapItems.size()) {
            end = file.length();
          } else {
            end = mapItems.get(mapItemIdx + 1).offset.getOriginalOffset();
          }
          long size = end - start;
          rawDexFile.debugInfoItem = new DebugInfoItem((int)size);
          rawDexFile.debugInfoItem.read(file);
          break;
        }
        case MapItem.TYPE_ANNOTATION_ITEM:
          rawDexFile.annotationItems = new ArrayList<AnnotationItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            AnnotationItem newAnnotationItem = new AnnotationItem();
            rawDexFile.annotationItems.add(newAnnotationItem);
            newAnnotationItem.read(file);
          }
          break;
        case MapItem.TYPE_ENCODED_ARRAY_ITEM:
          rawDexFile.encodedArrayItems = new ArrayList<EncodedArrayItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            EncodedArrayItem newEncodedArrayItem = new EncodedArrayItem();
            rawDexFile.encodedArrayItems.add(newEncodedArrayItem);
            newEncodedArrayItem.read(file);
          }
          break;
        case MapItem.TYPE_ANNOTATIONS_DIRECTORY_ITEM:
          rawDexFile.annotationsDirectoryItems =
          new ArrayList<AnnotationsDirectoryItem>(mapItem.size);
          for (int i = 0; i < mapItem.size; i++) {
            AnnotationsDirectoryItem newAnnotationsDirectoryItem = new AnnotationsDirectoryItem();
            rawDexFile.annotationsDirectoryItems.add(newAnnotationsDirectoryItem);
            newAnnotationsDirectoryItem.read(file);
          }
          break;
        default:
          Log.errorAndQuit("Encountered unknown map item when reading map item list.");
      }
      mapItemIdx++;
    }
  }

  @Override
  public void write(DexRandomAccessFile file) throws IOException {
    file.alignForwards(4);
    file.getOffsetTracker().updatePositionOfNextOffsettable(file);
    file.writeUInt(mapItems.size());
    for (MapItem mapItem : mapItems) {
      mapItem.write(file);
    }
  }

  @Override
  public void incrementIndex(IndexUpdateKind kind, int insertedIdx) {
    // Do nothing.
  }
}
