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

import java.io.IOException;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import dexfuzz.ExecutionResult;
import dexfuzz.Log;
import dexfuzz.Options;
import dexfuzz.StreamConsumer;

/**
 * Handles execution either on a remote target device, or on a local host computer.
 */
public class Device {
  private boolean isHost;
  private String deviceName;
  private boolean usingSpecificDevice;
  private boolean noBootImage;

  private String androidHostOut;
  private String androidProductOut;
  private String androidData;

  private boolean programPushed;

  /**
   * The constructor for a host "device".
   */
  public Device() {
    this.isHost = true;
    this.deviceName = "[HostDevice]";
    setup();
  }

  /**
   * The constructor for an ADB connected device.
   */
  public Device(String deviceName, boolean noBootImage) {
    if (!deviceName.isEmpty()) {
      this.deviceName = deviceName;
      this.usingSpecificDevice = true;
    }
    this.noBootImage = noBootImage;
    setup();
  }

  private String checkForEnvVar(Map<String, String> envVars, String key) {
    if (!envVars.containsKey(key)) {
      Log.errorAndQuit("Cannot run a fuzzed program if $" + key + " is not set!");
    }
    return envVars.get(key);
  }

  private String getHostCoreImagePathWithArch() {
    // TODO: Using host currently implies x86 (see Options.java), change this when generalized.
    assert(Options.useArchX86);
    return androidHostOut + "/framework/x86/core.art";
  }

  private String getHostCoreImagePathNoArch() {
    return androidHostOut + "/framework/core.art";
  }

  private void setup() {
    programPushed = false;

    Map<String, String> envVars = System.getenv();
    androidProductOut = checkForEnvVar(envVars, "ANDROID_PRODUCT_OUT");
    androidHostOut = checkForEnvVar(envVars, "ANDROID_HOST_OUT");

    if (Options.executeOnHost) {
      File coreImage = new File(getHostCoreImagePathWithArch());
      if (!coreImage.exists()) {
        Log.errorAndQuit("Host core image not found at " + coreImage.getPath()
            + ". Did you forget to build it?");
      }
    }
    if (!isHost) {
      // Create temporary consumers for the initial test.
      StreamConsumer outputConsumer = new StreamConsumer();
      outputConsumer.start();
      StreamConsumer errorConsumer = new StreamConsumer();
      errorConsumer.start();

      // Check for ADB.
      try {
        ProcessBuilder pb = new ProcessBuilder();
        pb.command("adb", "devices");
        Process process = pb.start();
        int exitValue = process.waitFor();
        if (exitValue != 0) {
          Log.errorAndQuit("Problem executing ADB - is it in your $PATH?");
        }
      } catch (IOException e) {
        Log.errorAndQuit("IOException when executing ADB, is it working?");
      } catch (InterruptedException e) {
        Log.errorAndQuit("InterruptedException when executing ADB, is it working?");
      }

      // Check we can run something on ADB.
      ExecutionResult result = executeCommand("true", true, outputConsumer, errorConsumer);
      if (result.getFlattenedAll().contains("device not found")) {
        Log.errorAndQuit("Couldn't connect to specified ADB device: " + deviceName);
      }

      outputConsumer.shutdown();
      errorConsumer.shutdown();
    } else {
      androidData = checkForEnvVar(envVars, "ANDROID_DATA");
    }
  }

  /**
   * Get the name that would be provided to adb -s to communicate specifically with this device.
   */
  public String getName() {
    assert(!isHost);
    return deviceName;
  }

  public boolean isHost() {
    return isHost;
  }

  public boolean isUsingSpecificDevice() {
    return usingSpecificDevice;
  }

  /**
   * Certain AOSP builds of Android may not have a full boot.art built. This will be set if
   * we use --no-boot-image, and is used by Executors when deciding the arguments for dalvikvm
   * and dex2oat when performing host-side verification.
   */
  public boolean noBootImageAvailable() {
    return noBootImage;
  }

  /**
   * Get the command prefix for this device if we want to use adb shell.
   */
  public String getExecutionShellPrefix() {
    if (isHost) {
      return "";
    }
    return getExecutionPrefixWithAdb("shell");
  }

