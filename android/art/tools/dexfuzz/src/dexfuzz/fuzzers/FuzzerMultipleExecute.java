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
import dexfuzz.program.Program;

/**
 * Fuzz programs multiple times, testing each.
 */
public class FuzzerMultipleExecute extends FuzzerMultiple {
  public FuzzerMultipleExecute(BaseListener listener) {
    super(listener);
    addExecutors();
  }

  @Override
  protected String getNextOutputFilename() {
    // In MultipleExecute, always use the same output.
    return Options.outputFile;
  }

  @Override
  public void run() {
    // TODO: Test that all seed files execute correctly before they are mutated!
    for (iterations = 0; iterations < Options.repeat; iterations++) {
      listener.handleIterationStarted(iterations);
      Program program = fuzz();
      if (safeToExecute()) {
        execute(program);
      }
      listener.handleIterationFinished(iterations);
    }
    listener.handleSummary();
  }
}
