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

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Semaphore;

/**
 * process.waitFor() can block if its output buffers are not drained.
 * These threads are used to keep the buffers drained, and provide the final
 * output once the command has finished executing. Each Executor has its own
 * output and error StreamConsumers.
 */
public class StreamConsumer extends Thread {
  private List<String> output;
  private BufferedReader reader;

  private State state;

  private Semaphore workToBeDone;
  private Semaphore outputIsReady;

  enum State {
    WAITING,
    CONSUMING,
    SHOULD_STOP_CONSUMING,
    FINISHED,
    ERROR
  }

  /**
   * Create a StreamConsumer, will be immediately ready to start consuming.
   */
  public StreamConsumer() {
    output = new ArrayList<String>();
    workToBeDone = new Semaphore(0);
    outputIsReady = new Semaphore(0);

    state = State.WAITING;
  }

  /**
   * Executor should call this to provide its StreamConsumers with the Streams
   * for a Process it is about to call waitFor() on.
   */
  public void giveStreamAndStartConsuming(InputStream stream) {
    output.clear();

    reader = new BufferedReader(new InputStreamReader(stream));

    changeState(State.CONSUMING, State.WAITING);

    // Tell consumer there is work to be done.
    workToBeDone.release();
  }

  /**
   * Executor should call this once its call to waitFor() returns.
   */
  public void processFinished() {
    changeState(State.SHOULD_STOP_CONSUMING, State.CONSUMING);
  }

  /**
   * Executor should call this to get the captured output of this StreamConsumer.
   */
  public List<String> getOutput() {

    try {
      // Wait until the output is ready.
      outputIsReady.acquire();
    } catch (InterruptedException e) {
      Log.error("Client of StreamConsumer was interrupted while waiting for output?");
      return null;
    }

    // Take a copy of the Strings, so when we call output.clear(), we don't
    // clear the ExecutionResult's list.
    List<String> copy = new ArrayList<String>(output);
    return copy;
  }

  /**
   * Executor should call this when we're shutting down.
   */
  public void shutdown() {
    changeState(State.FINISHED, State.WAITING);

    // Tell Consumer there is work to be done (it will check first if FINISHED has been set.)
    workToBeDone.release();
  }

  private void consume() {
    try {

      if (checkState(State.SHOULD_STOP_CONSUMING)) {
        // Caller already called processFinished() before we even started
        // consuming. Just get what we can and finish.
        while (reader.ready()) {
          output.add(reader.readLine());
        }
      } else {
        // Caller's process is still executing, so just loop and consume.
        while (checkState(State.CONSUMING)) {
          Thread.sleep(50);
          while (reader.ready()) {
            output.add(reader.readLine());
          }
        }
      }

      if (checkState(State.SHOULD_STOP_CONSUMING)) {
        changeState(State.WAITING, State.SHOULD_STOP_CONSUMING);
      } else {
        Log.error("StreamConsumer stopped consuming, but was not told to?");
        setErrorState();
      }

      reader.close();

    } catch (IOException e) {
      Log.error("StreamConsumer caught IOException while consuming");
      setErrorState();
    } catch (InterruptedException e) {
      Log.error("StreamConsumer caught InterruptedException while consuming");
      setErrorState();
    }

    // Tell client of Consumer that the output is ready.
    outputIsReady.release();
  }

  @Override
  public void run() {
    while (checkState(State.WAITING)) {
      try {
        // Wait until there is work to be done
        workToBeDone.acquire();
      } catch (InterruptedException e) {
        Log.error("StreamConsumer caught InterruptedException while waiting for work");
        setErrorState();
        break;
      }

      // Check first if we're done
      if (checkState(State.FINISHED)) {
        break;
      }

      // Make sure we're either supposed to be consuming
      // or supposed to be finishing up consuming
      if (!(checkState(State.CONSUMING) || checkState(State.SHOULD_STOP_CONSUMING))) {
        Log.error("invalid state: StreamConsumer told about work, but not CONSUMING?");
        Log.error("state was: " + getCurrentState());
        setErrorState();
        break;
      }

      consume();
    }
  }

  private synchronized boolean checkState(State expectedState) {
    return (expectedState == state);
  }

  private synchronized void changeState(State newState, State previousState) {
    if (state != previousState) {
      Log.error("StreamConsumer Unexpected state: " + state + ", expected " + previousState);
      state = State.ERROR;
    } else {
      state = newState;
    }
  }

  private synchronized void setErrorState() {
    state = State.ERROR;
  }

  private synchronized State getCurrentState() {
    return state;
  }
}
