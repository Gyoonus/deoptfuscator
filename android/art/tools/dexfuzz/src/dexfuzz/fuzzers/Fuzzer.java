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

import dexfuzz.Log;
import dexfuzz.Options;
import dexfuzz.Timer;
import dexfuzz.executors.Architecture;
import dexfuzz.executors.Arm64InterpreterExecutor;
import dexfuzz.executors.Arm64OptimizingBackendExecutor;
import dexfuzz.executors.ArmInterpreterExecutor;
import dexfuzz.executors.ArmOptimizingBackendExecutor;
import dexfuzz.executors.Device;
import dexfuzz.executors.Executor;
import dexfuzz.executors.Mips64InterpreterExecutor;
import dexfuzz.executors.Mips64OptimizingBackendExecutor;
import dexfuzz.executors.MipsInterpreterExecutor;
import dexfuzz.executors.MipsOptimizingBackendExecutor;
import dexfuzz.executors.X86InterpreterExecutor;
import dexfuzz.executors.X86OptimizingBackendExecutor;
import dexfuzz.executors.X86_64InterpreterExecutor;
import dexfuzz.executors.X86_64OptimizingBackendExecutor;
import dexfuzz.listeners.BaseListener;
import dexfuzz.program.Mutation;
import dexfuzz.program.Program;
import dexfuzz.rawdex.DexRandomAccessFile;
import dexfuzz.rawdex.OffsetTracker;
import dexfuzz.rawdex.RawDexFile;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A particular fuzzing strategy, this class provides the common methods
 * most fuzzing will involve, and subclasses override the run() method, to
 * employ a particular strategy.
 */
public abstract class Fuzzer {
  private List<Executor> executors;
  private OffsetTracker offsetTracker;

  /**
   * This is the executor that we use to test for self-divergent programs.
   */
  private Executor goldenExecutor;

  /*
   * These two flags are set during fuzz(), and then cleared at the end of execute().
   */
  private boolean mutatedSuccessfully;
  private boolean savedSuccessfully;

  private Timer totalTimer = new Timer("Total Time");
  private Timer timerDexInput = new Timer("DEX Input");
  private Timer timerProgGen = new Timer("Program Generation");
  private Timer timerMutation = new Timer("Mutation Time");
  private Timer timerDexOutput = new Timer("DEX Output");
  private Timer timerChecksumCalc = new Timer("Checksum Calculation");

  protected BaseListener listener;

  protected Fuzzer(BaseListener listener) {
    totalTimer.start();
    executors = new ArrayList<Executor>();
    this.listener = listener;
  }

  public abstract void run();

  protected abstract String getNextInputFilename();

  protected abstract String getNextOutputFilename();

  /**
   * Call this after fuzzer execution to print out timing results.
   */
  public void printTimingInfo() {
    totalTimer.stop();
    timerDexInput.printTime(listener);
    timerProgGen.printTime(listener);
    timerMutation.printTime(listener);
    timerDexOutput.printTime(listener);
    timerChecksumCalc.printTime(listener);
    totalTimer.printTime(listener);
  }

  /**
   * Make sure this is called to correctly shutdown each Executor's StreamConsumers.
   */
  public void shutdown() {
    if (executors != null) {
      for (Executor executor : executors) {
        executor.shutdown();
      }
    }
  }

  private void addExecutorsForArchitecture(Device device, Class<? extends Executor> optimizing,
      Class<? extends Executor> interpreter) {
    // NB: Currently OptimizingBackend MUST come immediately before same arch's Interpreter.
    // This is because intepreter execution relies on there being an OAT file already
    // created to produce correct debug information. Otherwise we will see
    // false-positive divergences.
    try {
      if (Options.useOptimizing) {
        Constructor<? extends Executor> constructor =
            optimizing.getConstructor(BaseListener.class, Device.class);
        executors.add(constructor.newInstance(listener, device));
      }
      if (Options.useInterpreter) {
        Constructor<? extends Executor> constructor =
            interpreter.getConstructor(BaseListener.class, Device.class);
        executors.add(constructor.newInstance(listener, device));
      }
    } catch (NoSuchMethodException e) {
      Log.errorAndQuit("Executor doesn't have correct constructor.");
    } catch (InstantiationException e) {
      Log.errorAndQuit("Executor couldn't be instantiated.");
    } catch (IllegalAccessException e) {
      Log.errorAndQuit("Executor couldn't be accessed.");
    } catch (IllegalArgumentException e) {
      Log.errorAndQuit("Invalid arguments to instantiation of Executor.");
    } catch (InvocationTargetException e) {
      Log.errorAndQuit("Instantiation of Executor threw an Exception!");
    }
  }

