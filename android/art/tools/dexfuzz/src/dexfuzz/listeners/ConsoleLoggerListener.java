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
import dexfuzz.executors.Executor;
import dexfuzz.program.Mutation;

import java.util.List;
import java.util.Map;

/**
 * Logs output to the console, when not using --repeat.
 */
public class ConsoleLoggerListener extends BaseListener {
  @Override
  public void setup() {

  }

  @Override
  public void shutdown() {

  }

  private void logToConsole(String msg) {
    System.out.println("CONSOLE: " + msg);
  }

  @Override
  public void handleSuccessfulHostVerification() {
    logToConsole("Successful host verification");
  }

  @Override
  public void handleFailedHostVerification(ExecutionResult verificationResult) {
    logToConsole("Failed host verification");
  }

  @Override
  public void handleMutations(List<Mutation> mutations) {
    for (Mutation mutation : mutations) {
      logToConsole("Applied mutation: " + mutation.toString());
    }
  }

  @Override
  public void handleArchitectureSplit() {
    logToConsole("Detected architectural split.");
  }

  @Override
  public void handleFailedTargetVerification() {
    logToConsole("Failed target verification");
  }

  @Override
  public void handleIterationStarted(int iteration) {
    logToConsole("Starting iteration " + iteration);
  }

  @Override
  public void handleIterationFinished(int iteration) {
    logToConsole("Finished iteration " + iteration);
  }

  @Override
  public void handleTimeouts(List<Executor> timedOut, List<Executor> didNotTimeOut) {
    logToConsole("Timed out: " + timedOut.size() + " Did not time out: " + didNotTimeOut.size());
  }

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    logToConsole("Got divergences!");
    int outputCount = 1;
    for (List<Executor> executors : outputMap.values()) {
      logToConsole("Output " + outputCount + ":");
      for (Executor executor : executors) {
        logToConsole("  " + executor.getName());
      }
      outputCount++;
    }
  }

  @Override
  public void handleFuzzingFile(String inputFile) {
    logToConsole("Fuzzing: " + inputFile);
  }

  @Override
  public void handleSeed(long seed) {
    logToConsole("Seed: " + seed);
  }

  @Override
  public void handleHostVerificationSigabort(ExecutionResult verificationResult) {
    logToConsole("Sigaborted host verification");
  }

  @Override
  public void handleSuccessfullyFuzzedFile(String programName) {
    logToConsole("Program " + programName + " successfully fuzzed.");
  }

  @Override
  public void handleSuccess(Map<String, List<Executor>> outputMap) {
    logToConsole("Execution was successful");
  }

  @Override
  public void handleDumpOutput(String outputLine, Executor executor) {
    logToConsole(executor.getName() + " OUTPUT: " + outputLine);
  }

  @Override
  public void handleDumpVerify(String verifyLine) {
    logToConsole("VERIFY: " + verifyLine);
  }

  @Override
  public void handleMutationFail() {
    logToConsole("DEX file was not mutated");
  }

  @Override
  public void handleMutationStats(String statsString) {
    logToConsole("Mutations performed: " + statsString);
  }

  @Override
  public void handleTiming(String name, float elapsedTime) {
    logToConsole(String.format("'%s': %.3fs", name, elapsedTime));
  }

  @Override
  public void handleSummary() {
    logToConsole("--- SUMMARY ---");
  }

  @Override
  public void handleSelfDivergence() {
    logToConsole("Seen self divergence");
  }

  @Override
  public void handleMessage(String msg) {
    logToConsole(msg);
  }
}
