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

import dexfuzz.Log.LogTag;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Stores options for dexfuzz.
 */
public class Options {
  /**
   * Constructor has been disabled for this class, which should only be used statically.
   */
  private Options() { }

  // KEY VALUE OPTIONS
  public static final List<String> inputFileList = new ArrayList<String>();
  public static String outputFile = "";
  public static long rngSeed = -1;
  public static boolean usingProvidedSeed = false;
  public static int methodMutations = 3;
  public static int minMethods = 2;
  public static int maxMethods = 10;
  public static final Map<String,Integer> mutationLikelihoods = new HashMap<String,Integer>();
  public static String executeClass = "Main";
  public static String deviceName = "";
  public static boolean usingSpecificDevice = false;
  public static int repeat = 1;
  public static int divergenceRetry = 10;
  public static String executeDirectory = "/data/art-test";
  public static String androidRoot = "";
  public static String dumpMutationsFile = "mutations.dump";
  public static String loadMutationsFile = "mutations.dump";
  public static String reportLogFile = "report.log";
  public static String uniqueDatabaseFile = "unique_progs.db";

  // FLAG OPTIONS
  public static boolean execute;
  public static boolean executeOnHost;
  public static boolean noBootImage;
  public static boolean useInterpreter;
  public static boolean useOptimizing;
  public static boolean useArchArm;
  public static boolean useArchArm64;
  public static boolean useArchX86;
  public static boolean useArchX86_64;
  public static boolean useArchMips;
  public static boolean useArchMips64;
  public static boolean skipHostVerify;
  public static boolean shortTimeouts;
  public static boolean dumpOutput;
  public static boolean dumpVerify;
  public static boolean mutateLimit;
  public static boolean reportUnique;
  public static boolean skipMutation;
  public static boolean dumpMutations;
  public static boolean loadMutations;
  public static boolean runBisectionSearch;
  public static boolean quiet;