  protected void addExecutors() {
    Device device = null;
    if (Options.executeOnHost) {
      device = new Device();
    } else {
      device = new Device(Options.deviceName, Options.noBootImage);
    }

    if (Options.useArchArm64) {
      addExecutorsForArchitecture(device, Arm64OptimizingBackendExecutor.class,
          Arm64InterpreterExecutor.class);
    }

    if (Options.useArchArm) {
      addExecutorsForArchitecture(device, ArmOptimizingBackendExecutor.class,
          ArmInterpreterExecutor.class);
    }

    if (Options.useArchX86_64) {
      addExecutorsForArchitecture(device, X86_64OptimizingBackendExecutor.class,
          X86_64InterpreterExecutor.class);
    }

    if (Options.useArchX86) {
      addExecutorsForArchitecture(device, X86OptimizingBackendExecutor.class,
          X86InterpreterExecutor.class);
    }

    if (Options.useArchMips64) {
      addExecutorsForArchitecture(device, Mips64OptimizingBackendExecutor.class,
          Mips64InterpreterExecutor.class);
    }

    if (Options.useArchMips) {
      addExecutorsForArchitecture(device, MipsOptimizingBackendExecutor.class,
          MipsInterpreterExecutor.class);
    }

    // Add the first backend as the golden executor for self-divergence tests.
    goldenExecutor = executors.get(0);
  }

  /**
   * Called from each Fuzzer subclass that we can instantiate. Parses the program, fuzzes it,
   * and then saves it, if mutation was successful. We can use --skip-mutation to bypass
   * the mutation phase, if we wanted to verify that a test program itself works.
   */
  protected Program fuzz() {
    String inputFile = getNextInputFilename();
    Program program = loadProgram(inputFile, null);
    if (program == null) {
      Log.errorAndQuit("Problem loading seed file.");
    }
    // Mutate the program.
    if (!Options.skipMutation) {
      timerMutation.start();
      program.mutateTheProgram();

      mutatedSuccessfully = program.updateRawDexFile();
      timerMutation.stop();
      if (!mutatedSuccessfully) {
        listener.handleMutationFail();
      }
    } else {
      Log.info("Skipping mutation stage as requested.");
      mutatedSuccessfully = true;
    }
    if (mutatedSuccessfully) {
      savedSuccessfully = saveProgram(program, getNextOutputFilename());
    }
    return program;
  }

  protected boolean safeToExecute() {
    return mutatedSuccessfully && savedSuccessfully;
  }

  protected void execute(Program program) {
    if (!safeToExecute()) {
      Log.errorAndQuit("Your Fuzzer subclass called execute() "
          + "without checking safeToExecute()!");
    }

    String programName = getNextOutputFilename();
    boolean verified = true;

    if (!Options.skipHostVerify && !Options.executeOnHost) {
      verified = goldenExecutor.verifyOnHost(programName);
      if (verified) {
        listener.handleSuccessfulHostVerification();
      }
    }

    if (verified) {
      boolean skipAnalysis = false;

      for (Executor executor : executors) {
        executor.reset();
        executor.prepareProgramForExecution(programName);
        executor.execute(programName);
        if (!executor.didTargetVerify()) {
          listener.handleFailedTargetVerification();
          skipAnalysis = true;
          break;
        }
        // Results are saved in the executors until they reset, usually at the
        // next iteration.
      }

      if (!skipAnalysis) {
        listener.handleSuccessfullyFuzzedFile(programName);
        analyseResults(program, programName);
      }
    }

    goldenExecutor.finishedWithProgramOnDevice();
    mutatedSuccessfully = false;
    savedSuccessfully = false;
  }

  /**
   * Checks if the different outputs we observed align with different architectures.
   */
  private boolean checkForArchitectureSplit(Map<String, List<Executor>> outputMap) {
    if (outputMap.size() != 2) {
      // Cannot have a two-way split if we don't have 2 kinds of output.
      return false;
    }

    Architecture[] architectures = new Architecture[2];
    int archIdx = 0;

    // For each kind of output we saw, make sure they all
    // came from the same architecture.
    for (List<Executor> executorList : outputMap.values()) {
      architectures[archIdx] = executorList.get(0).getArchitecture();
      for (int execIdx = 1; execIdx < executorList.size(); execIdx++) {
        if (executorList.get(execIdx).getArchitecture() != architectures[archIdx]) {
          // Not every executor with this output shared the same architecture.
          return false;
        }
      }
      archIdx++;
    }

    // Now make sure that the two outputs we saw were different architectures.
    if (architectures[0] == architectures[1]) {
      return false;
    }
    return true;
  }

