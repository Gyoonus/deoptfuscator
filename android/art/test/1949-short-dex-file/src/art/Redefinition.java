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

package art;

import java.util.ArrayList;
// Common Redefinition functions. Placed here for use by CTS
public class Redefinition {
  public static final class CommonClassDefinition {
    public final Class<?> target;
    public final byte[] class_file_bytes;
    public final byte[] dex_file_bytes;

    public CommonClassDefinition(Class<?> target, byte[] class_file_bytes, byte[] dex_file_bytes) {
      this.target = target;
      this.class_file_bytes = class_file_bytes;
      this.dex_file_bytes = dex_file_bytes;
    }
  }

  // A set of possible test configurations. Test should set this if they need to.
  // This must be kept in sync with the defines in ti-agent/common_helper.cc
  public static enum Config {
    COMMON_REDEFINE(0),
    COMMON_RETRANSFORM(1),
    COMMON_TRANSFORM(2);

    private final int val;
    private Config(int val) {
      this.val = val;
    }
  }

  public static void setTestConfiguration(Config type) {
    nativeSetTestConfiguration(type.val);
  }

  private static native void nativeSetTestConfiguration(int type);

  // Transforms the class
  public static native void doCommonClassRedefinition(Class<?> target,
                                                      byte[] classfile,
                                                      byte[] dexfile);

  public static void doMultiClassRedefinition(CommonClassDefinition... defs) {
    ArrayList<Class<?>> classes = new ArrayList<>();
    ArrayList<byte[]> class_files = new ArrayList<>();
    ArrayList<byte[]> dex_files = new ArrayList<>();

    for (CommonClassDefinition d : defs) {
      classes.add(d.target);
      class_files.add(d.class_file_bytes);
      dex_files.add(d.dex_file_bytes);
    }
    doCommonMultiClassRedefinition(classes.toArray(new Class<?>[0]),
                                   class_files.toArray(new byte[0][]),
                                   dex_files.toArray(new byte[0][]));
  }

  public static void addMultiTransformationResults(CommonClassDefinition... defs) {
    for (CommonClassDefinition d : defs) {
      addCommonTransformationResult(d.target.getCanonicalName(),
                                    d.class_file_bytes,
                                    d.dex_file_bytes);
    }
  }

  public static native void doCommonMultiClassRedefinition(Class<?>[] targets,
                                                           byte[][] classfiles,
                                                           byte[][] dexfiles);
  public static native void doCommonClassRetransformation(Class<?>... target);
  public static native void setPopRetransformations(boolean pop);
  public static native void popTransformationFor(String name);
  public static native void enableCommonRetransformation(boolean enable);
  public static native void addCommonTransformationResult(String target_name,
                                                          byte[] class_bytes,
                                                          byte[] dex_bytes);
}