  /**
   * Print out usage information about dexfuzz, and then exit.
   */
  public static void usage() {
    Log.always("DexFuzz Usage:");
    Log.always("  --input=<file>         : Seed DEX file to be fuzzed");
    Log.always("                           (Can specify multiple times.)");
    Log.always("  --inputs=<file>        : Directory containing DEX files to be fuzzed.");
    Log.always("  --output=<file>        : Output DEX file to be produced");
    Log.always("");
    Log.always("  --execute              : Execute the resulting fuzzed program");
    Log.always("    --host               : Execute on host");
    Log.always("    --device=<device>    : Execute on an ADB-connected-device, where <device> is");
    Log.always("                           the argument given to adb -s. Default execution mode.");
    Log.always("    --execute-dir=<dir>  : Push tests to this directory to execute them.");
    Log.always("                           (Default: /data/art-test)");
    Log.always("    --android-root=<dir> : Set path where dalvikvm should look for binaries.");
    Log.always("                           Use this when pushing binaries to a custom location.");
    Log.always("    --no-boot-image      : Use this flag when boot.art is not available.");
    Log.always("    --skip-host-verify   : When executing, skip host-verification stage");
    Log.always("    --execute-class=<c>  : When executing, execute this class (default: Main)");
    Log.always("");
    Log.always("    --interpreter        : Include the Interpreter in comparisons");
    Log.always("    --optimizing         : Include the Optimizing Compiler in comparisons");
    Log.always("");
    Log.always("    --arm                : Include ARM backends in comparisons");
    Log.always("    --arm64              : Include ARM64 backends in comparisons");
    Log.always("    --allarm             : Short for --arm --arm64");
    Log.always("    --x86                : Include x86 backends in comparisons");
    Log.always("    --x86-64             : Include x86-64 backends in comparisons");
    Log.always("    --mips               : Include MIPS backends in comparisons");
    Log.always("    --mips64             : Include MIPS64 backends in comparisons");
    Log.always("");
    Log.always("    --dump-output        : Dump outputs of executed programs");
    Log.always("    --dump-verify        : Dump outputs of verification");
    Log.always("    --repeat=<n>         : Fuzz N programs, executing each one.");
    Log.always("    --short-timeouts     : Shorten timeouts (faster; use if");
    Log.always("                           you want to focus on output divergences)");
    Log.always("    --divergence-retry=<n> : Number of retries when checking if test is");
    Log.always("                           self-divergent. (Default: 10)");
    Log.always("  --seed=<seed>          : RNG seed to use");
    Log.always("  --method-mutations=<n> : Maximum number of mutations to perform on each method.");
    Log.always("                           (Default: 3)");
    Log.always("  --min-methods=<n>      : Minimum number of methods to mutate. (Default: 2)");
    Log.always("  --max-methods=<n>      : Maximum number of methods to mutate. (Default: 10)");
    Log.always("  --one-mutation         : Short for --method-mutations=1 ");
    Log.always("                             --min-methods=1 --max-methods=1");
    Log.always("  --likelihoods=<file>   : A file containing a table of mutation likelihoods");
    Log.always("  --mutate-limit         : Mutate only methods whose names end with _MUTATE");
    Log.always("  --skip-mutation        : Do not actually mutate the input, just output it");
    Log.always("                           after parsing");
    Log.always("");
    Log.always("  --dump-mutations[=<file>] : Dump an editable set of mutations applied");
    Log.always("                              to <file> (default: mutations.dump)");
    Log.always("  --load-mutations[=<file>] : Load and apply a set of mutations");
    Log.always("                              from <file> (default: mutations.dump)");
    Log.always("  --log=<tag>            : Set more verbose logging level: DEBUG, INFO, WARN");
    Log.always("  --report=<file>        : Use <file> to report results when using --repeat");
    Log.always("                           (Default: report.log)");
    Log.always("  --report-unique        : Print out information about unique programs generated");
    Log.always("  --unique-db=<file>     : Use <file> store results about unique programs");
    Log.always("                           (Default: unique_progs.db)");
    Log.always("  --bisection-search     : Run bisection search for divergences");
    Log.always("  --quiet                : Disables progress log");
    Log.always("");
    System.exit(0);
  }

  /**
   * Given a flag option (one that does not feature an =), handle it
   * accordingly. Report an error and print usage info if the flag is not
   * recognised.
   */
  private static void handleFlagOption(String flag) {
    if (flag.equals("execute")) {
      execute = true;
    } else if (flag.equals("host")) {
      executeOnHost = true;
    } else if (flag.equals("no-boot-image")) {
      noBootImage = true;
    } else if (flag.equals("skip-host-verify")) {
      skipHostVerify = true;
    } else if (flag.equals("interpreter")) {
      useInterpreter = true;
    } else if (flag.equals("optimizing")) {
      useOptimizing = true;
    } else if (flag.equals("arm")) {
      useArchArm = true;
    } else if (flag.equals("arm64")) {
      useArchArm64 = true;
    } else if (flag.equals("allarm")) {
      useArchArm = true;
      useArchArm64 = true;
    } else if (flag.equals("x86")) {
      useArchX86 = true;
    } else if (flag.equals("x86-64")) {
      useArchX86_64 = true;
    } else if (flag.equals("mips")) {
      useArchMips = true;
    } else if (flag.equals("mips64")) {
      useArchMips64 = true;
    } else if (flag.equals("mutate-limit")) {
      mutateLimit = true;
    } else if (flag.equals("report-unique")) {
      reportUnique = true;
    } else if (flag.equals("dump-output")) {
      dumpOutput = true;
    } else if (flag.equals("dump-verify")) {
      dumpVerify = true;
    } else if (flag.equals("short-timeouts")) {
      shortTimeouts = true;
    } else if (flag.equals("skip-mutation")) {
      skipMutation = true;
    } else if (flag.equals("dump-mutations")) {
      dumpMutations = true;
    } else if (flag.equals("load-mutations")) {
      loadMutations = true;
    } else if (flag.equals("one-mutation")) {
      methodMutations = 1;
      minMethods = 1;
      maxMethods = 1;
    } else if (flag.equals("bisection-search")) {
      runBisectionSearch = true;
    } else if (flag.equals("quiet")) {
      quiet = true;
    } else if (flag.equals("help")) {
      usage();
    } else {
      Log.error("Unrecognised flag: --" + flag);
      usage();
    }
  }

