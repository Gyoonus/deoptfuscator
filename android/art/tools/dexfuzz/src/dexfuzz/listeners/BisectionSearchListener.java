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

import dexfuzz.ExecutionResult;
import dexfuzz.executors.Executor;
import dexfuzz.Log;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Runs bisection search for divergent programs.
 */
public class BisectionSearchListener extends BaseListener {

  /**
   * Used to remember the seed used to fuzz the fuzzed file, so we can save it with this
   * seed as a name, if we find a divergence.
   */
  private long currentSeed;

  /**
   * Used to remember the name of the file we've fuzzed, so we can save it if we
   * find a divergence.
   */
  private String fuzzedFile;

  @Override
  public void handleSeed(long seed) {
    currentSeed = seed;
  }

  @Override
  public void handleSuccessfullyFuzzedFile(String programName) {
    fuzzedFile = programName;
  }

  private void writeToFile(String file, String toWrite) throws IOException {
    PrintWriter writer = new PrintWriter(file);
    writer.write(toWrite);
    writer.close();
  }

  private String extractExpectedOutput(ExecutionResult result) {
    StringBuilder builder = new StringBuilder();
    // Skip last, artificial output line with return code.
    for (int i = 0; i < result.output.size() - 1; i++) {
      builder.append(result.output.get(i)).append("\n");
    }
    return builder.toString();
  }

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    if (outputMap.size() != 2) {
      // It's unclear which output should be considered reference output.
      return;
    }
    try {
      File expected_output_file = File.createTempFile("expected_output", ".txt");
      String outputFile = String.format("bisection_outputs/%d_out.txt", currentSeed);
      String logFile = String.format("bisection_outputs/%d_log.txt", currentSeed);
      List<List<Executor>> executorsGroupedByOutput =
          new ArrayList<List<Executor>>(outputMap.values());
      List<String> outputs = new ArrayList<String>();
      for (List<Executor> executors : executorsGroupedByOutput) {
        outputs.add(extractExpectedOutput(executors.get(0).getResult()));
      }
      for (int i = 0; i < 2; i++) {
        String output = outputs.get(i);
        String otherOutput = outputs.get(1 - i);
        List<Executor> executors = executorsGroupedByOutput.get(i);
        for (Executor executor : executors) {
          if (executor.isBisectable()) {
            writeToFile(expected_output_file.getAbsolutePath(), otherOutput);
            ExecutionResult result = executor.runBisectionSearch(fuzzedFile,
                expected_output_file.getAbsolutePath(), logFile);
            writeToFile(outputFile, result.getFlattenedAllWithNewlines());
          }
        }
      }
      expected_output_file.delete();
    } catch (IOException e) {
      Log.error(
          "BisectionSearchListener.handleDivergences() caught an IOException " + e.toString());
    }
  }

}
