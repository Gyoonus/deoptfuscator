/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Provides a routine for diffing two collections of static or instance
 * fields.
 */
public class DiffFields {
  /**
   * Returns the result of diffing two collections of field values.
   *
   * @param current a list of fields in the current heap dump
   * @param baseline a list of fields in the baseline heap dump
   * @return list of diffed fields
   */
  public static List<DiffedFieldValue> diff(Iterable<FieldValue> current,
                                            Iterable<FieldValue> baseline) {
    List<FieldValue> currentSorted = new ArrayList<FieldValue>();
    for (FieldValue field : current) {
      currentSorted.add(field);
    }
    Collections.sort(currentSorted, FOR_DIFF);

    List<FieldValue> baselineSorted = new ArrayList<FieldValue>();
    for (FieldValue field : baseline) {
      baselineSorted.add(field);
    }
    Collections.sort(baselineSorted, FOR_DIFF);

    // Merge the two lists to form the diffed list of fields.
    List<DiffedFieldValue> diffed = new ArrayList<DiffedFieldValue>();
    int currentPos = 0;
    int baselinePos = 0;

    while (currentPos < currentSorted.size() && baselinePos < baselineSorted.size()) {
      FieldValue currentField = currentSorted.get(currentPos);
      FieldValue baselineField = baselineSorted.get(baselinePos);
      int compare = FOR_DIFF.compare(currentField, baselineField);
      if (compare < 0) {
        diffed.add(DiffedFieldValue.added(currentField));
        currentPos++;
      } else if (compare == 0) {
        diffed.add(DiffedFieldValue.matched(currentField, baselineField));
        currentPos++;
        baselinePos++;
      } else {
        diffed.add(DiffedFieldValue.deleted(baselineField));
        baselinePos++;
      }
    }

    while (currentPos < currentSorted.size()) {
      FieldValue currentField = currentSorted.get(currentPos);
      diffed.add(DiffedFieldValue.added(currentField));
      currentPos++;
    }

    while (baselinePos < baselineSorted.size()) {
      FieldValue baselineField = baselineSorted.get(baselinePos);
      diffed.add(DiffedFieldValue.deleted(baselineField));
      baselinePos++;
    }
    return diffed;
  }

  /**
   * Comparator used for sorting fields for the purposes of diff.
   * Fields with the same name and type are considered comparable, so we sort
   * by field name and type.
   */
  private static final Comparator<FieldValue> FOR_DIFF
    = Sort.withPriority(Sort.FIELD_VALUE_BY_NAME, Sort.FIELD_VALUE_BY_TYPE);
}
