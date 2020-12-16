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

import java.util.List;
import java.util.Map;

/**
 * Implements the live updating table of results when --repeat is being used.
 */
public class UpdatingConsoleListener extends BaseListener {
  long successfulVerification;
  long failedVerification;
  long failedMutation;
  long success;
  long timedOut;
  long divergence;
  long selfDivergent;
  long architectureSplit;
  long iterations;

  @Override
  public void setup() {
    System.out.println("|-----------------------------------------------------------------|");
    System.out.println("|Iterations|VerifyFail|MutateFail|Timed Out |Successful|Divergence|");
    System.out.println("|-----------------------------------------------------------------|");
  }

  @Override
  public void handleSuccessfulHostVerification() {
    successfulVerification++;
  }

  @Override
  public void handleFailedHostVerification(ExecutionResult verificationResult) {
    failedVerification++;
  }

  @Override
  public void handleFailedTargetVerification() {
    failedVerification++;
  }

  @Override
  public void handleIterationStarted(int iteration) {
    iterations++;
  }

  @Override
  public void handleIterationFinished(int iteration) {
    String output = String.format("| %-9d| %-9d| %-9d| %-9d| %-9d| %-9d|",
        iterations, failedVerification, failedMutation, timedOut, success,
        divergence - (selfDivergent + architectureSplit));
    System.out.print("\r" + output);
  }

  @Override
  public void handleTimeouts(List<Executor> timedOut, List<Executor> didNotTimeOut) {
    this.timedOut++;
  }

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

  @Override
  public void handleSuccess(Map<String, List<Executor>> outputMap) {
    success++;
  }

  @Override
  public void handleMutationFail() {
    failedMutation++;
  }

  @Override
  public void handleSummary() {
    System.out.println("\n|-----------------------------------------------------------------|");
  }
}
