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
 * An interface for instances/sites/heaps/etc in a heap dump that can be
 * related to corresponding instances/sites/heaps/etc in a second heap dump
 * when the two heap dumps have been diffed.
 */
public interface Diffable<T> {
  /**
   * Returns the object in the other heap dump that corresponds to this object.
   * When two heap dumps are diffed, diffable objects from the first heap dump
   * will be matched to "baseline" objects from the second heap dump, and
   * diffable objects from the second heap dump will be matched to "baseline"
   * objects from the first heap dump.
   *
   * @return the matched object from the other heap dump
   */
  T getBaseline();

  /**
   * Returns true if this is a placeholder object.
   * A placeholder object is used to indicate there is some object in the
   * baseline heap dump that is not in this heap dump. In that case, we create
   * a dummy place holder object in this heap dump as an indicator of the
   * object removed from the baseline heap dump.
   *
   * @return true if the object is a placeholder
   */
  boolean isPlaceHolder();
}