  /**
   * Get any extra flags required to execute ART on the host.
   */
  public String getHostExecutionFlags() {
    return String.format("-Xnorelocate -Ximage:%s", getHostCoreImagePathNoArch());
  }

  public String getAndroidHostOut() {
    return androidHostOut;
  }

  public String getAndroidProductOut() {
    return androidProductOut;
  }

  public ExecutionResult executeCommand(String command, boolean captureOutput) {
    assert(!captureOutput);
    return executeCommand(command, captureOutput, null, null);
  }

  public ExecutionResult executeCommand(String command, boolean captureOutput,
      StreamConsumer outputConsumer, StreamConsumer errorConsumer) {

    ExecutionResult result = new ExecutionResult();

    Log.info("Executing: " + command);

    try {
      ProcessBuilder processBuilder = new ProcessBuilder(splitCommand(command));
      processBuilder.environment().put("ANDROID_ROOT", androidHostOut);
      if (Options.executeOnHost) {
        processBuilder.environment().put("ANDROID_DATA", androidData);
      }
      Process process = processBuilder.start();

      if (captureOutput) {
        // Give the streams to the StreamConsumers.
        outputConsumer.giveStreamAndStartConsuming(process.getInputStream());
        errorConsumer.giveStreamAndStartConsuming(process.getErrorStream());
      }

      // Wait until the process is done - the StreamConsumers will keep the
      // buffers drained, so this shouldn't block indefinitely.
      // Get the return value as well.
      result.returnValue = process.waitFor();

      Log.info("Return value: " + result.returnValue);

      if (captureOutput) {
        // Tell the StreamConsumers to stop consuming, and wait for them to finish
        // so we know we have all of the output.
        outputConsumer.processFinished();
        errorConsumer.processFinished();
        result.output = outputConsumer.getOutput();
        result.error = errorConsumer.getOutput();

        // Always explicitly indicate the return code in the text output now.
        // NB: adb shell doesn't actually return exit codes currently, but this will
        // be useful if/when it does.
        result.output.add("RETURN CODE: " + result.returnValue);
      }

    } catch (IOException e) {
      Log.errorAndQuit("ExecutionResult.execute() caught an IOException");
    } catch (InterruptedException e) {
      Log.errorAndQuit("ExecutionResult.execute() caught an InterruptedException");
    }

    return result;
  }

  /**
   * Splits command respecting single quotes.
   */
  private List<String> splitCommand(String command) {
    List<String> ret = new ArrayList<String>();
    Matcher m = Pattern.compile("(\'[^\']+\'| *[^ ]+ *)").matcher(command);
    while (m.find())
      ret.add(m.group(1).trim().replace("\'", ""));
    return ret;
  }

  private String getExecutionPrefixWithAdb(String command) {
    if (usingSpecificDevice) {
      return String.format("adb -s %s %s ", deviceName, command);
    } else {
      return String.format("adb %s ", command);
    }
  }

  private String getCacheLocation(Architecture architecture) {
    String cacheLocation = "";
    if (isHost) {
      cacheLocation = androidData + "/dalvik-cache/" + architecture.asString() + "/";
    } else {
      cacheLocation = "/data/dalvik-cache/" + architecture.asString() + "/";
    }
    return cacheLocation;
  }

  private String getOatFileName(String testLocation, String programName) {
    // Converts e.g. /data/art-test/file.dex to data@art-test@file.dex
    return (testLocation.replace("/", "@").substring(1) + "@" + programName);
  }

  public void cleanCodeCache(Architecture architecture, String testLocation, String programName) {
    String command = getExecutionPrefixWithAdb("shell") + "rm -f " + getCacheLocation(architecture)
        + getOatFileName(testLocation, programName);
    executeCommand(command, false);
  }

  public void pushProgramToDevice(String programName, String testLocation) {
    assert(!isHost);
    if (!programPushed) {
      String command = getExecutionPrefixWithAdb("push") + programName + " " + testLocation;
      ExecutionResult result = executeCommand(command, false);
      if (result.returnValue != 0) {
        Log.errorAndQuit("Could not ADB PUSH program to device.");
      }
      programPushed = true;
    }
  }

  public void resetProgramPushed() {
    programPushed = false;
  }
}
