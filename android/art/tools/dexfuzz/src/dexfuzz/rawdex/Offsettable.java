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

/**
 * Tracks the original and updated positions of a RawDexObject when it is
 * parsed in from a DEX file, and written out to a mutated DEX file.
 */
public class Offsettable {
  /**
   * The position of this Offsettable's item when it was read in.
   */
  private int originalPosition;

  /**
   * Set as we write out any Offsettable, so the Offset knows what its
   * new value should be.
   */
  private int newPosition;

  /**
   * The actual Item this Offsettable contains.
   */
  private RawDexObject item;

  /**
   *  Set either when getOriginalPosition() is called by the OffsetTracker
   *  to put the location in the offsettable map, so when Offsets are being
   *  associated, they know which Offsettable to point at.
   *  Or when an Offsettable is created that is marked as new, so we don't
   *  need to know its original position, because an Offset will be directly
   *  associated with it.
   */
  private boolean originalPositionKnown;

  /**
   * Set when we calculate the new position of this Offsettable as the file is
   * being output.
   */
  private boolean updated;

  /**
   * Only the OffsetTracker should be able to create a new Offsettable.
   */
  public Offsettable(RawDexObject item, boolean isNew) {
    this.item = item;
    if (isNew) {
      // We no longer care about the original position of the Offsettable, because
      // we are at the stage where we manually point Offsets at Offsettables, and
      // don't need to use the OffsetTracker's offsettable map.
      // So just lie and say we know it now.
      originalPositionKnown = true;
    }
  }

  public RawDexObject getItem() {
    return item;
  }

  /**
   * Gets the offset from the beginning of the file to the RawDexObject this Offsettable
   * contains, when the file was originally read.
   * Called when we're associating Offsets with Offsettables using the OffsetTracker's
   * offsettable map.
   */
  public int getOriginalPosition() {
    if (!originalPositionKnown) {
      throw new Error("Cannot get the original position of an Offsettable when not yet set.");
    }
    return originalPosition;
  }

  public void setOriginalPosition(int pos) {
    originalPosition = pos;
    originalPositionKnown = true;
  }

  /**
   * Get the new position of this Offsettable, once it's been written out to the output file.
   */
  public int getNewPosition() {
    if (!updated) {
      throw new Error("Cannot request new position before it has been set!");
    }
    return newPosition;
  }

  /**
   * Record the new position of this Offsettable, as it is written out to the output file.
   */
  public void setNewPosition(int pos) {
    if (!updated) {
      newPosition = pos;
      updated = true;
    } else {
      throw new Error("Cannot update an Offsettable twice!");
    }
  }

  public boolean readyForFinalOffsetToBeWritten() {
    return (originalPositionKnown && updated);
  }
}