  /**
   * Given a key-value option (one that features an =), handle it
   * accordingly. Report an error and print usage info if the key is not
   * recognised.
   */
  private static void handleKeyValueOption(String key, String value) {
    if (key.equals("input")) {
      inputFileList.add(value);
    } else if (key.equals("inputs")) {
      File folder = new File(value);
      if (folder.listFiles() == null) {
        Log.errorAndQuit("Specified argument to --inputs is not a directory!");
      }
      for (File file : folder.listFiles()) {
        String inputName = value + "/" + file.getName();
        Log.always("Adding " + inputName + " to input seed files.");
        inputFileList.add(inputName);
      }
    } else if (key.equals("output")) {
      outputFile = value;
    } else if (key.equals("seed")) {
      rngSeed = Long.parseLong(value);
      usingProvidedSeed = true;
    } else if (key.equals("method-mutations")) {
      methodMutations = Integer.parseInt(value);
    } else if (key.equals("min-methods")) {
      minMethods = Integer.parseInt(value);
    } else if (key.equals("max-methods")) {
      maxMethods = Integer.parseInt(value);
    } else if (key.equals("repeat")) {
      repeat = Integer.parseInt(value);
    } else if (key.equals("divergence-retry")) {
      divergenceRetry = Integer.parseInt(value);
    } else if (key.equals("log")) {
      Log.setLoggingLevel(LogTag.valueOf(value.toUpperCase()));
    } else if (key.equals("likelihoods")) {
      setupMutationLikelihoodTable(value);
    } else if (key.equals("dump-mutations")) {
      dumpMutations = true;
      dumpMutationsFile = value;
    } else if (key.equals("load-mutations")) {
      loadMutations = true;
      loadMutationsFile = value;
    } else if (key.equals("report")) {
      reportLogFile = value;
    } else if (key.equals("unique-db")) {
      uniqueDatabaseFile = value;
    } else if (key.equals("execute-class")) {
      executeClass = value;
    } else if (key.equals("device")) {
      deviceName = value;
      usingSpecificDevice = true;
    } else if (key.equals("execute-dir")) {
      executeDirectory = value;
    } else if (key.equals("android-root")) {
      androidRoot = value;
    } else {
      Log.error("Unrecognised key: --" + key);
      usage();
    }
  }

  private static void setupMutationLikelihoodTable(String tableFilename) {
    try {
      BufferedReader reader = new BufferedReader(new FileReader(tableFilename));
      String line = reader.readLine();
      while (line != null) {
        line = line.replaceAll("\\s+", " ");
        String[] entries = line.split(" ");
        String name = entries[0].toLowerCase();
        int likelihood = Integer.parseInt(entries[1]);
        if (likelihood > 100) {
          likelihood = 100;
        }
        if (likelihood < 0) {
          likelihood = 0;
        }
        mutationLikelihoods.put(name, likelihood);
        line = reader.readLine();
      }
      reader.close();
    } catch (FileNotFoundException e) {
      Log.error("Unable to open mutation probability table file: " + tableFilename);
    } catch (IOException e) {
      Log.error("Unable to read mutation probability table file: " + tableFilename);
    }
  }

  /**
   * Called by the DexFuzz class during program initialisation to parse
   * the program's command line arguments.
   * @return If options were successfully read and validated.
   */
  public static boolean readOptions(String[] args) {
    for (String arg : args) {
      if (!(arg.startsWith("--"))) {
        Log.error("Unrecognised option: " + arg);
        usage();
      }

      // cut off the --
      arg = arg.substring(2);

      // choose between a --X=Y option (keyvalue) and a --X option (flag)
      if (arg.contains("=")) {
        String[] split = arg.split("=");
        handleKeyValueOption(split[0], split[1]);
      } else {
        handleFlagOption(arg);
      }
    }

    return validateOptions();
  }

