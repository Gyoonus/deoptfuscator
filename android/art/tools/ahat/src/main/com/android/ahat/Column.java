/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.ahat;

/**
 * Configuration of a Doc table column.
 */
class Column {
  public DocString heading;
  public Align align;
  public boolean visible;

  public static enum Align {
    LEFT, RIGHT
  };

  public Column(DocString heading, Align align, boolean visible) {
    this.heading = heading;
    this.align = align;
    this.visible = visible;
  }

  public Column(String heading, Align align, boolean visible) {
    this(DocString.text(heading), align, visible);
  }

  public Column(DocString heading, Align align) {
    this(heading, align, true);
  }

  /**
   * Construct a left-aligned column with a simple heading.
   */
  public Column(String heading) {
    this(DocString.text(heading), Align.LEFT);
  }

  /**
   * Construct a column with a simple heading.
   */
  public Column(String heading, Align align) {
    this(DocString.text(heading), align);
  }
}
