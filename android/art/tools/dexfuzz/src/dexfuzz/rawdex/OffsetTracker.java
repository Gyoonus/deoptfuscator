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
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class allows the recording of both Offsettable items (that is, items that can be
 * referred to by an offset somewhere else in the file - RawDexObjects) and Offsets.
 * The idea in a nutshell is that for every Offsettable item we read, we remember
 * its original position in the file using a map, and the order in which the Offsettables were
 * written out. We also remember every Offset we read in, and its value. Then, after reading
 * the whole file, we use the map to find the Offsettable it pointed at.
 * Then, as we write out the file, for every Offsettable we write out, we record its new position,
 * using the order we collected earlier. For every Offset we write out, we look at its Offsettable
 * to see where it was written. If it hasn't been written yet, then we write out a blank value
 * for the time being, remember where that blank value was written, and put the Offset into a
 * table for patching once everything has been written out.
 * There are some variables (index_after_map_list, restore_point) used for remembering certain
 * points to jump forward and back to, because we cannot read and write the file out in exactly
 * the same order.
 * TODO: Perhaps it makes more sense to just reorder the offsettable_table once it's been read,
 * in preparation for the order in which the file is written out?
 * Finally, we provide methods for adding new Offsettable items into the right place in the order
 * table.
 */
public class OffsetTracker {
  /**
   * A map from the original offset in the input DEX file to
   * the Offsettable it points to. (That Offsettable will contain
   * the actual item, and later on the new offset for the item when
   * the item is written out.
   */
  private Map<Integer, Offsettable> offsettableMap;

  /**
   * A table of all Offsettables. We need to ensure we write out
   * all items in the same order we read them in, to make sure we update
   * the Offsettable.new_position field with the correct value wrt to
   * the original_position field.
   */
  private List<Offsettable> offsettableTable;

  /**
   * A table of all offsets that is populated as we read in the DEX file.
   * As the end, we find the correct Offsettable for the Offset in the above
   * map, and associate them.
   */
  private List<Offset> needsAssociationTable;

  /**
   * A table of all offsets that we could not write out an updated offset for
   * as we write out a DEX file. Will be checked after writing is complete,
   * to allow specific patching of each offset's location as at that point
   * all Offsettables will have been updated with their new position.
   */
  private List<Offset> needsUpdateTable;

  /**
   * Tracks how far we are through the offsettable_table as we write out the file.
   */
  private int offsettableTableIdx;

  /**
   * Because we write in a slightly different order to how we read
   * (why? to read, we read the header, then the map list, and then use the map
   *  list to read everything else.
   *  when we write, we write the header, and then we cannot write the map list
   *  because we don't know where it will go yet, so we write everything else first)
   * so: we remember the position in the offsettable_table after we read the map list,
   * so we can skip there after we write out the header.
   */
  private int indexAfterMapList;

  /**
   * Related to index_after_map_list, this is the index we save when we're jumping back to
   * write the map list.
   */
  private int restorePoint;

  /**
   * Create a new OffsetTracker. Should persist between parsing a DEX file, and outputting
   * the mutated DEX file.
   */
  public OffsetTracker() {
    offsettableMap = new HashMap<Integer,Offsettable>();
    offsettableTable = new ArrayList<Offsettable>();
    needsAssociationTable = new ArrayList<Offset>();
    needsUpdateTable = new ArrayList<Offset>();
  }

  /**
   * Lookup an Item by the offset it had in the input DEX file.
   * @param offset The offset in the input DEX file.
   * @return The corresponding Item.
   */
  public RawDexObject getItemByOffset(int offset) {
    return offsettableMap.get(offset).getItem();
  }

  /**
   * As Items are read in, they call this function once they have word-aligned the file pointer,
   * to record their position and themselves into an Offsettable object, that will be tracked.
   * @param file Used for recording position into the new Offsettable.
   * @param item Used for recording the relevant Item into the new Offsettable.
   */
  public void getNewOffsettable(DexRandomAccessFile file, RawDexObject item) throws IOException {
    Offsettable offsettable = new Offsettable(item, false);
    offsettable.setOriginalPosition((int) file.getFilePointer());
    offsettableMap.put(offsettable.getOriginalPosition(), offsettable);
    offsettableTable.add(offsettable);
  }

  /**
   * As Items read in Offsets, they call this function with the offset they originally
   * read from the file, to allow later association with an Offsettable.
   * @param originalOffset The original offset read from the input DEX file.
   * @return An Offset that will later be associated with an Offsettable.
   */
  public Offset getNewOffset(int originalOffset) throws IOException {
    Offset offset = new Offset(false);
    offset.setOriginalOffset(originalOffset);
    needsAssociationTable.add(offset);
    return offset;
  }