  /**
   * Checks if the current options settings are valid, called after reading
   * all options.
   * @return If the options are valid or not.
   */
  private static boolean validateOptions() {
    // Deal with option assumptions.
    if (inputFileList.isEmpty()) {
      File seedFile = new File("fuzzingseed.dex");
      if (seedFile.exists()) {
        Log.always("Assuming --input=fuzzingseed.dex");
        inputFileList.add("fuzzingseed.dex");
      } else {
        Log.errorAndQuit("No input given, and couldn't find fuzzingseed.dex!");
        return false;
      }
    }

    if (outputFile.equals("")) {
      Log.always("Assuming --output=fuzzingseed_fuzzed.dex");
      outputFile = "fuzzingseed_fuzzed.dex";
    }


    if (mutationLikelihoods.isEmpty()) {
      File likelihoodsFile = new File("likelihoods.txt");
      if (likelihoodsFile.exists()) {
        Log.always("Assuming --likelihoods=likelihoods.txt ");
        setupMutationLikelihoodTable("likelihoods.txt");
      } else {
        Log.always("Using default likelihoods (see README for values)");
      }
    }

    // Now check for hard failures.
    if (repeat < 1) {
      Log.error("--repeat must be at least 1!");
      return false;
    }
    if (divergenceRetry < 0) {
      Log.error("--divergence-retry cannot be negative!");
      return false;
    }
    if (usingProvidedSeed && repeat > 1) {
      Log.error("Cannot use --repeat with --seed");
      return false;
    }
    if (loadMutations && dumpMutations) {
      Log.error("Cannot both load and dump mutations");
      return false;
    }
    if (repeat == 1 && inputFileList.size() > 1) {
      Log.error("Must use --repeat if you have provided more than one input");
      return false;
    }
    if (methodMutations < 0) {
      Log.error("Cannot use --method-mutations with a negative value.");
      return false;
    }
    if (minMethods < 0) {
      Log.error("Cannot use --min-methods with a negative value.");
      return false;
    }
    if (maxMethods < 0) {
      Log.error("Cannot use --max-methods with a negative value.");
      return false;
    }
    if (maxMethods < minMethods) {
      Log.error("Cannot use --max-methods that's smaller than --min-methods");
      return false;
    }
    if (executeOnHost && usingSpecificDevice) {
      Log.error("Cannot use --host and --device!");
      return false;
    }
    if (execute) {
      // When host-execution mode is specified, we don't need to select an architecture.
      if (!executeOnHost) {
        if (!(useArchArm
            || useArchArm64
            || useArchX86
            || useArchX86_64
            || useArchMips
            || useArchMips64)) {
          Log.error("No architecture to execute on was specified!");
          return false;
        }
      } else {
        // TODO: Select the correct architecture. For now, just assume x86.
        useArchX86 = true;
      }
      if ((useArchArm || useArchArm64) && (useArchX86 || useArchX86_64)) {
        Log.error("Did you mean to specify ARM and x86?");
        return false;
      }
      if ((useArchArm || useArchArm64) && (useArchMips || useArchMips64)) {
        Log.error("Did you mean to specify ARM and MIPS?");
        return false;
      }
      if ((useArchX86 || useArchX86_64) && (useArchMips || useArchMips64)) {
        Log.error("Did you mean to specify x86 and MIPS?");
        return false;
      }
      int backends = 0;
      if (useInterpreter) {
        backends++;
      }
      if (useOptimizing) {
        backends++;
      }
      if (useArchArm && useArchArm64) {
        // Could just be comparing optimizing-ARM versus optimizing-ARM64?
        backends++;
      }
      if (backends < 2) {
        Log.error("Not enough backends specified! Try --optimizing --interpreter!");
        return false;
      }
    }

    return true;
  }
}
