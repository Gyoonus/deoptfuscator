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

import java.util.List;

/**
 * Stores the output of an executed command.
 */
public class ExecutionResult {
  public List<String> output;
  public List<String> error;
  public int returnValue;

  private String flattenedOutput;
  private String flattenedOutputWithNewlines;
  private String flattenedError;
  private String flattenedErrorWithNewlines;
  private String flattenedAll;
  private String flattenedAllWithNewlines;

  private static final int TIMEOUT_RETURN_VALUE = 124;
  private static final int SIGABORT_RETURN_VALUE = 134;

  /**
   * Get only the output, with all lines concatenated together, excluding newline characters.
   */
  public String getFlattenedOutput() {
    if (flattenedOutput == null) {
      StringBuilder builder = new StringBuilder();
      for (String line : output) {
        builder.append(line);
      }
      flattenedOutput = builder.toString();
    }
    return flattenedOutput;
  }

  /**
   * Get only the output, with all lines concatenated together, including newline characters.
   */
  public String getFlattenedOutputWithNewlines() {
    if (flattenedOutputWithNewlines == null) {
      StringBuilder builder = new StringBuilder();
      for (String line : output) {
        builder.append(line).append("\n");
      }
      flattenedOutputWithNewlines = builder.toString();
    }
    return flattenedOutputWithNewlines;
  }

  /**
   * Get only the error, with all lines concatenated together, excluding newline characters.
   */
  public String getFlattenedError() {
    if (flattenedError == null) {
      StringBuilder builder = new StringBuilder();
      for (String line : error) {
        builder.append(line);
      }
      flattenedError = builder.toString();
    }
    return flattenedError;
  }

  /**
   * Get only the error, with all lines concatenated together, including newline characters.
   */
  public String getFlattenedErrorWithNewlines() {
    if (flattenedErrorWithNewlines == null) {
      StringBuilder builder = new StringBuilder();
      for (String line : error) {
        builder.append(line).append("\n");
      }
      flattenedErrorWithNewlines = builder.toString();
    }
    return flattenedErrorWithNewlines;
  }

  /**
   * Get both the output and error, concatenated together, excluding newline characters.
   */
  public String getFlattenedAll() {
    if (flattenedAll == null) {
      flattenedAll = getFlattenedOutput() + getFlattenedError();
    }
    return flattenedAll;
  }

  /**
   * Get both the output and error, concatenated together, including newline characters.
   */
  public String getFlattenedAllWithNewlines() {
    if (flattenedAllWithNewlines == null) {
      flattenedAllWithNewlines = getFlattenedOutputWithNewlines() + getFlattenedErrorWithNewlines();
    }
    return flattenedAllWithNewlines;
  }

  public boolean isTimeout() {
    return (returnValue == TIMEOUT_RETURN_VALUE);
  }

  public boolean isSigabort() {
    return (returnValue == SIGABORT_RETURN_VALUE);
  }
}