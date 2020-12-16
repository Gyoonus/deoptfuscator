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

package dexfuzz;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A wrapper for a dictionary tracking what mutations have been performed.
 */
public class MutationStats {

  public static class StatNotFoundException extends RuntimeException {
    private static final long serialVersionUID = -7038515184655168470L;
  }

  private Map<String,Long> stats;
  private List<String> statsOrder;

  public MutationStats() {
    stats = new HashMap<String,Long>();
    statsOrder = new ArrayList<String>();
  }

  public void incrementStat(String statName) {
    increaseStat(statName, 1);
  }

  /**
   * Increase the named stat by the specified amount.
   */
  public void increaseStat(String statName, long amt) {
    if (!stats.containsKey(statName)) {
      stats.put(statName, 0L);
      statsOrder.add(statName);
    }
    stats.put(statName, stats.get(statName) + amt);
  }

  /**
   * Get a string representing the collected stats - looks like a JSON dictionary.
   */
  public String getStatsString() {
    StringBuilder builder = new StringBuilder();
    builder.append("{");
    boolean first = true;
    for (String statName : statsOrder) {
      if (!first) {
        builder.append(", ");
      } else {
        first = false;
      }
      builder.append("\"").append(statName).append("\": ").append(stats.get(statName));
    }
    builder.append("}");
    return builder.toString();
  }
}
