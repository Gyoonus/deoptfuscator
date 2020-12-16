/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.ahat.heapdump;

/**
 * Used to identify and access basic information about a particular
 * heap from the heap dump. Standard Java heap dumps have a single heap,
 * called the "default" heap. Android heap dumps distinguish among "zygote",
 * "image", and "app" heaps. There will be a single instance of AhatHeap for
 * each different heap in the heap dump.
 */
public class AhatHeap implements Diffable<AhatHeap> {
  private String mName;
  private Size mSize = Size.ZERO;
  private int mIndex;
  private AhatHeap mBaseline;
  private boolean mIsPlaceHolder = false;

  AhatHeap(String name, int index) {
    mName = name;
    mIndex = index;
    mBaseline = this;
  }

  /**
   * Construct a place holder heap.
   */
  private AhatHeap(String name, AhatHeap baseline) {
    mName = name;
    mIndex = -1;
    mBaseline = baseline;
    baseline.setBaseline(this);
    mIsPlaceHolder = true;
  }

  /**
   * Construct a new place holder heap that has the given baseline heap.
   */
  static AhatHeap newPlaceHolderHeap(String name, AhatHeap baseline) {
    return new AhatHeap(name, baseline);
  }

  void addToSize(Size size) {
    mSize = mSize.plus(size);
  }

  /**
   * Returns a unique instance for this heap between 0 and the total number of
   * heaps in this snapshot, or -1 if this is a placeholder heap.
   */
  int getIndex() {
    return mIndex;
  }

  /**
   * Returns the name of this heap.
   * For example, "default", "app", "image", or "zygote".
   *
   * @return The name of the heap.
   */
  public String getName() {
    return mName;
  }

  /**
   * Returns the total number of bytes allocated on this heap.
   *
   * @return the total number of bytes allocated on this heap.
   */
  public Size getSize() {
    return mSize;
  }

  void setBaseline(AhatHeap baseline) {
    mBaseline = baseline;
  }

  @Override
  public AhatHeap getBaseline() {
    return mBaseline;
  }

  @Override
  public boolean isPlaceHolder() {
    return mIsPlaceHolder;
  }
}
