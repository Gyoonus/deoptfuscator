/*
 * Copyright (C) 2017 The Android Open Source Project
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

import dalvik.system.InMemoryDexClassLoader;
import dalvik.system.PathClassLoader;
import dalvik.system.VMRuntime;
import java.io.File;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    prepareNativeLibFileName(args[0]);

    // Enable hidden API checks in case they are disabled by default.
    init();

    // TODO there are sequential depencies between these test cases, and bugs
    // in the production code may lead to subsequent tests to erroneously pass,
    // or test the wrong thing. We rely on not deduping hidden API warnings
    // here for the same reasons), meaning the code under test and production
    // code are running in different configurations. Each test should be run in
    // a fresh process to ensure that they are working correcting and not
    // accidentally interfering with eachother.

    // Run test with both parent and child dex files loaded with class loaders.
    // The expectation is that hidden members in parent should be visible to
    // the child.
    doTest(false, false, false);
    doUnloading();

    // Now append parent dex file to boot class path and run again. This time
    // the child dex file should not be able to access private APIs of the
    // parent.
    appendToBootClassLoader(DEX_PARENT_BOOT);
    doTest(true, false, false);
    doUnloading();

    // Now run the same test again, but with the blacklist exmemptions list set
    // to "L" which matches everything.
    doTest(true, false, true);
    doUnloading();

    // And finally append to child to boot class path as well. With both in the
    // boot class path, access should be granted.
    appendToBootClassLoader(DEX_CHILD);
    doTest(true, true, false);
    doUnloading();
  }

  private static void doTest(boolean parentInBoot, boolean childInBoot, boolean whitelistAllApis)
      throws Exception {
    // Load parent dex if it is not in boot class path.
    ClassLoader parentLoader = null;
    if (parentInBoot) {
      parentLoader = BOOT_CLASS_LOADER;
    } else {
      parentLoader = new PathClassLoader(DEX_PARENT, ClassLoader.getSystemClassLoader());
    }

    // Load child dex if it is not in boot class path.
    ClassLoader childLoader = null;
    if (childInBoot) {
      if (parentLoader != BOOT_CLASS_LOADER) {
        throw new IllegalStateException(
            "DeclaringClass must be in parent class loader of CallingClass");
      }
      childLoader = BOOT_CLASS_LOADER;
    } else {
      childLoader = new InMemoryDexClassLoader(readDexFile(DEX_CHILD), parentLoader);
    }

    // Create a unique copy of the native library. Each shared library can only
    // be loaded once, but for some reason even classes from a class loader
    // cannot register their native methods against symbols in a shared library
    // loaded by their parent class loader.
    String nativeLibCopy = createNativeLibCopy(parentInBoot, childInBoot, whitelistAllApis);

    if (whitelistAllApis) {
      VMRuntime.getRuntime().setHiddenApiExemptions(new String[]{"L"});
    }

    // Invoke ChildClass.runTest
    Class.forName("ChildClass", true, childLoader)
        .getDeclaredMethod("runTest", String.class, Boolean.TYPE, Boolean.TYPE, Boolean.TYPE)
            .invoke(null, nativeLibCopy, parentInBoot, childInBoot, whitelistAllApis);

    VMRuntime.getRuntime().setHiddenApiExemptions(new String[0]);
  }

  // Routine which tries to figure out the absolute path of our native library.
  private static void prepareNativeLibFileName(String arg) throws Exception {
    String libName = System.mapLibraryName(arg);
    Method libPathsMethod = Runtime.class.getDeclaredMethod("getLibPaths");
    libPathsMethod.setAccessible(true);
    String[] libPaths = (String[]) libPathsMethod.invoke(Runtime.getRuntime());
    nativeLibFileName = null;
    for (String p : libPaths) {
      String candidate = p + libName;
      if (new File(candidate).exists()) {
        nativeLibFileName = candidate;
        break;
      }
    }
    if (nativeLibFileName == null) {
      throw new IllegalStateException("Didn't find " + libName + " in " +
          Arrays.toString(libPaths));
    }
  }

  // Helper to read dex file into memory.
  private static ByteBuffer readDexFile(String jarFileName) throws Exception {
    ZipFile zip = new ZipFile(new File(jarFileName));
    ZipEntry entry = zip.getEntry("classes.dex");
    InputStream is = zip.getInputStream(entry);
    int offset = 0;
    int size = (int) entry.getSize();
    ByteBuffer buffer = ByteBuffer.allocate(size);
    while (is.available() > 0) {
      is.read(buffer.array(), offset, size - offset);
    }
    is.close();
    zip.close();
    return buffer;
  }

  // Copy native library to a new file with a unique name so it does not
  // conflict with other loaded instance of the same binary file.
  private static String createNativeLibCopy(
      boolean parentInBoot, boolean childInBoot, boolean whitelistAllApis) throws Exception {
    String tempFileName = System.mapLibraryName(
        "hiddenapitest_" + (parentInBoot ? "1" : "0") + (childInBoot ? "1" : "0") +
         (whitelistAllApis ? "1" : "0"));
    File tempFile = new File(System.getenv("DEX_LOCATION"), tempFileName);
    Files.copy(new File(nativeLibFileName).toPath(), tempFile.toPath());
    return tempFile.getAbsolutePath();
  }

  private static void doUnloading() {
    // Do multiple GCs to prevent rare flakiness if some other thread is
    // keeping the classloader live.
    for (int i = 0; i < 5; ++i) {
       Runtime.getRuntime().gc();
    }
  }

  private static String nativeLibFileName;

  private static final String DEX_PARENT =
      new File(System.getenv("DEX_LOCATION"), "674-hiddenapi.jar").getAbsolutePath();
  private static final String DEX_PARENT_BOOT =
      new File(new File(System.getenv("DEX_LOCATION"), "res"), "boot.jar").getAbsolutePath();
  private static final String DEX_CHILD =
      new File(System.getenv("DEX_LOCATION"), "674-hiddenapi-ex.jar").getAbsolutePath();

  private static ClassLoader BOOT_CLASS_LOADER = Object.class.getClassLoader();

  private static native void appendToBootClassLoader(String dexPath);
  private static native void init();
}
