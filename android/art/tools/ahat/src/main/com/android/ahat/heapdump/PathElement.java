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
 * A single element along a reference path from a GC root to an instance in
 * the heap dump.
 * <p>
 * For example, assuming object A is a root a path to some object X might look
 * like:
 * <pre>
 *   A.x --&gt; B.y --&gt; C.z --&gt; X
 * </pre>
 *
 * A path element is a single node of that path, such as <code>B.y</code>.
 * @see AhatInstance#getPathFromGcRoot
 */
public class PathElement implements Diffable<PathElement> {
  /**
   * The instance along the reference path that this PathElement is associated
   * with.
   */
  public final AhatInstance instance;

  /**
   * A human readable description of which field in <code>instance</code> is
   * followed to reach the next element in the path.
   * Some examples:
   * <ul>
   * <li> "mBlah" for a class instance
   * <li> "[4]" for an array instance
   * <li> "" for the last element of the path
   * </ul>
   */
  public final String field;

  /**
   * True if <code>instance</code> is a (not necessarily immediate) dominator
   * of the final object in the path.
   */
  public boolean isDominator;

  /**
   * Constructs a PathElement object.
   * <code>isDominator</code> is set to false.
   *
   * @param instance the path element instance
   * @param field the path element field
   */
  public PathElement(AhatInstance instance, String field) {
    this.instance = instance;
    this.field = field;
    this.isDominator = false;
  }

  @Override public PathElement getBaseline() {
    return this;
  }

  @Override public boolean isPlaceHolder() {
    return false;
  }
}