  private boolean checkGoldenExecutorForSelfDivergence(String programName) {
    // Run golden executor multiple times, make sure it always produces
    // the same output, otherwise report that it is self-divergent.

    // TODO: Instead, produce a list of acceptable outputs, and see if the divergent
    // outputs of the backends fall within this set of outputs.
    String seenOutput = null;
    for (int i = 0; i < Options.divergenceRetry + 1; i++) {
      goldenExecutor.reset();
      goldenExecutor.execute(programName);
      String output = goldenExecutor.getResult().getFlattenedOutput();
      if (seenOutput == null) {
        seenOutput = output;
      } else if (!seenOutput.equals(output)) {
        return true;
      }
    }
    return false;
  }

  private void analyseResults(Program program, String programName) {
    // Check timeouts.
    // Construct two lists of executors, those who timed out, and those who did not.
    // Report if we had some timeouts.
    List<Executor> timedOut = new ArrayList<Executor>();
    List<Executor> didNotTimeOut = new ArrayList<Executor>();
    for (Executor executor : executors) {
      if (executor.getResult().isTimeout()) {
        timedOut.add(executor);
      } else {
        didNotTimeOut.add(executor);
      }
    }
    if (!timedOut.isEmpty()) {
      listener.handleTimeouts(timedOut, didNotTimeOut);
      // Do not bother reporting divergence information.
      return;
    }

    // Check divergences.
    // Construct a map {output1: [executor that produced output1, ...], output2: [...]}
    // If the map has more than one output, we had divergence, report it.
    Map<String, List<Executor>> outputMap = new HashMap<String, List<Executor>>();
    for (Executor executor : executors) {
      String output = executor.getResult().getFlattenedOutput();
      if (Options.dumpOutput) {
        listener.handleDumpOutput(
            executor.getResult().getFlattenedOutputWithNewlines(), executor);
      }
      if (outputMap.containsKey(output)) {
        outputMap.get(output).add(executor);
      } else {
        List<Executor> newList = new ArrayList<Executor>();
        newList.add(executor);
        outputMap.put(output, newList);
      }
    }

    if (outputMap.size() > 1) {
      // Report that we had divergence.
      listener.handleDivergences(outputMap);
      listener.handleMutations(program.getMutations());
      // If we found divergences, try running the "golden executor"
      // a few times in succession, to see if the output it produces is different
      // from run to run. If so, then we're probably executing something with either:
      //  a) randomness
      //  b) timing-dependent code
      //  c) threads
      // So we will not consider it a "true" divergence, but still useful?
      if (checkGoldenExecutorForSelfDivergence(programName)) {
        listener.handleSelfDivergence();
        return;
      }
      // If we found divergences, try checking if the differences
      // in outputs align with differences in architectures.
      // For example, if we have: {Output1: [ARM, ARM], Output2: [ARM64, ARM64]}
      if (checkForArchitectureSplit(outputMap)) {
        listener.handleArchitectureSplit();
      }
    } else {
      // No problems with execution.
      listener.handleSuccess(outputMap);
    }
  }

  private Program loadProgram(String inputName, List<Mutation> mutations) {
    Program program = null;
    try {
      DexRandomAccessFile input = new DexRandomAccessFile(inputName, "r");
      offsetTracker = new OffsetTracker();
      input.setOffsetTracker(offsetTracker);
      // Read the raw DexFile
      RawDexFile rawDexFile = new RawDexFile();
      timerDexInput.start();
      rawDexFile.read(input);
      timerDexInput.stop();
      input.close();
      // Create the program view.
      timerProgGen.start();
      program = new Program(rawDexFile, mutations, listener);
      timerProgGen.stop();
    } catch (FileNotFoundException e) {
      Log.errorAndQuit("Couldn't open a file called " + inputName);
    } catch (IOException e) {
      Log.errorAndQuit("IOException when trying to load a DEX test file!");
    }
    return program;
  }

  private boolean saveProgram(Program program, String outputName) {
    boolean success = false;

    try {
      // Write out the results of mutation.
      DexRandomAccessFile output = new DexRandomAccessFile(outputName, "rw");
      output.setOffsetTracker(offsetTracker);
      // Delete the contents of the file, in case it already existed.
      output.setLength(0);
      // Write out the file.
      timerDexOutput.start();
      program.writeRawDexFile(output);
      timerDexOutput.stop();
      // Recalculate the header info.
      timerChecksumCalc.start();
      program.updateRawDexFileHeader(output);
      timerChecksumCalc.stop();
      output.close();
      success = true;
    } catch (FileNotFoundException e) {
      Log.errorAndQuit("Couldn't open a file called " + outputName);
    } catch (IOException e) {
      Log.errorAndQuit("IOException when trying to save a DEX test file!");
    }
    return success;
  }
}
