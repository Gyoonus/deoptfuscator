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
 * Base class for Listeners, who are notified about certain events in dexfuzz's execution.
 */
public abstract class BaseListener {
  public void setup() { }

  public void shutdown() { }

  public void handleSuccessfulHostVerification() { }

  public void handleFailedHostVerification(ExecutionResult verificationResult) { }

  public void handleFailedTargetVerification() { }

  public void handleIterationStarted(int iteration) { }

  public void handleIterationFinished(int iteration) { }

  public void handleTimeouts(List<Executor> timedOut, List<Executor> didNotTimeOut) { }

  public void handleDivergences(Map<String, List<Executor>> outputMap) { }

  public void handleFuzzingFile(String inputFile) { }

  public void handleSeed(long seed) { }

  public void handleHostVerificationSigabort(ExecutionResult verificationResult) { }

  public void handleSuccess(Map<String, List<Executor>> outputMap) { }

  public void handleDumpOutput(String outputLine, Executor executor) { }

  public void handleDumpVerify(String verifyLine) { }

  public void handleMutationStats(String statsString) { }

  public void handleTiming(String name, float elapsedTime) { }

  public void handleMutationFail() { }

  public void handleSummary() { }

  public void handleSuccessfullyFuzzedFile(String programName) { }

  public void handleSelfDivergence() { }

  public void handleMessage(String msg) { }

  public void handleMutations(List<Mutation> mutations) { }

  public void handleArchitectureSplit() { }
}
