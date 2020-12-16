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

package dexfuzz.listeners;

import dexfuzz.ExecutionResult;
import dexfuzz.Log;
import dexfuzz.executors.Executor;
import dexfuzz.program.Mutation;
import dexfuzz.program.MutationSerializer;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Date;
import java.util.List;
import java.util.Map;

/**
 * Logs events to a file.
 */
public class LogFileListener extends BaseListener {
  private BufferedWriter writer;
  boolean ready = false;

  long successfulVerification;
  long failedVerification;
  long failedMutation;
  long success;
  long timedOut;
  long divergence;
  long selfDivergent;
  long architectureSplit;
  long iterations;

  private String logFile;

  public LogFileListener(String logFile) {
    this.logFile = logFile;
  }

  @Override
  public void setup() {
    try {
      writer = new BufferedWriter(new FileWriter(logFile));
      ready = true;
    } catch (IOException e) {
      e.printStackTrace();
    }
  }

  @Override
  public void shutdown() {
    try {
      writer.close();
    } catch (IOException e) {
      e.printStackTrace();
    }
    Log.always("Full log in " + logFile);
  }

  private void write(String msg) {
    if (!ready) {
      return;
    }
    try {
      writer.write(msg + "\n");
    } catch (IOException e) {
      e.printStackTrace();
    }
  }

  @Override
  public void handleSuccessfulHostVerification() {
    write("Host verification: SUCCESS");
    successfulVerification++;
  }

  @Override
  public void handleFailedHostVerification(ExecutionResult verificationResult) {
    write("Host verification: FAILED");
    failedVerification++;
  }

  @Override
  public void handleFailedTargetVerification() {
    write("Target verification: FAILED");
    failedVerification++;
  }

  @Override
  public void handleIterationStarted(int iteration) {
    write("--> FUZZ " + (iteration + 1));
    Date now = new Date(System.currentTimeMillis());
    write("Time started: " + now.toString());
    iterations++;
  }

  @Override
  public void handleTimeouts(List<Executor> timedOut, List<Executor> didNotTimeOut) {
    write("Some executors timed out.");
    write("Timed out:");
    for (Executor executor : timedOut) {
      write("  " + executor.getName());
    }
    if (!didNotTimeOut.isEmpty()) {
      write("Did not time out:");
      for (Executor executor : didNotTimeOut) {
        write("  " + executor.getName());
      }
    }
    this.timedOut++;
  }

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    write("DIVERGENCE between some executors!");
    int outputCount = 1;
    for (List<Executor> executors : outputMap.values()) {
      write("Output " + outputCount + ":");
      for (Executor executor : executors) {
        write("  " + executor.getName());
      }
      outputCount++;
    }
    divergence++;

    // You are probably interested in reading about these divergences while fuzzing
    // is taking place, so flush the writer now.
    try {
      writer.flush();
    } catch (IOException e) {
      e.printStackTrace();
    }
  }

  @Override
  public void handleFuzzingFile(String inputFile) {
    write("Fuzzing file '" + inputFile + "'");
  }

  @Override
  public void handleSeed(long seed) {
    write("Using " + seed + " for seed.");
    // Flush the seed as well, so if anything goes wrong we can see what seed lead
    // to the issue.
    try {
      writer.flush();
    } catch (IOException e) {
      e.printStackTrace();
    }
  }

  @Override
  public void handleHostVerificationSigabort(ExecutionResult verificationResult) {
    write("Host verification: SIGABORTED");
  }

  @Override
  public void handleSuccess(Map<String, List<Executor>> outputMap) {
    write("All executors agreed on result.");
    success++;
  }

  @Override
  public void handleDumpOutput(String outputLine, Executor executor) {
    write(executor.getName() + " OUTPUT:");
    write(outputLine);
  }

  @Override
  public void handleDumpVerify(String verifyLine) {
    write("VERIFY: " + verifyLine);
  }

  @Override
  public void handleMutationStats(String statsString) {
    write("Mutation Stats: " + statsString);
  }

  @Override
  public void handleTiming(String name, float elapsedTime) {
    write(String.format("'%s': %.3fs", name, elapsedTime));
  }

  @Override
  public void handleMutationFail() {
    write("Mutation process: FAILED");
    failedMutation++;
  }

  @Override
  public void handleSummary() {
    write("");
    write("---+++--- SUMMARY ---+++---");
    write("Fuzzing attempts: " + iterations);
    write("  Failed verification: " + failedVerification);
    write("  Failed mutation: " + failedMutation);
    write("  Timed out: " + timedOut);
    write("Successful: " + success);
    write("  Self divergent: " + selfDivergent);
    write("  Architecture split: " + architectureSplit);
    write("Produced divergence: " + divergence);

    long truelyDivergent = divergence - (selfDivergent + architectureSplit);
    long verified = success + timedOut + truelyDivergent;

    write("");

    float verifiedTotalRatio =
        (((float) (verified)) / iterations) * 100.0f;
    write(String.format("Percentage Verified/Total: %.3f%%", verifiedTotalRatio));

    float timedOutVerifiedRatio =
        (((float) timedOut) / (verified)) * 100.0f;
    write(String.format("Percentage Timed Out/Verified: %.3f%%", timedOutVerifiedRatio));

    float successfulVerifiedRatio =
        (((float) success) / (verified)) * 100.0f;
    write(String.format("Percentage Successful/Verified: %.3f%%", successfulVerifiedRatio));

    float divergentVerifiedRatio =
        (((float) truelyDivergent) / (verified)) * 100.0f;
    write(String.format("Percentage Divergent/Verified: %.3f%%", divergentVerifiedRatio));

    write("---+++--- SUMMARY ---+++---");
    write("");
  }

  @Override
  public void handleIterationFinished(int iteration) {
    write("");
  }

  @Override
  public void handleSuccessfullyFuzzedFile(String programName) {
    write("Successfully fuzzed file '" + programName + "'");
  }

  @Override
  public void handleSelfDivergence() {
    write("Golden Executor was self-divergent!");
    selfDivergent++;
  }

  @Override
  public void handleArchitectureSplit() {
    write("Divergent outputs align with difference in architectures.");
    architectureSplit++;
  }

  @Override
  public void handleMessage(String msg) {
    write(msg);
  }

  @Override
  public void handleMutations(List<Mutation> mutations) {
    write("Mutations Report");
    for (Mutation mutation : mutations) {
      write(MutationSerializer.getMutationString(mutation));
    }
  }
}
