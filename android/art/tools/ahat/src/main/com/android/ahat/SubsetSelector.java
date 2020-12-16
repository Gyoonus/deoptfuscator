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
 * The SubsetSelector is that can be added to a page that lets the
 * user select a limited number of elements to show.
 * This is used to limit the number of elements shown on a page by default,
 * requiring the user to explicitly request more, so users not interested in
 * more don't have to wait for everything to render.
 */
class SubsetSelector<T> {
  private static final int kIncrAmount = 1000;
  private static final int kDefaultShown = 1000;

  private Query mQuery;
  private String mId;
  private int mLimit;
  private List<T> mElements;

  /**
   * @param id - the name of the query parameter key that should hold
   * the limit selectors selected value.
   * @param query - The query for the current page. This is required so the
   * LimitSelector can add a link to the same page with modified limit
   * selection.
   * @param elements - the elements to select from. The collection of elements
   * should not be modified during the lifetime of the SubsetSelector object.
   */
  public SubsetSelector(Query query, String id, List<T> elements) {
    mQuery = query;
    mId = id;
    mLimit = getSelectedLimit(query, id, elements.size());
    mElements = elements;
  }

  // Return the list of elements included in the selected subset.
  public List<T> selected() {
    return mElements.subList(0, mLimit);
  }

  // Return the list of remaining elements not included in the selected subset.
  public List<T> remaining() {
    return mElements.subList(mLimit, mElements.size());
  }

  /**
   * Returns the currently selected limit.
   * @param query the current page query
   * @param size the total number of elements to select from
   * @return the number of selected elements
   */
  private static int getSelectedLimit(Query query, String id, int size) {
    String value = query.get(id, null);
    try {
      int ivalue = Math.min(size, Integer.parseInt(value));
      return Math.max(0, ivalue);
    } catch (NumberFormatException e) {
      // We can't parse the value as a number. Ignore it.
    }
    return Math.min(kDefaultShown, size);
  }

  // Render the limit selector to the given doc.
  // It has the form:
  //  (showing X of Y - show none - show less - show more - show all)
  public void render(Doc doc) {
    int all = mElements.size();
    if (all > kDefaultShown) {
      DocString menu = new DocString();
      menu.appendFormat("(%d of %d elements shown - ", mLimit, all);
      if (mLimit > 0) {
        int less = Math.max(0, mLimit - kIncrAmount);
        menu.appendLink(mQuery.with(mId, 0), DocString.text("show none"));
        menu.append(" - ");
        menu.appendLink(mQuery.with(mId, less), DocString.text("show less"));
        menu.append(" - ");
      } else {
        menu.append("show none - show less - ");
      }
      if (mLimit < all) {
        int more = Math.min(mLimit + kIncrAmount, all);
        menu.appendLink(mQuery.with(mId, more), DocString.text("show more"));
        menu.append(" - ");
        menu.appendLink(mQuery.with(mId, all), DocString.text("show all"));
        menu.append(")");
      } else {
        menu.append("show more - show all)");
      }
      doc.println(menu);
    }
  }
}
