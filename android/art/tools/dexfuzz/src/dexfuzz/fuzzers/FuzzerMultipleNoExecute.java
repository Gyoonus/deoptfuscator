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

package dexfuzz.fuzzers;

import dexfuzz.Options;
import dexfuzz.listeners.BaseListener;

/**
 * Fuzz programs multiple times, writing each one to a new DEX file.
 */
public class FuzzerMultipleNoExecute extends FuzzerMultiple {
  public FuzzerMultipleNoExecute(BaseListener listener) {
    super(listener);
  }

  @Override
  protected String getNextOutputFilename() {
    // In MultipleNoExecute, produce multiple files, each prefixed
    // with the iteration value.
    return String.format("%09d_%s", iterations, Options.outputFile);
  }

  @Override
  public void run() {
    for (iterations = 0; iterations < Options.repeat; iterations++) {
      listener.handleIterationStarted(iterations);
      fuzz();
      listener.handleIterationFinished(iterations);
    }
    listener.handleSummary();
  }
}
