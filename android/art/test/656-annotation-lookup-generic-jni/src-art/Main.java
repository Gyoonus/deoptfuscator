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

import java.io.InputStream;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class Main {

  public static void main(String[] args) throws Exception {
    // Extract Dex file contents from the secondary Jar file.
    String jarFilename =
        System.getenv("DEX_LOCATION") + "/656-annotation-lookup-generic-jni-ex.jar";
    ZipFile zipFile = new ZipFile(jarFilename);
    ZipEntry zipEntry = zipFile.getEntry("classes.dex");
    InputStream inputStream = zipFile.getInputStream(zipEntry);
    int dexFileSize = (int) zipEntry.getSize();
    byte[] dexFileContents = new byte[dexFileSize];
    inputStream.read(dexFileContents, 0, dexFileSize);

    // Create class loader from secondary Dex file.
    ByteBuffer dexBuffer = ByteBuffer.wrap(dexFileContents);
    ClassLoader classLoader = createUnquickenedDexClassLoader(dexBuffer);

    // Load and initialize the Test class.
    Class<?> testClass = classLoader.loadClass("Test");
    Method initialize = testClass.getMethod("initialize", String.class);
    initialize.invoke(null, args[0]);

    // Invoke Test.nativeMethodWithAnnotation().
    Method nativeMethodWithAnnotation = testClass.getMethod("nativeMethodWithAnnotation");
    // Invoking the native method Test.nativeMethodWithAnnotation used
    // to crash the Generic JNI trampoline during the resolution of
    // the method's annotations (DummyAnnotation) (see b/38454151).
    nativeMethodWithAnnotation.invoke(null);

    zipFile.close();
    System.out.println("passed");
  }

  // Create a class loader loading a Dex file in memory
  // *without creating an Oat file*. This way, the Dex file won't be
  // quickened and JNI stubs won't be compiled, thus forcing the use
  // of Generic JNI when invoking the native method
  // Test.nativeMethodWithAnnotation.
  static ClassLoader createUnquickenedDexClassLoader(ByteBuffer dexBuffer) {
    InMemoryDexClassLoader cl = new InMemoryDexClassLoader(dexBuffer, getBootClassLoader());
    return cl;
  }

  static ClassLoader getBootClassLoader() {
    ClassLoader cl = Main.class.getClassLoader();
    while (cl.getParent() != null) {
      cl = cl.getParent();
    }
    return cl;
  }

}