  /**
   * Only MapItem should call this method, when the MapItem that points to the header
   * is read.
   */
  public Offset getNewHeaderOffset(int originalOffset) throws IOException {
    Offset offset = new Offset(true);
    offset.setOriginalOffset(originalOffset);
    needsAssociationTable.add(offset);
    return offset;
  }

  /**
   * Call this after reading, to associate Offsets with Offsettables.
   */
  public void associateOffsets() {
    for (Offset offset : needsAssociationTable) {
      if (offset.getOriginalOffset() == 0 && !(offset.pointsAtHeader())) {
        offset.setPointsAtNull();
      } else {
        offset.pointTo(offsettableMap.get(offset.getOriginalOffset()));
        if (!offset.pointsToSomething()) {
          Log.error(String.format("Couldn't find original offset 0x%x!",
              offset.getOriginalOffset()));
        }
      }
    }
    needsAssociationTable.clear();
  }

  /**
   * As Items are written out into the output DEX file, this function is called
   * to update the next Offsettable with the file pointer's current position.
   * This should allow the tracking of new offset locations.
   * This also requires that reading and writing of all items happens in the same order
   * (with the exception of the map list, see above)
   * @param file Used for recording the new position.
   */
  public void updatePositionOfNextOffsettable(DexRandomAccessFile file) throws IOException {
    if (offsettableTableIdx == offsettableTable.size()) {
      Log.errorAndQuit("Not all created Offsettable items have been added to the "
          + "Offsettable Table!");
    }
    Offsettable offsettable = offsettableTable.get(offsettableTableIdx);
    offsettable.setNewPosition((int) file.getFilePointer());
    offsettableTableIdx++;
  }

  /**
   * As Items are written out, any writing out of an offset must call this function, passing
   * in the relevant offset. This function will write out the offset, if the associated
   * Offsettable has been updated with its new position, or else will write out a null value, and
   * the Offset will be stored for writing after all Items have been written, and all
   * Offsettables MUST have been updated.
   * @param offset The offset received from getNewOffset().
   * @param file Used for writing out to the file.
   * @param useUleb128 Whether or not the offset should be written in UINT or ULEB128 form.
   */
  public void tryToWriteOffset(Offset offset, DexRandomAccessFile file, boolean useUleb128)
      throws IOException {
    if (!offset.isNewOffset() && (!offset.pointsToSomething())) {
      if (useUleb128) {
        file.writeUleb128(0);
      } else {
        file.writeUInt(0);
      }
      return;
    }

    if (offset.readyForWriting()) {
      if (useUleb128) {
        file.writeUleb128(offset.getNewPositionOfItem());
      } else {
        file.writeUInt(offset.getNewPositionOfItem());
      }
    } else {
      offset.setOutputLocation((int) file.getFilePointer());
      if (useUleb128) {
        file.writeLargestUleb128(offset.getOriginalOffset());
        offset.setUsesUleb128();
      } else {
        file.writeUInt(offset.getOriginalOffset());
      }
      needsUpdateTable.add(offset);
    }
  }

  /**
   * This is called after all writing has finished, to write out any Offsets
   * that could not be written out during the original writing phase, because their
   * associated Offsettables hadn't had their new positions calculated yet.
   * @param file Used for writing out to the file.
   */
  public void updateOffsets(DexRandomAccessFile file) throws IOException {
    if (offsettableTableIdx != offsettableTable.size()) {
      Log.errorAndQuit("Being asked to update dangling offsets but the "
          + "correct number of offsettables has not been written out!");
    }
    for (Offset offset : needsUpdateTable) {
      file.seek(offset.getOutputLocation());
      if (offset.usesUleb128()) {
        file.writeLargestUleb128(offset.getNewPositionOfItem());
      } else {
        file.writeUInt(offset.getNewPositionOfItem());
      }
    }
    needsUpdateTable.clear();
  }

  /**
   * Called after writing out the header, to skip to after the map list.
   */
  public void skipToAfterMapList() {
    offsettableTableIdx = indexAfterMapList;
  }

  /**
   * Called once the map list needs to be written out, to set the
   * offsettable table index back to the right place.
   */
  public void goBackToMapList() {
    restorePoint = offsettableTableIdx;
    offsettableTableIdx = (indexAfterMapList - 1);
  }

  /**
   * Called once the map list has been written out, to set the
   * offsettable table index back to where it was before.
   */
  public void goBackToPreviousPoint() {
    if (offsettableTableIdx != indexAfterMapList) {
      Log.errorAndQuit("Being asked to go to the point before the MapList was written out,"
          + " but we're not in the right place.");
    }
    offsettableTableIdx = restorePoint;
  }

  /**
   * Called after reading in the map list, to remember the point to be skipped
   * to later.
   */
  public void rememberPointAfterMapList() {
    indexAfterMapList = offsettableTable.size();
  }

