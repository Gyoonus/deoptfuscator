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

package dexfuzz.executors;

import dexfuzz.ExecutionResult;
import dexfuzz.Options;
import dexfuzz.StreamConsumer;
import dexfuzz.listeners.BaseListener;

/**
 * Base class containing the common methods for executing a particular backend of ART.
 */
public abstract class Executor {
  private StreamConsumer outputConsumer;
  private StreamConsumer errorConsumer;

  protected ExecutionResult executionResult;
  protected String executeClass;

  // Set by subclasses.
  protected String name;
  protected int timeout;
  protected BaseListener listener;
  protected String testLocation;
  protected Architecture architecture;
  protected Device device;
  private boolean needsCleanCodeCache;
  private boolean isBisectable;

  protected Executor(String name, int timeout, BaseListener listener, Architecture architecture,
      Device device, boolean needsCleanCodeCache, boolean isBisectable) {
    executeClass = Options.executeClass;

    if (Options.shortTimeouts) {
      this.timeout = 2;
    } else {
      this.timeout = timeout;
    }

    this.name = name;
    this.listener = listener;
    this.architecture = architecture;
    this.device = device;
    this.needsCleanCodeCache = needsCleanCodeCache;
    this.isBisectable = isBisectable;

    if (Options.executeOnHost) {
      this.testLocation = System.getProperty("user.dir");
    } else {
      this.testLocation = Options.executeDirectory;
    }

    outputConsumer = new StreamConsumer();
    outputConsumer.start();
    errorConsumer = new StreamConsumer();
    errorConsumer.start();
  }

  /**
   * Called by subclass Executors in their execute() implementations.
   */
  protected ExecutionResult executeCommandWithTimeout(String command, boolean captureOutput) {
    String timeoutString = "timeout " + timeout + " ";
    return device.executeCommand(timeoutString + device.getExecutionShellPrefix() + command,
        captureOutput, outputConsumer, errorConsumer);
  }

  /**
   * Call this to make sure the StreamConsumer threads are stopped.
   */
  public void shutdown() {
    outputConsumer.shutdown();
    errorConsumer.shutdown();
  }

  /**
   * Called by the Fuzzer after each execution has finished, to clear the results.
   */
  public void reset() {
    executionResult = null;
  }

  /**
   * Called by the Fuzzer to verify the mutated program using the host-side dex2oat.
   */
  public boolean verifyOnHost(String programName) {
    StringBuilder commandBuilder = new StringBuilder();
    commandBuilder.append("dex2oat ");

    commandBuilder.append("--instruction-set=").append(architecture.asString());
    commandBuilder.append(" --instruction-set-features=default ");

    // Select the correct boot image.
    commandBuilder.append("--boot-image=").append(device.getAndroidProductOut());
    if (device.noBootImageAvailable()) {
      commandBuilder.append("/data/art-test/core.art ");
    } else {
      commandBuilder.append("/system/framework/boot.art ");
    }

    commandBuilder.append("--oat-file=output.oat ");
    commandBuilder.append("--android-root=").append(device.getAndroidHostOut()).append(" ");
    commandBuilder.append("--dex-file=").append(programName).append(" ");
    commandBuilder.append("--compiler-filter=quicken --runtime-arg -Xnorelocate ");

    ExecutionResult verificationResult = device.executeCommand(commandBuilder.toString(), true,
        outputConsumer, errorConsumer);

    boolean success = true;

    if (verificationResult.isSigabort()) {
      listener.handleHostVerificationSigabort(verificationResult);
      success = false;
    }

    if (success) {
      // Search for a keyword that indicates verification was not successful.
      // TODO: Determine if dex2oat crashed?
      for (String line : verificationResult.error) {
        if (line.contains("Verification error")
            || line.contains("Failure to verify dex file")) {
          success = false;
        }
        if (Options.dumpVerify) {
          // Strip out the start of the log lines.
          listener.handleDumpVerify(line.replaceFirst(".*(cc|h):\\d+] ",  ""));
        }
      }
    }

    if (!success) {
      listener.handleFailedHostVerification(verificationResult);
    }

    device.executeCommand("rm output.oat", false);

    return success;
  }

  /**
   * Called by the Fuzzer to upload the program to the target device.
   */
  public void prepareProgramForExecution(String programName) {
    if (!Options.executeOnHost) {
      device.pushProgramToDevice(programName, testLocation);
    }

    if (needsCleanCodeCache) {
      // Get the device to clean the code cache
      device.cleanCodeCache(architecture, testLocation, programName);
    }
  }

  /**
   * Executor subclasses need to override this, to construct their arguments for dalvikvm
   * invocation correctly.
   */
  protected abstract String constructCommand(String programName);

  /**
   * Executes runtime.
   */
  public void execute(String programName) {
    String command = "";
    String androidRoot = Options.androidRoot.trim();
    if (androidRoot.length() != 0) {
      command = "PATH=" + androidRoot + "/bin ";
      command += "ANDROID_ROOT=" + androidRoot + " ";
      command += "LD_LIBRARY_PATH="+ androidRoot + "/lib:" + androidRoot + "/lib64 ";
    }
    command += constructCommand(programName);
    executionResult = executeCommandWithTimeout(command, true);
  }

  /**
   * Runs bisection bug search.
   */
  public ExecutionResult runBisectionSearch(String programName, String expectedOutputFile, String logFile) {
    assert(isBisectable);
    String runtimeCommand = constructCommand(programName);
    StringBuilder commandBuilder = new StringBuilder();
    commandBuilder.append("bisection_search.py --raw-cmd '").append(runtimeCommand);
    commandBuilder.append("' --expected-output=").append(expectedOutputFile);
    commandBuilder.append(" --logfile=").append(logFile);
    if (!device.isHost()) {
      commandBuilder.append(" --device");
      if (device.isUsingSpecificDevice()) {
        commandBuilder.append(" --specific-device=").append(device.getName());
      }
    }
    return device.executeCommand(commandBuilder.toString(), true, outputConsumer, errorConsumer);
  }

  /**
   * Fuzzer.checkForArchitectureSplit() will use this determine the architecture of the Executor.
   */
  public Architecture getArchitecture() {
    return architecture;
  }

  /**
   * Used by the Fuzzer to get result of execution.
   */
  public ExecutionResult getResult() {
    return executionResult;
  }

  /**
   * Because dex2oat can accept a program with soft errors on the host, and then fail after
   * performing hard verification on the target, we need to check if the Executor detected
   * a target verification failure, before doing anything else with the resulting output.
   * Used by the Fuzzer.
   */
  public boolean didTargetVerify() {
    // TODO: Remove this once host-verification can be forced to always fail?
    String output = executionResult.getFlattenedAll();
    if (output.contains("VerifyError") || output.contains("Verification failed on class")) {
      return false;
    }
    return true;
  }

  public String getName() {
    return name;
  }

  public void finishedWithProgramOnDevice() {
    device.resetProgramPushed();
  }

  public boolean isBisectable() {
    return isBisectable;
  }
}
