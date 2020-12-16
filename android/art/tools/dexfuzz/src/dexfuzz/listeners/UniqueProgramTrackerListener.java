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

import dexfuzz.Log;
import dexfuzz.Options;
import dexfuzz.executors.Executor;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Tracks unique programs and outputs. Also saves divergent programs!
 */
public class UniqueProgramTrackerListener extends BaseListener {
  /**
   * Map of unique program MD5 sums, mapped to times seen.
   */
  private Map<String, Integer> uniquePrograms;

  /**
   * Map of unique program outputs (MD5'd), mapped to times seen.
   */
  private Map<String, Integer> uniqueOutputs;

  /**
   * Used to remember the seed used to fuzz the fuzzed file, so we can save it with this
   * seed as a name, if we find a divergence.
   */
  private long currentSeed;

  /**
   * Used to remember the name of the file we've fuzzed, so we can save it if we
   * find a divergence.
   */
  private String fuzzedFile;

  private MessageDigest digest;
  private String databaseFile;

  /**
   * Save the database every X number of iterations.
   */
  private static final int saveDatabasePeriod = 20;

  public UniqueProgramTrackerListener(String databaseFile) {
    this.databaseFile = databaseFile;
  }

  @Override
  public void handleSeed(long seed) {
    currentSeed = seed;
  }

  /**
   * Given a program filename, calculate the MD5sum of
   * this program.
   */
  private String getMD5SumOfProgram(String programName) {
    byte[] buf = new byte[256];
    try {
      FileInputStream stream = new FileInputStream(programName);
      boolean done = false;
      while (!done) {
        int bytesRead = stream.read(buf);
        if (bytesRead == -1) {
          done = true;
        } else {
          digest.update(buf);
        }
      }
      stream.close();
    } catch (FileNotFoundException e) {
      e.printStackTrace();
    } catch (IOException e) {
      e.printStackTrace();
    }
    return new String(digest.digest());
  }

  private String getMD5SumOfOutput(String output) {
    digest.update(output.getBytes());
    return new String(digest.digest());
  }

  @SuppressWarnings("unchecked")
  private void loadUniqueProgsData() {
    File file = new File(databaseFile);
    if (!file.exists()) {
      uniquePrograms = new HashMap<String, Integer>();
      uniqueOutputs = new HashMap<String, Integer>();
      return;
    }

    try {
      ObjectInputStream objectStream =
          new ObjectInputStream(new FileInputStream(databaseFile));
      uniquePrograms = (Map<String, Integer>) objectStream.readObject();
      uniqueOutputs = (Map<String, Integer>) objectStream.readObject();
      objectStream.close();
    } catch (FileNotFoundException e) {
      e.printStackTrace();
    } catch (IOException e) {
      e.printStackTrace();
    } catch (ClassNotFoundException e) {
      e.printStackTrace();
    }

  }

  private void saveUniqueProgsData() {
    // Since we could potentially stop the program while writing out this DB,
    // copy the old file beforehand, and then delete it if we successfully wrote out the DB.
    boolean oldWasSaved = false;
    File file = new File(databaseFile);
    if (file.exists()) {
      try {
        Process process =
            Runtime.getRuntime().exec(String.format("cp %1$s %1$s.old", databaseFile));
        // Shouldn't block, cp shouldn't produce output.
        process.waitFor();
        oldWasSaved = true;
      } catch (IOException exception) {
        exception.printStackTrace();
      } catch (InterruptedException exception) {
        exception.printStackTrace();
      }
    }

    // Now write out the DB.
    boolean success = false;
    try {
      ObjectOutputStream objectStream =
          new ObjectOutputStream(new FileOutputStream(databaseFile));
      objectStream.writeObject(uniquePrograms);
      objectStream.writeObject(uniqueOutputs);
      objectStream.close();
      success = true;
    } catch (FileNotFoundException e) {
      e.printStackTrace();
    } catch (IOException e) {
      e.printStackTrace();
    }

    // If we get here, and we successfully wrote out the DB, delete the saved one.
    if (oldWasSaved && success) {
      try {
        Process process =
            Runtime.getRuntime().exec(String.format("rm %s.old", databaseFile));
        // Shouldn't block, rm shouldn't produce output.
        process.waitFor();
      } catch (IOException exception) {
        exception.printStackTrace();
      } catch (InterruptedException exception) {
        exception.printStackTrace();
      }
    } else if (oldWasSaved && !success) {
      Log.error("Failed to successfully write out the unique programs DB!");
      Log.error("Old DB should be saved in " + databaseFile + ".old");
    }
  }

  private void addToMap(String md5sum, Map<String, Integer> map) {
    if (map.containsKey(md5sum)) {
      map.put(md5sum, map.get(md5sum) + 1);
    } else {
      map.put(md5sum, 1);
    }
  }

  private void saveDivergentProgram() {
    File before = new File(fuzzedFile);
    File after = new File(String.format("divergent_programs/%d.dex", currentSeed));
    boolean success = before.renameTo(after);
    if (!success) {
      Log.error("Failed to save divergent program! Does divergent_programs/ exist?");
    }
  }

  @Override
  public void setup() {
    try {
      digest = MessageDigest.getInstance("MD5");
      loadUniqueProgsData();
    } catch (NoSuchAlgorithmException e) {
      e.printStackTrace();
    }
  }

  @Override
  public void handleIterationFinished(int iteration) {
    if ((iteration % saveDatabasePeriod) == (saveDatabasePeriod - 1)) {
      saveUniqueProgsData();
    }
  }

  @Override
  public void handleSuccessfullyFuzzedFile(String programName) {
    String md5sum = getMD5SumOfProgram(programName);
    addToMap(md5sum, uniquePrograms);

    fuzzedFile = programName;
  }

  @Override
  public void handleDivergences(Map<String, List<Executor>> outputMap) {
    // Just use the first one.
    String output = (String) outputMap.keySet().toArray()[0];
    String md5sum = getMD5SumOfOutput(output);
    addToMap(md5sum, uniqueOutputs);

    saveDivergentProgram();
  }

  @Override
  public void handleSuccess(Map<String, List<Executor>> outputMap) {
    // There's only one, use it.
    String output = (String) outputMap.keySet().toArray()[0];
    String md5sum = getMD5SumOfOutput(output);
    addToMap(md5sum, uniqueOutputs);
  }

  @Override
  public void handleSummary() {
    if (Options.reportUnique) {
      Log.always("-- UNIQUE PROGRAM REPORT --");
      Log.always("Unique Programs Seen: " + uniquePrograms.size());
      Log.always("Unique Outputs Seen: " + uniqueOutputs.size());
      Log.always("---------------------------");
    }

    saveUniqueProgsData();
  }

}