  private void updateHeaderOffsetIfValid(Offset offset, Offsettable previousFirst,
      Offsettable newFirst, String offsetName) {
    if (offset.pointsToThisOffsettable(previousFirst)) {
      offset.pointToNew(newFirst);
    } else {
      Log.errorAndQuit("Header " + offsetName + " offset not pointing at first element?");
    }
  }

  private void addTypeListsToMapFile(RawDexFile rawDexFile, Offsettable typeListOffsettable) {
    // Create a MapItem for the TypeLists
    MapItem typeListMapItem = new MapItem();
    typeListMapItem.offset = new Offset(false);
    typeListMapItem.offset.pointToNew(typeListOffsettable);
    typeListMapItem.type = MapItem.TYPE_TYPE_LIST;
    typeListMapItem.size = 1;

    // Insert into the MapList.
    // (first, find the MapItem that points to StringDataItems...)
    int idx = 0;
    for (MapItem mapItem : rawDexFile.mapList.mapItems) {
      if (mapItem.type == MapItem.TYPE_STRING_DATA_ITEM) {
        break;
      }
      idx++;
    }
    // (now insert the TypeList MapItem just before the StringDataItem one...)
    rawDexFile.mapList.mapItems.add(idx, typeListMapItem);
  }

  private void addFieldIdsToHeaderAndMapFile(RawDexFile rawDexFile,
      Offsettable fieldOffsettable) {
    // Add the field IDs to the header.
    rawDexFile.header.fieldIdsOff.unsetNullAndPointTo(fieldOffsettable);
    rawDexFile.header.fieldIdsSize = 1;

    // Create a MapItem for the field IDs.
    MapItem fieldMapItem = new MapItem();
    fieldMapItem.offset = new Offset(false);
    fieldMapItem.offset.pointToNew(fieldOffsettable);
    fieldMapItem.type = MapItem.TYPE_FIELD_ID_ITEM;
    fieldMapItem.size = 1;

    // Insert into the MapList.
    // (first, find the MapItem that points to MethodIdItems...)
    int idx = 0;
    for (MapItem mapItem : rawDexFile.mapList.mapItems) {
      if (mapItem.type == MapItem.TYPE_METHOD_ID_ITEM) {
        break;
      }
      idx++;
    }
    // (now insert the FieldIdItem MapItem just before the MethodIdItem one...)
    rawDexFile.mapList.mapItems.add(idx, fieldMapItem);
  }


  private void updateOffsetsInHeaderAndMapFile(RawDexFile rawDexFile,
      Offsettable newFirstOffsettable) {
    Offsettable prevFirstOffsettable = null;
    for (int i = 0; i < offsettableTable.size(); i++) {
      if (offsettableTable.get(i) == newFirstOffsettable) {
        prevFirstOffsettable = offsettableTable.get(i + 1);
        break;
      }
    }
    if (prevFirstOffsettable == null) {
      Log.errorAndQuit("When calling updateMapListOffsets, could not find new "
          + "first offsettable?");
    }

    // Based on the type of the item we just added, check the relevant Offset in the header
    // and if it pointed at the prev_first_offsettable, make it point at the new one.
    // NB: if it isn't pointing at the prev one, something is wrong.
    HeaderItem header = rawDexFile.header;
    if (newFirstOffsettable.getItem() instanceof StringIdItem) {
      updateHeaderOffsetIfValid(header.stringIdsOff, prevFirstOffsettable,
          newFirstOffsettable, "StringID");
    } else if (newFirstOffsettable.getItem() instanceof TypeIdItem) {
      updateHeaderOffsetIfValid(header.typeIdsOff, prevFirstOffsettable,
          newFirstOffsettable, "TypeID");
    } else if (newFirstOffsettable.getItem() instanceof ProtoIdItem) {
      updateHeaderOffsetIfValid(header.protoIdsOff, prevFirstOffsettable,
          newFirstOffsettable, "ProtoID");
    } else if (newFirstOffsettable.getItem() instanceof FieldIdItem) {
      updateHeaderOffsetIfValid(header.fieldIdsOff, prevFirstOffsettable,
          newFirstOffsettable, "FieldID");
    } else if (newFirstOffsettable.getItem() instanceof MethodIdItem) {
      updateHeaderOffsetIfValid(header.methodIdsOff, prevFirstOffsettable,
          newFirstOffsettable, "MethodID");
    } else if (newFirstOffsettable.getItem() instanceof ClassDefItem) {
      updateHeaderOffsetIfValid(header.classDefsOff, prevFirstOffsettable,
          newFirstOffsettable, "ClassDef");
    }

    // Now iterate through the MapList's MapItems, and see if their Offsets pointed at the
    // prev_first_offsettable, and if so, make them now point at the new_first_offsettable.
    for (MapItem mapItem : rawDexFile.mapList.mapItems) {
      if (mapItem.offset.pointsToThisOffsettable(prevFirstOffsettable)) {
        Log.info("Updating offset in MapItem (type: " + mapItem.type + ") after "
            + "we called insertNewOffsettableAsFirstOfType()");
        mapItem.offset.pointToNew(newFirstOffsettable);
      }
    }
  }

