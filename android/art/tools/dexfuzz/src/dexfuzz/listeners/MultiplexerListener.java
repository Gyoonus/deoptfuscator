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

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Handles situation where multiple Listeners are wanted, passes notifications
 * onto each Listener it is responsible for.
 */
public class MultiplexerListener extends BaseListener {

  private List<BaseListener> listeners;

  @Override
  public void setup() {
    listeners = new ArrayList<BaseListener>();
  }

  public void addListener(BaseListener listener) {
    listeners.add(listener);
    listener.setup();
  }

  @Override
  public void shutdown() {
    for (BaseListener listener : listeners) {
      listener.shutdown();
    }
  }

  @Override
  public void handleSuccessfulHostVerification() {
    for (BaseListener listener : listeners) {
      listener.handleSuccessfulHostVerification();
    }
  }

  @Override
  public void handleFailedHostVerification(ExecutionResult verificationResult) {
    for (BaseListener listener : listeners) {
      listener.handleFailedHostVerification(verificationResult);
    }
  }

  @Override
  public void handleFailedTargetVerification() {
    for (BaseListener listener : listeners) {
      listener.handleFailedTargetVerification();
    }
  }

  @Override
  public void handleIterationStarted(int iteration) {
    for (BaseListener listener : listeners) {
      listener.handleIterationStarted(iteration);
    }
  }

  @Override
  public void handleIterationFinished(int iteration) {
    for (BaseListener listener : listeners) {
      listener.handleIterationFinished(iteration);
    }
  }

  @Override
  public void handleTimeouts(List<Executor> timedOut, List<Executor> didNotTimeOut) {
    for (BaseListener listener : listeners) {
      listener.handleTimeouts(timedOut, didNotTimeOut);
    }
  }

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    for (BaseListener listener : listeners) {
      listener.handleDivergences(outputMap);
    }
  }

  @Override
  public void handleFuzzingFile(String inputFile) {
    for (BaseListener listener : listeners) {
      listener.handleFuzzingFile(inputFile);
    }
  }

  @Override
  public void handleSeed(long seed) {
    for (BaseListener listener : listeners) {
      listener.handleSeed(seed);
    }
  }

  @Override
  public void handleHostVerificationSigabort(ExecutionResult verificationResult) {
    for (BaseListener listener : listeners) {
      listener.handleHostVerificationSigabort(verificationResult);
    }
  }

  @Override
  public void handleSuccess(Map<String, List<Executor>> outputMap) {
    for (BaseListener listener : listeners) {
      listener.handleSuccess(outputMap);
    }
  }

  @Override
  public void handleDumpOutput(String outputLine, Executor executor) {
    for (BaseListener listener : listeners) {
      listener.handleDumpOutput(outputLine, executor);
    }
  }

  @Override
  public void handleDumpVerify(String verifyLine) {
    for (BaseListener listener : listeners) {
      listener.handleDumpVerify(verifyLine);
    }
  }

  @Override
  public void handleMutationStats(String statsString) {
    for (BaseListener listener : listeners) {
      listener.handleMutationStats(statsString);
    }
  }

  @Override
  public void handleTiming(String name, float elapsedTime) {
    for (BaseListener listener : listeners) {
      listener.handleTiming(name, elapsedTime);
    }
  }

  @Override
  public void handleMutationFail() {
    for (BaseListener listener : listeners) {
      listener.handleMutationFail();
    }
  }

  @Override
  public void handleSummary() {
    for (BaseListener listener : listeners) {
      listener.handleSummary();
    }
  }

  @Override
  public void handleSuccessfullyFuzzedFile(String programName) {
    for (BaseListener listener : listeners) {
      listener.handleSuccessfullyFuzzedFile(programName);
    }
  }

  @Override
  public void handleSelfDivergence() {
    for (BaseListener listener : listeners) {
      listener.handleSelfDivergence();
    }
  }

  @Override
  public void handleMessage(String msg) {
    for (BaseListener listener : listeners) {
      listener.handleMessage(msg);
    }
  }

  @Override
  public void handleMutations(List<Mutation> mutations) {
    for (BaseListener listener : listeners) {
      listener.handleMutations(mutations);
    }
  }

  @Override
  public void handleArchitectureSplit() {
    for (BaseListener listener : listeners) {
      listener.handleArchitectureSplit();
    }
  }
}
