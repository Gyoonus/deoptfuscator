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

import java.util.List;

/**
 * An interface for rendering a page of content to the user.
 */
interface Doc extends AutoCloseable {
  /**
   * Output the title of the page.
   */
  void title(String format, Object... args);

  /**
   * Print a line of text for a page menu.
   */
  void menu(DocString string);

  /**
   * Start a new section with the given title.
   */
  void section(String title);

  /**
   * Print a line of text in a normal font.
   */
  void println(DocString string);

  /**
   * Print a line of text in a large font that is easy to see and click on.
   */
  void big(DocString string);

  /**
   * Start a table with the given columns.
   *
   * An IllegalArgumentException is thrown if no columns are provided.
   *
   * This should be followed by calls to the 'row' method to fill in the table
   * contents and the 'end' method to end the table.
   */
  void table(Column... columns);

  /**
   * Start a table with the following heading structure:
   *   |  description  |  c2  | c3 | ... |
   *   | h1 | h2 | ... |      |    |     |
   *
   * Where subcols describes h1, h2, ...
   * and cols describes c2, c3, ...
   *
   * This should be followed by calls to the 'row' method to fill in the table
   * contents and the 'end' method to end the table.
   */
  void table(DocString description, List<Column> subcols, List<Column> cols);

  /**
   * Add a row to the currently active table.
   * The number of values must match the number of columns provided for the
   * currently active table.
   */
  void row(DocString... values);

  /**
   * Start a new description list.
   *
   * This should be followed by calls to description() and finally a call to
   * end().
   */
  void descriptions();

  /**
   * Add a description to the currently active description list.
   */
  void description(DocString key, DocString value);

  /**
   * End the currently active table or description list.
   */
  void end();
}