  private void insertOffsettableAt(int idx, Offsettable offsettable) {
    offsettableTable.add(idx, offsettable);
    if (indexAfterMapList > idx) {
      indexAfterMapList++;
    }
    if (restorePoint > idx) {
      restorePoint++;
    }
  }

  /**
   * If we're creating our first TypeList, then IdCreator has to call this method to
   * ensure it gets put into the correct place in the offsettable table.
   * This assumes TypeLists always come before StringDatas.
   */
  public Offsettable insertNewOffsettableAsFirstEverTypeList(RawDexObject item,
      RawDexFile rawDexFile) {
    // We find the first StringDataItem, the type lists will come before this.
    Log.info("Calling insertNewOffsettableAsFirstEverTypeList()");
    for (int i = 0; i < offsettableTable.size(); i++) {
      if (offsettableTable.get(i).getItem() instanceof StringDataItem) {
        Offsettable offsettable = new Offsettable(item, true);
        insertOffsettableAt(i, offsettable);
        addTypeListsToMapFile(rawDexFile, offsettable);
        return offsettable;
      }
    }
    Log.errorAndQuit("Could not find any StringDataItems to insert the type list before.");
    return null;
  }

  /**
   * If we're creating our first FieldId, then IdCreator has to call this method to
   * ensure it gets put into the correct place in the offsettable table.
   * This assumes FieldIds always come before MethodIds.
   */
  public Offsettable insertNewOffsettableAsFirstEverField(RawDexObject item,
      RawDexFile rawDexFile) {
    // We find the first MethodIdItem, the fields will come before this.
    Log.info("Calling insertNewOffsettableAsFirstEverField()");
    for (int i = 0; i < offsettableTable.size(); i++) {
      if (offsettableTable.get(i).getItem() instanceof MethodIdItem) {
        Offsettable offsettable = new Offsettable(item, true);
        insertOffsettableAt(i, offsettable);
        addFieldIdsToHeaderAndMapFile(rawDexFile, offsettable);
        return offsettable;
      }
    }
    Log.errorAndQuit("Could not find any MethodIdItems to insert the field before.");
    return null;
  }

  /**
   * If we're creating a new Item (such as FieldId, MethodId) that is placed into the
   * first position of the relevant ID table, then IdCreator has to call this method to
   * ensure it gets put into the correct place in the offsettable table.
   */
  public Offsettable insertNewOffsettableAsFirstOfType(RawDexObject item,
      RawDexFile rawDexFile) {
    Log.debug("Calling insertNewOffsettableAsFirstOfType()");
    int index = getOffsettableIndexForFirstItemType(item);
    if (index == -1) {
      Log.errorAndQuit("Could not find any object of class: " + item.getClass());
    }
    Offsettable offsettable = new Offsettable(item, true);
    insertOffsettableAt(index, offsettable);
    updateOffsetsInHeaderAndMapFile(rawDexFile, offsettable);
    return offsettable;
  }

  /**
   * IdCreator has to call this method when it creates a new IdItem, to make sure it
   * gets put into the correct place in the offsettable table. IdCreator should
   * provide the IdItem that should come before this new IdItem.
   */
  public Offsettable insertNewOffsettableAfter(RawDexObject item, RawDexObject itemBefore) {
    Log.debug("Calling insertNewOffsettableAfter()");
    int index = getOffsettableIndexForItem(itemBefore);
    if (index == -1) {
      Log.errorAndQuit("Did not find specified 'after' object in offsettable table.");
    }
    Offsettable offsettable = new Offsettable(item, true);
    insertOffsettableAt(index + 1, offsettable);
    return offsettable;
  }

  private int getOffsettableIndexForFirstItemType(RawDexObject item) {
    Class<?> itemClass = item.getClass();
    for (int i = 0; i < offsettableTable.size(); i++) {
      if (offsettableTable.get(i).getItem().getClass().equals(itemClass)) {
        return i;
      }
    }
    return -1;
  }

  private int getOffsettableIndexForItem(RawDexObject item) {
    for (int i = 0; i < offsettableTable.size(); i++) {
      if (offsettableTable.get(i).getItem() == item) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Given a RawDexObject, get the Offsettable that contains it.
   */
  public Offsettable getOffsettableForItem(RawDexObject item) {
    for (int i = 0; i < offsettableTable.size(); i++) {
      if (offsettableTable.get(i).getItem() == item) {
        return offsettableTable.get(i);
      }
    }
    return null;
  }
}
