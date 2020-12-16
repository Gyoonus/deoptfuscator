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

public class Offset {
  /**
   * The absolute value of this offset as it was originally read.
   */
  private int originalOffset;

  /**
   * The Offsettable that this Offset points to.
   */
  private Offsettable offsettable;

  /**
   * The location of this Offset in the new file, ONLY SET IF the Offset
   * couldn't be written because what it points to hasn't been written
   * yet.
   */
  private int outputLocation;

  /**
   * Was the output location for this Offset set?.
   */
  private boolean outputLocationSet;

  /**
   * Does this Offset need to be written out using ULEB128?.
   */
  private boolean useUleb128;

  /**
   * Was this Offset created after reading, during mutation?.
   */
  private boolean isNewOffset;

  /**
   * Only one Offset should have this flag set, the MapItem that points
   * to the HeaderItem.
   */
  private boolean pointsAtHeader;

  /**
   * If an Offset pointed at 0 (because it is not actually a valid Offset),
   * and it's not pointing at the header, then this is set.
   */
  private boolean pointsAtNull;

  public Offset(boolean header) {
    pointsAtHeader = header;
  }

  public RawDexObject getPointedToItem() {
    return offsettable.getItem();
  }

  public boolean pointsToSomething() {
    return offsettable != null;
  }

  public boolean pointsAtNull() {
    return pointsAtNull;
  }

  public boolean pointsAtHeader() {
    return pointsAtHeader;
  }

  /**
   * Returns true if this Offset points at the provided RawDexObject.
   */
  public boolean pointsToThisItem(RawDexObject thisItem) {
    if (!pointsToSomething()) {
      return false;
    }
    return (offsettable.getItem().equals(thisItem));
  }

  /**
   * Returns true if this Offset points at the provided Offsettable.
   */
  public boolean pointsToThisOffsettable(Offsettable thisOffsettable) {
    if (!pointsToSomething()) {
      return false;
    }
    return (offsettable.equals(thisOffsettable));
  }

  /**
   * Makes this Offset point at a new Offsettable.
   */
  public void pointTo(Offsettable offsettableItem) {
    if (offsettable != null) {
      Log.debug("Updating what an Offset points to...");
    }
    offsettable = offsettableItem;
  }

  /**
   * Call this to make an Offset that pointed at null before now point at something.
   * An Offset may have previously pointed at null before...
   * Example: if there are no fields referred to in a DEX file, then header.field_ids_off
   * will point at null. We distinguish when Offsets point at null, and are not pointing
   * at the header (only the header MapItem should do this) with a flag. Therefore, this
   * method is needed to indicate that this Offset now points at something.
   */
  public void unsetNullAndPointTo(Offsettable offsettableItem) {
    pointsAtNull = false;
    if (offsettable != null) {
      Log.debug("Updating what an Offset points to...");
    }
    offsettable = offsettableItem;
  }

  public void pointToNew(Offsettable offsettableItem) {
    offsettable = offsettableItem;
    isNewOffset = true;
  }

  public int getNewPositionOfItem() {
    return offsettable.getNewPosition();
  }

  public boolean usesUleb128() {
    return useUleb128;
  }

  /**
   * Mark this Offset as using the ULEB128 encoding.
   */
  public void setUsesUleb128() {
    if (useUleb128) {
      throw new Error("Offset is already marked as using ULEB128!");
    }
    useUleb128 = true;
  }

  public boolean isNewOffset() {
    return isNewOffset;
  }

  public void setPointsAtNull() {
    pointsAtNull = true;
  }

  public void setOutputLocation(int loc) {
    outputLocation = loc;
    outputLocationSet = true;
  }

  /**
   * Get the location in the output DEX file where this offset has been written.
   * (This is used when patching Offsets when the Offsettable position was not
   * known at the time of writing out the Offset.)
   */
  public int getOutputLocation() {
    if (!outputLocationSet) {
      throw new Error("Output location was not set yet!");
    }
    return outputLocation;
  }

  public void setOriginalOffset(int offset) {
    originalOffset = offset;
  }

  public int getOriginalOffset() {
    return originalOffset;
  }

  public boolean readyForWriting() {
    return offsettable.readyForFinalOffsetToBeWritten();
  }
}
