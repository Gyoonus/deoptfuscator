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

package dexfuzz.listeners;

import java.util.List;
import java.util.Map;

import dexfuzz.executors.Executor;

/**
 * Counts divergences as they appear and checks if the testing was successful
 * or not. Testing is successful if all divergences found are either self
 * divergent or caused by differences in architectures.
 */
public class FinalStatusListener extends BaseListener {
  private long divergence;
  private long selfDivergent;
  private long architectureSplit;

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    divergence++;
  }

  @Override
  public void handleSelfDivergence() {
    selfDivergent++;
  }

  @Override
  public void handleArchitectureSplit() {
    architectureSplit++;
  }

  public boolean isSuccessful() {
    return (divergence - selfDivergent - architectureSplit) == 0;
  }
}
