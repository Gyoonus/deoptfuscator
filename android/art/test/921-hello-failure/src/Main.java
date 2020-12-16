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

import art.Redefinition;
import java.util.Arrays;

public class Main {

  public static void main(String[] args) {
    Verification.doTest(new Transform());
    NewName.doTest(new Transform());
    DifferentAccess.doTest(new Transform());
    NewInterface.doTest(new Transform2());
    MissingInterface.doTest(new Transform2());
    ReorderInterface.doTest(new Transform2());
    MultiRedef.doTest(new Transform(), new Transform2());
    MultiRetrans.doTest(new Transform(), new Transform2());
    NewMethod.doTest(new Transform());
    MissingMethod.doTest(new Transform3());
    MethodChange.doTest(new Transform());
    NewField.doTest(new Transform());
    MissingField.doTest(new Transform4("there"));
    FieldChange.doTest(new Transform4("there again"));
    Unmodifiable.doTest(new Transform[] { new Transform(), });
    Undefault.doTest(new Transform5());
  }

  // TODO Replace this shim with a better re-write of this test.
  private static Redefinition.CommonClassDefinition mapCCD(CommonClassDefinition d) {
    return new Redefinition.CommonClassDefinition(d.target, d.class_file_bytes, d.dex_file_bytes);
  }

  private static Redefinition.CommonClassDefinition[] toCCDA(CommonClassDefinition[] ds) {
    return Arrays.stream(ds).map(Main::mapCCD).toArray(Redefinition.CommonClassDefinition[]::new);
  }

  public static void doCommonClassRedefinition(Class<?> target,
                                               byte[] classfile,
                                               byte[] dexfile) throws Exception {
    Redefinition.doCommonClassRedefinition(target, classfile, dexfile);
  }
  public static void doMultiClassRedefinition(CommonClassDefinition... defs) throws Exception {
    Redefinition.doMultiClassRedefinition(toCCDA(defs));
  }
  public static void addMultiTransformationResults(CommonClassDefinition... defs) throws Exception {
    Redefinition.addMultiTransformationResults(toCCDA(defs));
  }
  public static void doCommonMultiClassRedefinition(Class<?>[] targets,
                                                    byte[][] classfiles,
                                                    byte[][] dexfiles) throws Exception {
    Redefinition.doCommonMultiClassRedefinition(targets, classfiles, dexfiles);
  }
  public static void doCommonClassRetransformation(Class<?>... target) throws Exception {
    Redefinition.doCommonClassRetransformation(target);
  }
  public static void enableCommonRetransformation(boolean enable) {
    Redefinition.enableCommonRetransformation(enable);
  }
  public static void addCommonTransformationResult(String target_name,
                                                   byte[] class_bytes,
                                                   byte[] dex_bytes) {
    Redefinition.addCommonTransformationResult(target_name, class_bytes, dex_bytes);
  }
}
